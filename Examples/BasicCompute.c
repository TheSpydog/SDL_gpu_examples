#include <SDL_gpu_examples.h>

static SDL_GpuGraphicsPipeline* DrawPipeline;
static SDL_GpuTexture* Texture;
static SDL_GpuSampler* Sampler;
static SDL_GpuBuffer* VertexBuffer;

typedef struct PositionTextureVertex
{
    float x, y, z;
    float u, v;
} PositionTextureVertex;

static int Init(Context* context)
{
    int result = CommonInit(context, 0);
    if (result < 0)
    {
        return result;
    }

    size_t csCodeSize;
    void* csBytes = LoadAsset("Content/Shaders/Compiled/FillTexture.comp.spv", &csCodeSize);
    if (csBytes == NULL)
    {
        SDL_Log("Could not load compute shader from disk!");
        return -1;
    }

    size_t vsCodeSize;
    void* vsBytes = LoadAsset("Content/Shaders/Compiled/TexturedQuad.vert.spv", &vsCodeSize);
    if (vsBytes == NULL)
    {
        SDL_Log("Could not load vertex shader from disk!");
        return -1;
    }

    size_t fsCodeSize;
    void* fsBytes = LoadAsset("Content/Shaders/Compiled/TexturedQuad.frag.spv", &fsCodeSize);
    if (fsBytes == NULL)
    {
        SDL_Log("Could not load fragment shader from disk!");
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

    SDL_GpuShader *vertexShader = SDL_GpuCreateShader(context->Device, &(SDL_GpuShaderCreateInfo){
        .stage = SDL_GPU_SHADERSTAGE_VERTEX,
        .code = vsBytes,
        .codeSize = vsCodeSize,
        .entryPointName = "vs_main",
        .format = SDL_GPU_SHADERFORMAT_SPIRV
    });
    if (vertexShader == NULL)
    {
        SDL_Log("Failed to create vertex shader!");
        return -1;
    }

    SDL_GpuShader *fragmentShader = SDL_GpuCreateShader(context->Device, &(SDL_GpuShaderCreateInfo){
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

    SDL_GpuComputePipeline* fillTexturePipeline = SDL_GpuCreateComputePipeline(context->Device, &(SDL_GpuComputePipelineCreateInfo){
        .computeShader = computeShader,
        .pipelineResourceLayoutInfo = (SDL_GpuComputePipelineResourceLayoutInfo){
            .readOnlyStorageBufferCount = 0,
            .readOnlyStorageTextureCount = 0,
            .readWriteStorageBufferCount = 0,
            .readWriteStorageTextureCount = 1,
            .uniformBufferCount = 0
        }
    });

    DrawPipeline = SDL_GpuCreateGraphicsPipeline(context->Device, &(SDL_GpuGraphicsPipelineCreateInfo){
		.attachmentInfo = {
			.colorAttachmentCount = 1,
			.colorAttachmentDescriptions = (SDL_GpuColorAttachmentDescription[]){{
				.format = SDL_GpuGetSwapchainFormat(context->Device, context->Window),
				.blendState = {
					.blendEnable = SDL_TRUE,
					.alphaBlendOp = SDL_GPU_BLENDOP_ADD,
					.colorBlendOp = SDL_GPU_BLENDOP_ADD,
					.colorWriteMask = 0xF,
					.srcColorBlendFactor = SDL_BLENDFACTOR_ONE,
					.srcAlphaBlendFactor = SDL_BLENDFACTOR_ONE,
					.dstColorBlendFactor = SDL_BLENDFACTOR_ZERO,
					.dstAlphaBlendFactor = SDL_BLENDFACTOR_ZERO
				}
			}}
		},
        .primitiveType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
		.vertexInputState = (SDL_GpuVertexInputState){
			.vertexBindingCount = 1,
			.vertexBindings = (SDL_GpuVertexBinding[]){{
				.binding = 0,
				.inputRate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
				.stepRate = 0,
				.stride = sizeof(PositionTextureVertex)
			}},
			.vertexAttributeCount = 2,
			.vertexAttributes = (SDL_GpuVertexAttribute[]){{
				.binding = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_VECTOR3,
				.location = 0,
				.offset = 0
			}, {
				.binding = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_VECTOR2,
				.location = 1,
				.offset = sizeof(float) * 3
			}}
		},
        .multisampleState.sampleMask = 0xFFFF,
        .vertexShader = vertexShader,
        .fragmentShader = fragmentShader,
        .fragmentResourceLayoutInfo.samplerCount = 1
    });

    SDL_GpuQueueDestroyShader(context->Device, computeShader);
    SDL_GpuQueueDestroyShader(context->Device, vertexShader);
    SDL_GpuQueueDestroyShader(context->Device, fragmentShader);
    SDL_free(csBytes);
    SDL_free(vsBytes);
    SDL_free(fsBytes);

    int w, h;
    SDL_GetWindowSizeInPixels(context->Window, &w, &h);

    Texture = SDL_GpuCreateTexture(context->Device, &(SDL_GpuTextureCreateInfo){
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8,
        .width = w,
        .height = h,
        .depth = 1,
        .layerCount = 1,
        .levelCount = 1,
        .usageFlags = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE_BIT | SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT
    });

    Sampler = SDL_GpuCreateSampler(context->Device, &(SDL_GpuSamplerStateCreateInfo){
        .addressModeU = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .addressModeV = SDL_GPU_SAMPLERADDRESSMODE_REPEAT
    });

    VertexBuffer = SDL_GpuCreateGpuBuffer(
        context->Device,
        SDL_GPU_BUFFERUSAGE_VERTEX_BIT,
        sizeof(PositionTextureVertex) * 6
    );

    SDL_GpuTransferBuffer* transferBuffer = SDL_GpuCreateTransferBuffer(
		context->Device,
		SDL_GPU_TRANSFERUSAGE_BUFFER,
		SDL_GPU_TRANSFER_MAP_WRITE,
		sizeof(PositionTextureVertex) * 6
	);

	PositionTextureVertex* transferData;

	SDL_GpuMapTransferBuffer(
		context->Device,
		transferBuffer,
		SDL_FALSE,
		(void**) &transferData
	);

    transferData[0] = (PositionTextureVertex) { -1, -1, 0, 0, 0 };
	transferData[1] = (PositionTextureVertex) {  1, -1, 0, 1, 0 };
	transferData[2] = (PositionTextureVertex) {  1,  1, 0, 1, 1 };
	transferData[3] = (PositionTextureVertex) { -1, -1, 0, 0, 0 };
	transferData[4] = (PositionTextureVertex) {  1,  1, 0, 1, 1 };
	transferData[5] = (PositionTextureVertex) { -1,  1, 0, 0, 1 };

	SDL_GpuUnmapTransferBuffer(context->Device, transferBuffer);

	SDL_GpuCommandBuffer* cmdBuf = SDL_GpuAcquireCommandBuffer(context->Device);
	SDL_GpuCopyPass* copyPass = SDL_GpuBeginCopyPass(cmdBuf);

	SDL_GpuUploadToBuffer(
		copyPass,
		transferBuffer,
		VertexBuffer,
		&(SDL_GpuBufferCopy) {
			.srcOffset = 0,
			.dstOffset = 0,
			.size = sizeof(PositionTextureVertex) * 6
		},
		SDL_FALSE
	);

	SDL_GpuEndCopyPass(copyPass);

    SDL_GpuComputePass* computePass = SDL_GpuBeginComputePass(cmdBuf);

    SDL_GpuBindComputePipeline(computePass, fillTexturePipeline);
    SDL_GpuBindComputeRWStorageTextures(
        computePass,
        0,
        (SDL_GpuStorageTextureReadWriteBinding[]) {{
            .textureSlice.texture = Texture,
            .cycle = SDL_FALSE
        }},
        1
    );
    SDL_GpuDispatchCompute(computePass, w / 8, h / 8, 1);

    SDL_GpuEndComputePass(computePass);
	SDL_GpuSubmit(cmdBuf);

    SDL_GpuQueueDestroyComputePipeline(context->Device, fillTexturePipeline);
	SDL_GpuQueueDestroyTransferBuffer(context->Device, transferBuffer);

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
        SDL_Log("GpuAcquireCommandBuffer failed!");
        return -1;
    }

    Uint32 w, h;
    SDL_GpuTexture* swapchainTexture = SDL_GpuAcquireSwapchainTexture(cmdbuf, context->Window, &w, &h);
    if (swapchainTexture != NULL)
    {
        SDL_GpuRenderPass* renderPass = SDL_GpuBeginRenderPass(
            cmdbuf,
            (SDL_GpuColorAttachmentInfo[]){{
                .textureSlice.texture = swapchainTexture,
                .loadOp = SDL_GPU_LOADOP_CLEAR,
                .storeOp = SDL_GPU_STOREOP_STORE,
                .clearColor.a = 1,
                .cycle = SDL_FALSE
            }},
            1,
            NULL
        );

        SDL_GpuBindGraphicsPipeline(renderPass, DrawPipeline);
        SDL_GpuBindVertexBuffers(renderPass, 0, 1, &(SDL_GpuBufferBinding){ .gpuBuffer = VertexBuffer, .offset = 0 });
        SDL_GpuBindFragmentSamplers(renderPass, 0, &(SDL_GpuTextureSamplerBinding){ .texture = Texture, .sampler = Sampler }, 1);
        SDL_GpuDrawPrimitives(renderPass, 0, 2);

        SDL_GpuEndRenderPass(renderPass);
    }

    SDL_GpuSubmit(cmdbuf);

    return 0;
}

static void Quit(Context* context)
{
    SDL_GpuQueueDestroyGraphicsPipeline(context->Device, DrawPipeline);
    SDL_GpuQueueDestroyTexture(context->Device, Texture);
    SDL_GpuQueueDestroySampler(context->Device, Sampler);
    SDL_GpuQueueDestroyGpuBuffer(context->Device, VertexBuffer);

    CommonQuit(context);
}

Example BasicCompute_Example = { "BasicCompute", Init, Update, Draw, Quit };
