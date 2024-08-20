#include "Common.h"

static SDL_GpuGraphicsPipeline* DrawPipeline;
static SDL_GpuTexture* Texture;
static SDL_GpuSampler* Sampler;
static SDL_GpuBuffer* VertexBuffer;

static int Init(Context* context)
{
    int result = CommonInit(context, 0);
    if (result < 0)
    {
        return result;
    }

    SDL_GpuShader *vertexShader = LoadShader(context->Device, "TexturedQuad.vert", 0, 0, 0, 0);
    if (vertexShader == NULL)
    {
        SDL_Log("Failed to create vertex shader!");
        return -1;
    }

    SDL_GpuShader *fragmentShader = LoadShader(context->Device, "TexturedQuad.frag", 1, 0, 0, 0);
    if (fragmentShader == NULL)
    {
        SDL_Log("Failed to create fragment shader!");
        return -1;
    }

    SDL_GpuComputePipeline* fillTexturePipeline = CreateComputePipelineFromShader(
	context->Device,
	"FillTexture.comp",
	&(SDL_GpuComputePipelineCreateInfo) {
		.readWriteStorageTextureCount = 1,
		.threadCountX = 8,
		.threadCountY = 8,
		.threadCountZ = 1,
	}
    );

    DrawPipeline = SDL_GpuCreateGraphicsPipeline(context->Device, &(SDL_GpuGraphicsPipelineCreateInfo){
		.attachmentInfo = {
			.colorAttachmentCount = 1,
			.colorAttachmentDescriptions = (SDL_GpuColorAttachmentDescription[]){{
				.format = SDL_GpuGetSwapchainTextureFormat(context->Device, context->Window),
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

    SDL_GpuReleaseShader(context->Device, vertexShader);
    SDL_GpuReleaseShader(context->Device, fragmentShader);

    int w, h;
    SDL_GetWindowSizeInPixels(context->Window, &w, &h);

    Texture = SDL_GpuCreateTexture(context->Device, &(SDL_GpuTextureCreateInfo){
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8,
        .width = w,
        .height = h,
        .layerCountOrDepth = 1,
        .levelCount = 1,
        .usageFlags = SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE_BIT | SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT
    });

    Sampler = SDL_GpuCreateSampler(context->Device, &(SDL_GpuSamplerCreateInfo){
        .addressModeU = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .addressModeV = SDL_GPU_SAMPLERADDRESSMODE_REPEAT
    });

    VertexBuffer = SDL_GpuCreateBuffer(
        context->Device,
        &(SDL_GpuBufferCreateInfo) {
            .usageFlags = SDL_GPU_BUFFERUSAGE_VERTEX_BIT,
            .sizeInBytes = sizeof(PositionTextureVertex) * 6
        }
    );

    SDL_GpuTransferBuffer* transferBuffer = SDL_GpuCreateTransferBuffer(
		context->Device,
        &(SDL_GpuTransferBufferCreateInfo) {
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .sizeInBytes = sizeof(PositionTextureVertex) * 6
        }
	);

	PositionTextureVertex* transferData = SDL_GpuMapTransferBuffer(
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

	SDL_GpuUnmapTransferBuffer(context->Device, transferBuffer);

	SDL_GpuCommandBuffer* cmdBuf = SDL_GpuAcquireCommandBuffer(context->Device);
	SDL_GpuCopyPass* copyPass = SDL_GpuBeginCopyPass(cmdBuf);

	SDL_GpuUploadToBuffer(
		copyPass,
		&(SDL_GpuTransferBufferLocation) {
			.transferBuffer = transferBuffer,
			.offset = 0
		},
		&(SDL_GpuBufferRegion) {
			.buffer = VertexBuffer,
			.offset = 0,
			.size = sizeof(PositionTextureVertex) * 6
		},
		SDL_FALSE
	);

	SDL_GpuEndCopyPass(copyPass);

    SDL_GpuComputePass* computePass = SDL_GpuBeginComputePass(
        cmdBuf,
        (SDL_GpuStorageTextureReadWriteBinding[]){{
            .texture = Texture
        }},
        1,
        NULL,
        0
    );

    SDL_GpuBindComputePipeline(computePass, fillTexturePipeline);
    SDL_GpuDispatchCompute(computePass, w / 8, h / 8, 1);
    SDL_GpuEndComputePass(computePass);

	SDL_GpuSubmit(cmdBuf);

    SDL_GpuReleaseComputePipeline(context->Device, fillTexturePipeline);
	SDL_GpuReleaseTransferBuffer(context->Device, transferBuffer);

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
                .texture = swapchainTexture,
                .loadOp = SDL_GPU_LOADOP_CLEAR,
                .storeOp = SDL_GPU_STOREOP_STORE,
                .clearColor.a = 1,
                .cycle = SDL_FALSE
            }},
            1,
            NULL
        );

        SDL_GpuBindGraphicsPipeline(renderPass, DrawPipeline);
        SDL_GpuBindVertexBuffers(renderPass, 0, &(SDL_GpuBufferBinding){ .buffer = VertexBuffer, .offset = 0 }, 1);
        SDL_GpuBindFragmentSamplers(renderPass, 0, &(SDL_GpuTextureSamplerBinding){ .texture = Texture, .sampler = Sampler }, 1);
        SDL_GpuDrawPrimitives(renderPass, 0, 6);

        SDL_GpuEndRenderPass(renderPass);
    }

    SDL_GpuSubmit(cmdbuf);

    return 0;
}

static void Quit(Context* context)
{
    SDL_GpuReleaseGraphicsPipeline(context->Device, DrawPipeline);
    SDL_GpuReleaseTexture(context->Device, Texture);
    SDL_GpuReleaseSampler(context->Device, Sampler);
    SDL_GpuReleaseBuffer(context->Device, VertexBuffer);

    CommonQuit(context);
}

Example BasicCompute_Example = { "BasicCompute", Init, Update, Draw, Quit };
