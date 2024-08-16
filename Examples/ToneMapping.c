/* Special thanks to Matt Taylor for this overview of tonemapping: https://64.github.io/tonemapping/ */

#include "Common.h"

static SDL_GpuTexture* HDRTexture;
static SDL_GpuTexture* ToneMapTexture;
static SDL_GpuTexture* TransferTexture;

static SDL_GpuSwapchainComposition swapchainCompositions[] =
{
	SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
	SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR,
	SDL_GPU_SWAPCHAINCOMPOSITION_HDR_EXTENDED_LINEAR,
	SDL_GPU_SWAPCHAINCOMPOSITION_HDR10_ST2048
};
static const char* swapchainCompositionNames[] =
{
	"SDL_GPU_SWAPCHAINCOMPOSITION_SDR",
	"SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR",
	"SDL_GPU_SWAPCHAINCOMPOSITION_HDR_EXTENDED_LINEAR",
	"SDL_GPU_SWAPCHAINCOMPOSITION_HDR10_ST2048"
};
static Sint32 swapchainCompositionCount = sizeof(swapchainCompositions)/sizeof(SDL_GpuSwapchainComposition);
static Sint32 swapchainCompositionSelectionIndex = 0;
static SDL_GpuSwapchainComposition currentSwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;

static const char* tonemapOperatorNames[] =
{
	"Reinhard",
	"ExtendedReinhardLuminance",
	"Hable",
	"ACES"
};
static Sint32 tonemapOperatorCount = sizeof(tonemapOperatorNames)/sizeof(char*);
static SDL_GpuComputePipeline* tonemapOperators[sizeof(tonemapOperatorNames)/sizeof(char*)];
static Sint32 tonemapOperatorSelectionIndex = 0;
static SDL_GpuComputePipeline* currentTonemapOperator;

static SDL_GpuComputePipeline* LinearToSRGBPipeline;
static SDL_GpuComputePipeline* LinearToST2084Pipeline;

static int w, h;

static void ChangeSwapchainComposition(Context* context, Uint32 selectionIndex)
{
	if (SDL_GpuSupportsSwapchainComposition(context->Device, context->Window, swapchainCompositions[selectionIndex]))
	{
		currentSwapchainComposition = swapchainCompositions[selectionIndex];
		SDL_Log("Changing swapchain composition to %s", swapchainCompositionNames[selectionIndex]);
		SDL_GpuSetSwapchainParameters(context->Device, context->Window, currentSwapchainComposition, SDL_GPU_PRESENTMODE_VSYNC);
	}
	else
	{
		SDL_Log("Swapchain composition %s unsupported", swapchainCompositionNames[selectionIndex]);
	}
}

static void ChangeTonemapOperator(Context* context, Uint32 selectionIndex)
{
	SDL_Log("Changing tonemap operator to %s", tonemapOperatorNames[selectionIndex]);
	currentTonemapOperator = tonemapOperators[selectionIndex];
}

static SDL_GpuComputePipeline* BuildPostProcessComputePipeline(SDL_GpuDevice *device, const char* spvFile)
{
	return CreateComputePipelineFromShader(
		device,
		spvFile,
		&(SDL_GpuComputePipelineCreateInfo){
			.readOnlyStorageTextureCount = 1,
			.readWriteStorageTextureCount = 1,
			.threadCountX = 8,
			.threadCountY = 8,
			.threadCountZ = 1,
		}
	);
}

static int Init(Context* context)
{
    /* Manually set up example for HDR rendering */
    context->Device = SDL_GpuCreateDevice(SDL_TRUE, SDL_FALSE, NULL);
	if (context->Device == NULL)
	{
		SDL_Log("GpuCreateDevice failed");
		return -1;
	}

    int img_x, img_y, n;
    float *hdrImageData = LoadHDRImage("memorial.hdr", &img_x, &img_y, &n, 4);

    if (hdrImageData == NULL)
    {
        SDL_Log("Could not load HDR image data!");
        return -1;
    }

    context->Window = SDL_CreateWindow(context->ExampleName, img_x, img_y, 0);
	if (context->Window == NULL)
	{
		SDL_Log("CreateWindow failed: %s", SDL_GetError());
		return -1;
	}

	if (!SDL_GpuClaimWindow(context->Device, context->Window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC))
	{
		SDL_Log("GpuClaimWindow failed");
		return -1;
	}

    SDL_GetWindowSizeInPixels(context->Window, &w, &h);

	SDL_GpuShader* vertexShader = LoadShader(context->Device, "PositionColorTransform.vert", 0, 0, 0, 0);
	if (vertexShader == NULL)
	{
		SDL_Log("Failed to create vertex shader!");
		return -1;
	}

	SDL_GpuShader* fragmentShader = LoadShader(context->Device, "SolidColor.frag", 0, 0, 0, 0);
	if (fragmentShader == NULL)
	{
		SDL_Log("Failed to create fragment shader!");
		return -1;
	}

    HDRTexture = SDL_GpuCreateTexture(context->Device, &(SDL_GpuTextureCreateInfo){
		.type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_SFLOAT,
        .width = img_x,
        .height = img_y,
        .depth = 1,
        .layerCount = 1,
        .levelCount = 1,
        .usageFlags = SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ_BIT
    });

	ToneMapTexture = SDL_GpuCreateTexture(context->Device, &(SDL_GpuTextureCreateInfo){
		.type = SDL_GPU_TEXTURETYPE_2D,
		.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_SFLOAT,
		.width = img_x,
		.height = img_y,
		.depth = 1,
		.layerCount = 1,
		.levelCount = 1,
		.usageFlags = SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ_BIT | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE_BIT
	});

	TransferTexture = SDL_GpuCreateTexture(context->Device, &(SDL_GpuTextureCreateInfo){
		.type = SDL_GPU_TEXTURETYPE_2D,
		.format = SDL_GpuGetSwapchainTextureFormat(context->Device, context->Window),
		.width = img_x,
		.height = img_y,
		.depth = 1,
		.layerCount = 1,
		.levelCount = 1,
		.usageFlags = SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE_BIT
	});

    SDL_GpuReleaseShader(context->Device, vertexShader);
    SDL_GpuReleaseShader(context->Device, fragmentShader);

    SDL_GpuTransferBuffer* imageDataTransferBuffer = SDL_GpuCreateTransferBuffer(
        context->Device,
		&(SDL_GpuTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.sizeInBytes = sizeof(float) * 4 * img_x * img_y
		}
    );

    Uint8* imageTransferPtr;
    SDL_GpuMapTransferBuffer(context->Device, imageDataTransferBuffer, SDL_FALSE, (void**) &imageTransferPtr);
    SDL_memcpy(imageTransferPtr, hdrImageData, sizeof(float) * 4 * img_x * img_y);
    SDL_GpuUnmapTransferBuffer(context->Device, imageDataTransferBuffer);

    SDL_free(hdrImageData);

    SDL_GpuCommandBuffer* uploadCmdBuf = SDL_GpuAcquireCommandBuffer(context->Device);
    SDL_GpuCopyPass* copyPass = SDL_GpuBeginCopyPass(uploadCmdBuf);

    SDL_GpuUploadToTexture(
        copyPass,
        &(SDL_GpuTextureTransferInfo) {
        	.transferBuffer = imageDataTransferBuffer,
        	.offset = 0, /* Zeroes out the rest */
        },
        &(SDL_GpuTextureRegion){
            .texture = HDRTexture,
            .w = img_x,
            .h = img_y,
            .d = 1
        },
        SDL_FALSE
    );

    SDL_GpuEndCopyPass(copyPass);

    SDL_GpuSubmit(uploadCmdBuf);

    SDL_GpuReleaseTransferBuffer(context->Device, imageDataTransferBuffer);

	tonemapOperators[0] = BuildPostProcessComputePipeline(context->Device, "ToneMapReinhard.comp");
	tonemapOperators[1] = BuildPostProcessComputePipeline(context->Device, "ToneMapExtendedReinhardLuminance.comp");
	tonemapOperators[2] = BuildPostProcessComputePipeline(context->Device, "ToneMapHable.comp");
	tonemapOperators[3] = BuildPostProcessComputePipeline(context->Device, "ToneMapACES.comp");

	currentTonemapOperator = tonemapOperators[0];

	LinearToSRGBPipeline = BuildPostProcessComputePipeline(context->Device, "LinearToSRGB.comp");
	LinearToST2084Pipeline = BuildPostProcessComputePipeline(context->Device, "LinearToST2084.comp");

	SDL_Log("Press Left/Right to cycle swapchain composition");
	SDL_Log("Press Up/Down to cycle tonemap operators");

    return 0;
}

static int Update(Context* context)
{
	/* Swapchain composition selection */
	if (context->LeftPressed)
	{
		swapchainCompositionSelectionIndex -= 1;
		if (swapchainCompositionSelectionIndex < 0)
		{
			swapchainCompositionSelectionIndex = swapchainCompositionCount - 1;
		}

		ChangeSwapchainComposition(context, swapchainCompositionSelectionIndex);
	}
	else if (context->RightPressed)
	{
		swapchainCompositionSelectionIndex = (swapchainCompositionSelectionIndex + 1) % swapchainCompositionCount;

		ChangeSwapchainComposition(context, swapchainCompositionSelectionIndex);
	}

	if (context->UpPressed)
	{
		tonemapOperatorSelectionIndex -= 1;
		if (tonemapOperatorSelectionIndex < 0)
		{
			tonemapOperatorSelectionIndex = tonemapOperatorCount - 1;
		}

		ChangeTonemapOperator(context, tonemapOperatorSelectionIndex);
	}
	else if (context->DownPressed)
	{
		tonemapOperatorSelectionIndex = (tonemapOperatorSelectionIndex + 1) % tonemapOperatorCount;

		ChangeTonemapOperator(context, tonemapOperatorSelectionIndex);
	}

    return 0;
}

static int Draw(Context* context)
{
    SDL_GpuCommandBuffer* cmdbuf = SDL_GpuAcquireCommandBuffer(context->Device);
    if (cmdbuf == NULL)
    {
        SDL_Log("GpuAcquireCommandBuffer failed");
        return -1;
    }

    Uint32 swapchainWidth, swapchainHeight;
    SDL_GpuTexture* swapchainTexture = SDL_GpuAcquireSwapchainTexture(cmdbuf, context->Window, &swapchainWidth, &swapchainHeight);
    if (swapchainTexture != NULL)
    {
		/* Tonemap */
		SDL_GpuComputePass* computePass = SDL_GpuBeginComputePass(
			cmdbuf,
			(SDL_GpuStorageTextureReadWriteBinding[]){{
				.texture = ToneMapTexture,
				.cycle = SDL_TRUE
			}},
			1,
			NULL,
			0
		);

		SDL_GpuBindComputePipeline(computePass, currentTonemapOperator);
		SDL_GpuBindComputeStorageTextures(
			computePass,
			0,
			&HDRTexture,
			1
		);
		SDL_GpuDispatchCompute(computePass, w / 8, h / 8, 1);
		SDL_GpuEndComputePass(computePass);

		SDL_GpuTexture* BlitSourceTexture = ToneMapTexture;

		/* Transfer to target color space if necessary */
		if (
			currentSwapchainComposition == SDL_GPU_SWAPCHAINCOMPOSITION_SDR ||
			currentSwapchainComposition == SDL_GPU_SWAPCHAINCOMPOSITION_HDR10_ST2048
		) {
			computePass = SDL_GpuBeginComputePass(
				cmdbuf,
				(SDL_GpuStorageTextureReadWriteBinding[]){{
					.texture = TransferTexture,
					.cycle = SDL_TRUE
				}},
				1,
				NULL,
				0
			);

			if (currentSwapchainComposition == SDL_GPU_SWAPCHAINCOMPOSITION_SDR)
			{
				SDL_GpuBindComputePipeline(computePass, LinearToSRGBPipeline);
			}
			else
			{
				SDL_GpuBindComputePipeline(computePass, LinearToST2084Pipeline);
			}

			SDL_GpuBindComputeStorageTextures(
				computePass,
				0,
				&ToneMapTexture,
				1
			);
			SDL_GpuDispatchCompute(computePass, w / 8, h / 8, 1);
			SDL_GpuEndComputePass(computePass);

			BlitSourceTexture = TransferTexture;
		}

		/* Blit to swapchain */
		SDL_GpuBlit(
			cmdbuf,
			&(SDL_GpuTextureRegion){
				.texture = BlitSourceTexture,
				.w = w,
				.h = h,
				.d = 1,
			},
			&(SDL_GpuTextureRegion){
				.texture = swapchainTexture,
				.w = swapchainWidth,
				.h = swapchainHeight,
				.d = 1
			},
			SDL_GPU_FILTER_NEAREST,
			SDL_FALSE
		);
    }

    SDL_GpuSubmit(cmdbuf);

    return 0;
}

static void Quit(Context* context)
{
	for (Sint32 i = 0; i < tonemapOperatorCount; i += 1)
	{
		SDL_GpuReleaseComputePipeline(context->Device, tonemapOperators[i]);
	}

	SDL_GpuReleaseComputePipeline(context->Device, LinearToSRGBPipeline);
	SDL_GpuReleaseComputePipeline(context->Device, LinearToST2084Pipeline);

    SDL_GpuReleaseTexture(context->Device, HDRTexture);
	SDL_GpuReleaseTexture(context->Device, ToneMapTexture);
	SDL_GpuReleaseTexture(context->Device, TransferTexture);

    SDL_GpuUnclaimWindow(context->Device, context->Window);
    SDL_DestroyWindow(context->Window);
    SDL_GpuDestroyDevice(context->Device);
}

Example ToneMapping_Example = { "ToneMapping", Init, Update, Draw, Quit };
