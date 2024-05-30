/* Special thanks to Matt Taylor for this overview of tonemapping: https://64.github.io/tonemapping/ */

#include <SDL_gpu_examples.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static SDL_GpuTexture* HDRTexture;
static SDL_GpuTexture* ToneMapTexture;
static SDL_GpuTexture* TransferTexture;

static SDL_GpuSwapchainComposition swapchainCompositions[] =
{
	SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
	SDL_GPU_SWAPCHAINCOMPOSITION_SDR_SRGB,
	SDL_GPU_SWAPCHAINCOMPOSITION_HDR,
	SDL_GPU_SWAPCHAINCOMPOSITION_HDR_ADVANCED
};
static const char* swapchainCompositionNames[] =
{
	"SDL_GPU_SWAPCHAINCOMPOSITION_SDR",
	"SDL_GPU_SWAPCHAINCOMPOSITION_SDR_SRGB",
	"SDL_GPU_SWAPCHAINCOMPOSITION_HDR",
	"SDL_GPU_SWAPCHAINCOMPOSITION_HDR_ADVANCED"
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
	size_t csCodeSize;
	void *csBytes = LoadAsset(spvFile, &csCodeSize);
	if (csBytes == NULL)
	{
		SDL_Log("Could not load compute shader from disk!");
		return NULL;
	}

	SDL_GpuShader* computeShader = SDL_GpuCreateShader(
		device,
		&(SDL_GpuShaderCreateInfo){
			.stage = SDL_GPU_SHADERSTAGE_COMPUTE,
			.code = csBytes,
			.codeSize = csCodeSize,
			.entryPointName = "cs_main",
			.format = SDL_GPU_SHADERFORMAT_SPIRV
		}
	);

	if (computeShader == NULL)
	{
		SDL_Log("Failed to create compute shader!");
		return NULL;
	}

	SDL_GpuComputePipeline* computePipeline = SDL_GpuCreateComputePipeline(
		device,
		&(SDL_GpuComputePipelineCreateInfo){
			.computeShader = computeShader,
			.pipelineResourceLayoutInfo.readOnlyStorageTextureCount = 1,
			.pipelineResourceLayoutInfo.readWriteStorageTextureCount = 1
		}
	);

	SDL_GpuQueueDestroyShader(device, computeShader);
	SDL_free(csBytes);

	return computePipeline;
}

static int Init(Context* context)
{
    /* Manually set up example for HDR rendering */
    context->Device = SDL_GpuCreateDevice(SDL_GPU_BACKEND_D3D11, SDL_TRUE);
	if (context->Device == NULL)
	{
		SDL_Log("GpuCreateDevice failed");
		return -1;
	}

    int img_x, img_y, n;
    float *hdrImageData = stbi_loadf("Content/Images/memorial.hdr", &img_x, &img_y, &n, 4);

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

	size_t vsCodeSize;
	void* vsBytes = LoadAsset("Content/Shaders/Compiled/PositionColorTransform.vert.spv", &vsCodeSize);
	if (vsBytes == NULL)
	{
		SDL_Log("Could not load vertex shader from disk!");
		return -1;
	}

	size_t fsCodeSize;
	void *fsBytes = LoadAsset("Content/Shaders/Compiled/SolidColor.frag.spv", &fsCodeSize);
	if (fsBytes == NULL)
	{
		SDL_Log("Could not load fragment shader from disk!");
		return -1;
	}

	SDL_GpuShader* vertexShader = SDL_GpuCreateShader(context->Device, &(SDL_GpuShaderCreateInfo){
		.stage = SDL_GPU_SHADERSTAGE_VERTEX,
		.code = vsBytes,
		.codeSize = vsCodeSize,
		.entryPointName = "vs_main",
		.format = SDL_GPU_SHADERFORMAT_SPIRV,
	});
	if (vertexShader == NULL)
	{
		SDL_Log("Failed to create vertex shader!");
		return -1;
	}

	SDL_GpuShader* fragmentShader = SDL_GpuCreateShader(context->Device, &(SDL_GpuShaderCreateInfo){
		.stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
		.code = fsBytes,
		.codeSize = fsCodeSize,
		.entryPointName = "fs_main",
		.format = SDL_GPU_SHADERFORMAT_SPIRV
	});
	if (fragmentShader == NULL)
	{
		SDL_Log("Failed to create fragment shader!");
		return -1;
	}

    HDRTexture = SDL_GpuCreateTexture(context->Device, &(SDL_GpuTextureCreateInfo){
        .format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_SFLOAT,
        .width = img_x,
        .height = img_y,
        .depth = 1,
        .layerCount = 1,
        .levelCount = 1,
        .usageFlags = SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ_BIT
    });

	ToneMapTexture = SDL_GpuCreateTexture(context->Device, &(SDL_GpuTextureCreateInfo){
		.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_SFLOAT,
		.width = img_x,
		.height = img_y,
		.depth = 1,
		.layerCount = 1,
		.levelCount = 1,
		.usageFlags = SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ_BIT | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE_BIT
	});

	TransferTexture = SDL_GpuCreateTexture(context->Device, &(SDL_GpuTextureCreateInfo){
		.format = SDL_GpuGetSwapchainTextureFormat(context->Device, context->Window),
		.width = img_x,
		.height = img_y,
		.depth = 1,
		.layerCount = 1,
		.levelCount = 1,
		.usageFlags = SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE_BIT
	});

    SDL_GpuQueueDestroyShader(context->Device, vertexShader);
    SDL_GpuQueueDestroyShader(context->Device, fragmentShader);
    SDL_free(vsBytes);
    SDL_free(fsBytes);

    SDL_GpuTransferBuffer* imageDataTransferBuffer = SDL_GpuCreateTransferBuffer(
        context->Device,
        SDL_GPU_TRANSFERUSAGE_TEXTURE,
        SDL_GPU_TRANSFER_MAP_WRITE,
        sizeof(float) * 4 * img_x * img_y
    );

    SDL_GpuSetTransferData(
        context->Device,
        hdrImageData,
        imageDataTransferBuffer,
        &(SDL_GpuBufferCopy){
            .srcOffset = 0,
            .dstOffset = 0,
            .size = sizeof(float) * 4 * img_x * img_y
        },
        SDL_FALSE
    );

    SDL_GpuCommandBuffer* uploadCmdBuf = SDL_GpuAcquireCommandBuffer(context->Device);
    SDL_GpuCopyPass* copyPass = SDL_GpuBeginCopyPass(uploadCmdBuf);

    SDL_GpuUploadToTexture(
        copyPass,
        imageDataTransferBuffer,
        &(SDL_GpuTextureRegion){
            .textureSlice.texture = HDRTexture,
            .w = img_x,
            .h = img_y,
            .d = 1
        },
        &(SDL_GpuBufferImageCopy){
            .bufferOffset = 0
        },
        SDL_FALSE
    );

    SDL_GpuEndCopyPass(copyPass);

    SDL_GpuSubmit(uploadCmdBuf);

    SDL_GpuQueueDestroyTransferBuffer(context->Device, imageDataTransferBuffer);

	tonemapOperators[0] = BuildPostProcessComputePipeline(context->Device, "Content/Shaders/Compiled/ToneMapReinhard.comp.spv");
	tonemapOperators[1] = BuildPostProcessComputePipeline(context->Device, "Content/Shaders/Compiled/ToneMapExtendedReinhardLuminance.comp.spv");
	tonemapOperators[2] = BuildPostProcessComputePipeline(context->Device, "Content/Shaders/Compiled/ToneMapHable.comp.spv");
	tonemapOperators[3] = BuildPostProcessComputePipeline(context->Device, "Content/Shaders/Compiled/ToneMapACES.comp.spv");

	currentTonemapOperator = tonemapOperators[0];

	LinearToSRGBPipeline = BuildPostProcessComputePipeline(context->Device, "Content/Shaders/Compiled/LinearToSRGB.comp.spv");
	LinearToST2084Pipeline = BuildPostProcessComputePipeline(context->Device, "Content/Shaders/Compiled/LinearToST2084.comp.spv");

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

	context->LeftPressed = 0;
	context->RightPressed = 0;
	context->DownPressed = 0;
	context->UpPressed = 0;

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
				.textureSlice.texture = ToneMapTexture,
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
			&(SDL_GpuTextureSlice){
				.texture = HDRTexture
			},
			1
		);
		SDL_GpuDispatchCompute(computePass, w / 8, h / 8, 1);
		SDL_GpuEndComputePass(computePass);

		SDL_GpuTexture* BlitSourceTexture = ToneMapTexture;

		/* Transfer to target color space if necessary */
		if (
			currentSwapchainComposition == SDL_GPU_SWAPCHAINCOMPOSITION_SDR ||
			currentSwapchainComposition == SDL_GPU_SWAPCHAINCOMPOSITION_HDR_ADVANCED
		) {
			computePass = SDL_GpuBeginComputePass(
				cmdbuf,
				(SDL_GpuStorageTextureReadWriteBinding[]){{
					.textureSlice.texture = TransferTexture,
					.cycle = SDL_TRUE
				}},
				1,
				NULL,
				0
			);

			SDL_GpuBindComputePipeline(computePass, LinearToSRGBPipeline);
			SDL_GpuBindComputeStorageTextures(
				computePass,
				0,
				&(SDL_GpuTextureSlice){
					.texture = ToneMapTexture
				},
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
				.textureSlice.texture = BlitSourceTexture,
				.w = w,
				.h = h,
				.d = 1,
			},
			&(SDL_GpuTextureRegion){
				.textureSlice.texture = swapchainTexture,
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
		SDL_GpuQueueDestroyComputePipeline(context->Device, tonemapOperators[i]);
	}

	SDL_GpuQueueDestroyComputePipeline(context->Device, LinearToSRGBPipeline);
	SDL_GpuQueueDestroyComputePipeline(context->Device, LinearToST2084Pipeline);

    SDL_GpuQueueDestroyTexture(context->Device, HDRTexture);
	SDL_GpuQueueDestroyTexture(context->Device, ToneMapTexture);
	SDL_GpuQueueDestroyTexture(context->Device, TransferTexture);

    SDL_GpuUnclaimWindow(context->Device, context->Window);
    SDL_DestroyWindow(context->Window);
    SDL_GpuDestroyDevice(context->Device);
}

Example ToneMapping_Example = { "ToneMapping", Init, Update, Draw, Quit };
