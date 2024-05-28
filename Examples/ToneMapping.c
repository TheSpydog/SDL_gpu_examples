#include <SDL_gpu_examples.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static SDL_GpuTexture* HDRTexture;
static SDL_GpuTexture* ToneMapTexture;
static SDL_GpuBuffer* QuadBuffer;

static SDL_GpuComputePipeline* ToneMapReinhardHDRExtendedLinear;

static int w, h;

typedef struct PositionTextureVertex
{
    float x, y, z;
    float u, v;
} PositionTextureVertex;

static int Init(Context* context)
{
    /* Manually set up example for HDR rendering */
    context->Device = SDL_GpuCreateDevice(SDL_GPU_BACKEND_ALL, SDL_TRUE);
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

	if (!SDL_GpuClaimWindow(context->Device, context->Window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR_SRGB, SDL_GPU_PRESENTMODE_VSYNC))
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

	size_t csCodeSize;
	void *csBytes = LoadAsset("Content/Shaders/Compiled/ToneMapReinhard.comp.spv", &csCodeSize);
	if (csBytes == NULL)
	{
		SDL_Log("Could not load compute shader from disk!");
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

	SDL_GpuShader* computeShader = SDL_GpuCreateShader(context->Device, &(SDL_GpuShaderCreateInfo){
		.stage = SDL_GPU_SHADERSTAGE_COMPUTE,
		.code = csBytes,
		.codeSize = csCodeSize,
		.entryPointName = "cs_main",
		.format = SDL_GPU_SHADERFORMAT_SPIRV
	});
	if (computeShader == NULL)
	{
		SDL_Log("Failed to create compute shader!");
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
		.usageFlags = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE_BIT
	});

	QuadBuffer = SDL_GpuCreateGpuBuffer(
		context->Device,
		SDL_GPU_BUFFERUSAGE_VERTEX_BIT,
		sizeof(PositionTextureVertex) * 6
	);

	ToneMapReinhardHDRExtendedLinear = SDL_GpuCreateComputePipeline(context->Device, &(SDL_GpuComputePipelineCreateInfo){
		.computeShader = computeShader,
		.pipelineResourceLayoutInfo.readOnlyStorageTextureCount = 1,
		.pipelineResourceLayoutInfo.readWriteStorageTextureCount = 1
	});

    SDL_GpuQueueDestroyShader(context->Device, vertexShader);
    SDL_GpuQueueDestroyShader(context->Device, fragmentShader);
	SDL_GpuQueueDestroyShader(context->Device, computeShader);
    SDL_free(vsBytes);
    SDL_free(fsBytes);
	SDL_free(csBytes);

    SDL_GpuTransferBuffer* bufferTransferBuffer = SDL_GpuCreateTransferBuffer(
        context->Device,
        SDL_GPU_TRANSFERUSAGE_BUFFER,
        SDL_GPU_TRANSFER_MAP_WRITE,
        sizeof(PositionTextureVertex) * 6
    );

    PositionTextureVertex* mapPointer;

    SDL_GpuMapTransferBuffer(
        context->Device,
        bufferTransferBuffer,
        SDL_FALSE,
        (void**) &mapPointer
    );

    mapPointer[0] = (PositionTextureVertex) { -1, -1, 0, 0, 0 };
	mapPointer[1] = (PositionTextureVertex) {  1, -1, 0, 1, 0 };
	mapPointer[2] = (PositionTextureVertex) {  1,  1, 0, 1, 1 };
	mapPointer[3] = (PositionTextureVertex) { -1, -1, 0, 0, 0 };
	mapPointer[4] = (PositionTextureVertex) {  1,  1, 0, 1, 1 };
	mapPointer[5] = (PositionTextureVertex) { -1,  1, 0, 0, 1 };

    SDL_GpuUnmapTransferBuffer(
        context->Device,
        bufferTransferBuffer
    );

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

    SDL_GpuUploadToBuffer(
        copyPass,
        bufferTransferBuffer,
        QuadBuffer,
        &(SDL_GpuBufferCopy) {
            .srcOffset = 0,
            .dstOffset = 0,
            .size = sizeof(PositionTextureVertex) * 6
        },
        SDL_FALSE
    );

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

    SDL_GpuQueueDestroyTransferBuffer(context->Device, bufferTransferBuffer);
    SDL_GpuQueueDestroyTransferBuffer(context->Device, imageDataTransferBuffer);

    return 0;
}

static int Update(Context* context)
{
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
		SDL_GpuComputePass* computePass = SDL_GpuBeginComputePass(cmdbuf);

		SDL_GpuBindComputePipeline(computePass, ToneMapReinhardHDRExtendedLinear);

		SDL_GpuBindComputeStorageTextures(
			computePass,
			0,
			&(SDL_GpuTextureSlice){
				.texture = HDRTexture
			},
			1
		);

		SDL_GpuBindComputeRWStorageTextures(
			computePass,
			0,
			&(SDL_GpuStorageTextureReadWriteBinding){
				.textureSlice.texture = ToneMapTexture,
				.cycle = SDL_TRUE
			},
			1
		);

		SDL_GpuDispatchCompute(computePass, swapchainWidth / 8, swapchainHeight / 8, 1);

		SDL_GpuEndComputePass(computePass);

		SDL_GpuBlit(
			cmdbuf,
			&(SDL_GpuTextureRegion){
				.textureSlice.texture = ToneMapTexture,
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
	SDL_GpuQueueDestroyComputePipeline(context->Device, ToneMapReinhardHDRExtendedLinear);
    SDL_GpuQueueDestroyGpuBuffer(context->Device, QuadBuffer);
    SDL_GpuQueueDestroyTexture(context->Device, HDRTexture);
	SDL_GpuQueueDestroyTexture(context->Device, ToneMapTexture);

    SDL_GpuUnclaimWindow(context->Device, context->Window);
    SDL_DestroyWindow(context->Window);
    SDL_GpuDestroyDevice(context->Device);
}

Example ToneMapping_Example = { "ToneMapping", Init, Update, Draw, Quit };
