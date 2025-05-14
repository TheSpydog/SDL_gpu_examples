/* Special thanks to Matt Taylor for this overview of tonemapping: https://64.github.io/tonemapping/ */

#include "Common.h"

static SDL_GPUTexture* HDRTexture;
static SDL_GPUTexture* ToneMapTexture;
static SDL_GPUTexture* TransferTexture;

static SDL_GPUSwapchainComposition swapchainCompositions[] =
{
	SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
	SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR,
	SDL_GPU_SWAPCHAINCOMPOSITION_HDR_EXTENDED_LINEAR,
	SDL_GPU_SWAPCHAINCOMPOSITION_HDR10_ST2084
};
static const char* swapchainCompositionNames[] =
{
	"SDL_GPU_SWAPCHAINCOMPOSITION_SDR",
	"SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR",
	"SDL_GPU_SWAPCHAINCOMPOSITION_HDR_EXTENDED_LINEAR",
	"SDL_GPU_SWAPCHAINCOMPOSITION_HDR10_ST2084"
};
static Sint32 swapchainCompositionCount = sizeof(swapchainCompositions)/sizeof(SDL_GPUSwapchainComposition);
static Sint32 swapchainCompositionSelectionIndex = 0;
static SDL_GPUSwapchainComposition currentSwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;

static const char* tonemapOperatorNames[] =
{
	"Reinhard",
	"ExtendedReinhardLuminance",
	"Hable",
	"ACES"
};
static Sint32 tonemapOperatorCount = sizeof(tonemapOperatorNames)/sizeof(char*);
static SDL_GPUComputePipeline* tonemapOperators[sizeof(tonemapOperatorNames)/sizeof(char*)];
static Sint32 tonemapOperatorSelectionIndex = 0;
static SDL_GPUComputePipeline* currentTonemapOperator;

static SDL_GPUComputePipeline* LinearToSRGBPipeline;
static SDL_GPUComputePipeline* LinearToST2084Pipeline;

static int w, h;

static void ChangeSwapchainComposition(Context* context, Uint32 selectionIndex)
{
	if (SDL_WindowSupportsGPUSwapchainComposition(context->Device, context->Window, swapchainCompositions[selectionIndex]))
	{
		currentSwapchainComposition = swapchainCompositions[selectionIndex];
		SDL_Log("Changing swapchain composition to %s", swapchainCompositionNames[selectionIndex]);
		SDL_SetGPUSwapchainParameters(context->Device, context->Window, currentSwapchainComposition, SDL_GPU_PRESENTMODE_VSYNC);
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

static SDL_GPUComputePipeline* BuildPostProcessComputePipeline(SDL_GPUDevice *device, const char* spvFile)
{
	return CreateComputePipelineFromShader(
		device,
		spvFile,
		&(SDL_GPUComputePipelineCreateInfo){
			.num_readonly_storage_textures = 1,
			.num_readwrite_storage_textures = 1,
			.threadcount_x = 8,
			.threadcount_y = 8,
			.threadcount_z = 1,
		}
	);
}

static int Init(Context* context)
{
	int result = CommonInit(context, 0);
	if (result < 0)
	{
		return result;
	}

    int n;
    float *hdrImageData = LoadHDRImage("memorial.hdr", &w, &h, &n, 4);

    if (hdrImageData == NULL)
    {
        SDL_Log("Could not load HDR image data!");
        return -1;
    }

	SDL_SetWindowSize(context->Window, w, h);

	SDL_GPUShader* vertexShader = LoadShader(context->Device, "PositionColorTransform.vert", 0, 0, 0, 0);
	if (vertexShader == NULL)
	{
		SDL_Log("Failed to create vertex shader!");
		return -1;
	}

	SDL_GPUShader* fragmentShader = LoadShader(context->Device, "SolidColor.frag", 0, 0, 0, 0);
	if (fragmentShader == NULL)
	{
		SDL_Log("Failed to create fragment shader!");
		return -1;
	}

    HDRTexture = SDL_CreateGPUTexture(context->Device, &(SDL_GPUTextureCreateInfo){
		.type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
        .width = w,
        .height = h,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ
    });

	ToneMapTexture = SDL_CreateGPUTexture(context->Device, &(SDL_GPUTextureCreateInfo){
		.type = SDL_GPU_TEXTURETYPE_2D,
		.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT,
		.width = w,
		.height = h,
		.layer_count_or_depth = 1,
		.num_levels = 1,
		.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE
	});

	TransferTexture = SDL_CreateGPUTexture(context->Device, &(SDL_GPUTextureCreateInfo){
		.type = SDL_GPU_TEXTURETYPE_2D,
		.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
		.width = w,
		.height = h,
		.layer_count_or_depth = 1,
		.num_levels = 1,
		.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE
	});

    SDL_ReleaseGPUShader(context->Device, vertexShader);
    SDL_ReleaseGPUShader(context->Device, fragmentShader);

    SDL_GPUTransferBuffer* imageDataTransferBuffer = SDL_CreateGPUTransferBuffer(
        context->Device,
		&(SDL_GPUTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = sizeof(float) * 4 * w * h
		}
    );

    Uint8* imageTransferPtr = SDL_MapGPUTransferBuffer(
	    context->Device,
	    imageDataTransferBuffer,
	    false
    );
    SDL_memcpy(imageTransferPtr, hdrImageData, sizeof(float) * 4 * w * h);
    SDL_UnmapGPUTransferBuffer(context->Device, imageDataTransferBuffer);

    SDL_free(hdrImageData);

    SDL_GPUCommandBuffer* uploadCmdBuf = SDL_AcquireGPUCommandBuffer(context->Device);
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);

    SDL_UploadToGPUTexture(
        copyPass,
        &(SDL_GPUTextureTransferInfo) {
        	.transfer_buffer = imageDataTransferBuffer,
        	.offset = 0, /* Zeroes out the rest */
        },
        &(SDL_GPUTextureRegion){
            .texture = HDRTexture,
            .w = w,
            .h = h,
            .d = 1
        },
        false
    );

    SDL_EndGPUCopyPass(copyPass);

    SDL_SubmitGPUCommandBuffer(uploadCmdBuf);

    SDL_ReleaseGPUTransferBuffer(context->Device, imageDataTransferBuffer);

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
    SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(context->Device);
    if (cmdbuf == NULL)
    {
        SDL_Log("AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return -1;
    }

    SDL_GPUTexture* swapchainTexture;
	Uint32 swapchainWidth, swapchainHeight;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdbuf, context->Window, &swapchainTexture, &swapchainWidth, &swapchainHeight)) {
        SDL_Log("WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
        return -1;
    }

    if (swapchainTexture != NULL)
    {
		/* Tonemap */
		SDL_GPUComputePass* computePass = SDL_BeginGPUComputePass(
			cmdbuf,
			(SDL_GPUStorageTextureReadWriteBinding[]){{
				.texture = ToneMapTexture,
				.cycle = true
			}},
			1,
			NULL,
			0
		);

		SDL_BindGPUComputePipeline(computePass, currentTonemapOperator);
		SDL_BindGPUComputeStorageTextures(
			computePass,
			0,
			&HDRTexture,
			1
		);
		SDL_DispatchGPUCompute(computePass, w / 8, h / 8, 1);
		SDL_EndGPUComputePass(computePass);

		SDL_GPUTexture* BlitSourceTexture = ToneMapTexture;

		/* Transfer to target color space if necessary */
		if (
			currentSwapchainComposition == SDL_GPU_SWAPCHAINCOMPOSITION_SDR ||
			currentSwapchainComposition == SDL_GPU_SWAPCHAINCOMPOSITION_HDR10_ST2084
		) {
			computePass = SDL_BeginGPUComputePass(
				cmdbuf,
				(SDL_GPUStorageTextureReadWriteBinding[]){{
					.texture = TransferTexture,
					.cycle = true
				}},
				1,
				NULL,
				0
			);

			if (currentSwapchainComposition == SDL_GPU_SWAPCHAINCOMPOSITION_SDR)
			{
				SDL_BindGPUComputePipeline(computePass, LinearToSRGBPipeline);
			}
			else
			{
				SDL_BindGPUComputePipeline(computePass, LinearToST2084Pipeline);
			}

			SDL_BindGPUComputeStorageTextures(
				computePass,
				0,
				&ToneMapTexture,
				1
			);
			SDL_DispatchGPUCompute(computePass, w / 8, h / 8, 1);
			SDL_EndGPUComputePass(computePass);

			BlitSourceTexture = TransferTexture;
		}

		/* Blit to swapchain */
		SDL_BlitGPUTexture(
			cmdbuf,
			&(SDL_GPUBlitInfo){
				.source.texture = BlitSourceTexture,
				.source.w = w,
				.source.h = h,
				.destination.texture = swapchainTexture,
				.destination.w = swapchainWidth,
				.destination.h = swapchainHeight,
				.load_op = SDL_GPU_LOADOP_DONT_CARE,
				.filter = SDL_GPU_FILTER_NEAREST
			}
		);
    }

    SDL_SubmitGPUCommandBuffer(cmdbuf);

    return 0;
}

static void Quit(Context* context)
{
	for (Sint32 i = 0; i < tonemapOperatorCount; i += 1)
	{
		SDL_ReleaseGPUComputePipeline(context->Device, tonemapOperators[i]);
	}

	SDL_ReleaseGPUComputePipeline(context->Device, LinearToSRGBPipeline);
	SDL_ReleaseGPUComputePipeline(context->Device, LinearToST2084Pipeline);

    SDL_ReleaseGPUTexture(context->Device, HDRTexture);
	SDL_ReleaseGPUTexture(context->Device, ToneMapTexture);
	SDL_ReleaseGPUTexture(context->Device, TransferTexture);

    SDL_ReleaseWindowFromGPUDevice(context->Device, context->Window);
    SDL_DestroyWindow(context->Window);
    SDL_DestroyGPUDevice(context->Device);
}

Example ToneMapping_Example = { "ToneMapping", Init, Update, Draw, Quit };
