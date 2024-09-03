#include "Common.h"

static SDL_GPUGraphicsPipeline* DrawPipeline;
static SDL_GPUTexture* Texture;
static SDL_GPUSampler* Sampler;
static SDL_GPUBuffer* VertexBuffer;

static int Init(Context* context)
{
    int result = CommonInit(context, 0);
    if (result < 0)
    {
        return result;
    }

    SDL_GPUShader *vertexShader = LoadShader(context->Device, "TexturedQuad.vert", 0, 0, 0, 0);
    if (vertexShader == NULL)
    {
        SDL_Log("Failed to create vertex shader!");
        return -1;
    }

    SDL_GPUShader *fragmentShader = LoadShader(context->Device, "TexturedQuad.frag", 1, 0, 0, 0);
    if (fragmentShader == NULL)
    {
        SDL_Log("Failed to create fragment shader!");
        return -1;
    }

    SDL_GPUComputePipeline* fillTexturePipeline = CreateComputePipelineFromShader(
	context->Device,
	"FillTexture.comp",
	&(SDL_GPUComputePipelineCreateInfo) {
		.writeOnlyStorageTextureCount = 1,
		.threadCountX = 8,
		.threadCountY = 8,
		.threadCountZ = 1,
	}
    );

    DrawPipeline = SDL_CreateGPUGraphicsPipeline(context->Device, &(SDL_GPUGraphicsPipelineCreateInfo){
		.attachmentInfo = {
			.colorAttachmentCount = 1,
			.colorAttachmentDescriptions = (SDL_GPUColorAttachmentDescription[]){{
				.format = SDL_GetGPUSwapchainTextureFormat(context->Device, context->Window),
				.blendState = {
					.blendEnable = SDL_TRUE,
					.alphaBlendOp = SDL_GPU_BLENDOP_ADD,
					.colorBlendOp = SDL_GPU_BLENDOP_ADD,
					.colorWriteMask = 0xF,
					.srcColorBlendFactor = SDL_GPU_BLENDFACTOR_ONE,
					.srcAlphaBlendFactor = SDL_GPU_BLENDFACTOR_ONE,
					.dstColorBlendFactor = SDL_GPU_BLENDFACTOR_ZERO,
					.dstAlphaBlendFactor = SDL_GPU_BLENDFACTOR_ZERO
				}
			}}
		},
        .primitiveType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
		.vertexInputState = (SDL_GPUVertexInputState){
			.vertexBindingCount = 1,
			.vertexBindings = (SDL_GPUVertexBinding[]){{
				.binding = 0,
				.inputRate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
				.instanceStepRate = 0,
				.stride = sizeof(PositionTextureVertex)
			}},
			.vertexAttributeCount = 2,
			.vertexAttributes = (SDL_GPUVertexAttribute[]){{
				.binding = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
				.location = 0,
				.offset = 0
			}, {
				.binding = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
				.location = 1,
				.offset = sizeof(float) * 3
			}}
		},
        .multisampleState.sampleMask = 0xFFFF,
        .vertexShader = vertexShader,
        .fragmentShader = fragmentShader
    });

    SDL_ReleaseGPUShader(context->Device, vertexShader);
    SDL_ReleaseGPUShader(context->Device, fragmentShader);

    int w, h;
    SDL_GetWindowSizeInPixels(context->Window, &w, &h);

    Texture = SDL_CreateGPUTexture(context->Device, &(SDL_GPUTextureCreateInfo){
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .width = w,
        .height = h,
        .layerCountOrDepth = 1,
        .levelCount = 1,
        .usageFlags = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_SAMPLER
    });

    Sampler = SDL_CreateGPUSampler(context->Device, &(SDL_GPUSamplerCreateInfo){
        .addressModeU = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .addressModeV = SDL_GPU_SAMPLERADDRESSMODE_REPEAT
    });

    VertexBuffer = SDL_CreateGPUBuffer(
        context->Device,
        &(SDL_GPUBufferCreateInfo) {
            .usageFlags = SDL_GPU_BUFFERUSAGE_VERTEX,
            .sizeInBytes = sizeof(PositionTextureVertex) * 6
        }
    );

    SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(
		context->Device,
        &(SDL_GPUTransferBufferCreateInfo) {
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .sizeInBytes = sizeof(PositionTextureVertex) * 6
        }
	);

	PositionTextureVertex* transferData = SDL_MapGPUTransferBuffer(
		context->Device,
		transferBuffer,
		SDL_FALSE
	);

    transferData[0] = (PositionTextureVertex) { -1, -1, 0, 0, 0 };
	transferData[1] = (PositionTextureVertex) {  1, -1, 0, 1, 0 };
	transferData[2] = (PositionTextureVertex) {  1,  1, 0, 1, 1 };
	transferData[3] = (PositionTextureVertex) { -1, -1, 0, 0, 0 };
	transferData[4] = (PositionTextureVertex) {  1,  1, 0, 1, 1 };
	transferData[5] = (PositionTextureVertex) { -1,  1, 0, 0, 1 };

	SDL_UnmapGPUTransferBuffer(context->Device, transferBuffer);

	SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(context->Device);
	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);

	SDL_UploadToGPUBuffer(
		copyPass,
		&(SDL_GPUTransferBufferLocation) {
			.transferBuffer = transferBuffer,
			.offset = 0
		},
		&(SDL_GPUBufferRegion) {
			.buffer = VertexBuffer,
			.offset = 0,
			.size = sizeof(PositionTextureVertex) * 6
		},
		SDL_FALSE
	);

	SDL_EndGPUCopyPass(copyPass);

    SDL_GPUComputePass* computePass = SDL_BeginGPUComputePass(
        cmdBuf,
        (SDL_GPUStorageTextureWriteOnlyBinding[]){{
            .texture = Texture
        }},
        1,
        NULL,
        0
    );

    SDL_BindGPUComputePipeline(computePass, fillTexturePipeline);
    SDL_DispatchGPUCompute(computePass, w / 8, h / 8, 1);
    SDL_EndGPUComputePass(computePass);

	SDL_SubmitGPUCommandBuffer(cmdBuf);

    SDL_ReleaseGPUComputePipeline(context->Device, fillTexturePipeline);
	SDL_ReleaseGPUTransferBuffer(context->Device, transferBuffer);

    return 0;
}

static int Update(Context* context)
{
    return 0;
}

static int Draw(Context* context)
{
    SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(context->Device);
    if (cmdbuf == NULL)
    {
        SDL_Log("GPUAcquireCommandBuffer failed!");
        return -1;
    }

    Uint32 w, h;
    SDL_GPUTexture* swapchainTexture = SDL_AcquireGPUSwapchainTexture(cmdbuf, context->Window, &w, &h);
    if (swapchainTexture != NULL)
    {
        SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(
            cmdbuf,
            (SDL_GPUColorAttachmentInfo[]){{
                .texture = swapchainTexture,
                .loadOp = SDL_GPU_LOADOP_CLEAR,
                .storeOp = SDL_GPU_STOREOP_STORE,
                .clearColor.a = 1,
                .cycle = SDL_FALSE
            }},
            1,
            NULL
        );

        SDL_BindGPUGraphicsPipeline(renderPass, DrawPipeline);
        SDL_BindGPUVertexBuffers(renderPass, 0, &(SDL_GPUBufferBinding){ .buffer = VertexBuffer, .offset = 0 }, 1);
        SDL_BindGPUFragmentSamplers(renderPass, 0, &(SDL_GPUTextureSamplerBinding){ .texture = Texture, .sampler = Sampler }, 1);
        SDL_DrawGPUPrimitives(renderPass, 6, 1, 0, 0);

        SDL_EndGPURenderPass(renderPass);
    }

    SDL_SubmitGPUCommandBuffer(cmdbuf);

    return 0;
}

static void Quit(Context* context)
{
    SDL_ReleaseGPUGraphicsPipeline(context->Device, DrawPipeline);
    SDL_ReleaseGPUTexture(context->Device, Texture);
    SDL_ReleaseGPUSampler(context->Device, Sampler);
    SDL_ReleaseGPUBuffer(context->Device, VertexBuffer);

    CommonQuit(context);
}

Example BasicCompute_Example = { "BasicCompute", Init, Update, Draw, Quit };
