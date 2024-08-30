#include "Common.h"

static SDL_GPUGraphicsPipeline* MaskerPipeline;
static SDL_GPUGraphicsPipeline* MaskeePipeline;
static SDL_GPUBuffer* VertexBuffer;
static SDL_GPUTexture* DepthStencilTexture;

static int Init(Context* context)
{
	int result = CommonInit(context, 0);
	if (result < 0)
	{
		return result;
	}

	SDL_GPUShader* vertexShader = LoadShader(context->Device, "PositionColor.vert", 0, 0, 0, 0);
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

	SDL_GPUTextureFormat depthStencilFormat;

	if (SDL_GPUTextureSupportsFormat(
		context->Device,
		SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT,
		SDL_GPU_TEXTURETYPE_2D,
		SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET_BIT
	))
	{
		depthStencilFormat = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
	}
	else if (SDL_GPUTextureSupportsFormat(
		context->Device,
		SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT,
		SDL_GPU_TEXTURETYPE_2D,
		SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET_BIT
	))
	{
		depthStencilFormat = SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
	}
	else
	{
		SDL_Log("Stencil formats not supported!");
		return -1;
	}

	SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {
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
			}},
			.hasDepthStencilAttachment = SDL_TRUE,
			.depthStencilFormat = depthStencilFormat
		},
		.depthStencilState = (SDL_GPUDepthStencilState){
			.stencilTestEnable = SDL_TRUE,
			.frontStencilState = (SDL_GPUStencilOpState){
				.compareOp = SDL_GPU_COMPAREOP_NEVER,
				.failOp = SDL_GPU_STENCILOP_REPLACE
			},
			.reference = 1,
			.writeMask = 0xFF
		},
		.rasterizerState = (SDL_GPURasterizerState){
			.cullMode = SDL_GPU_CULLMODE_NONE,
			.fillMode = SDL_GPU_FILLMODE_FILL,
			.frontFace = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE
		},
		.vertexInputState = (SDL_GPUVertexInputState){
			.vertexBindingCount = 1,
			.vertexBindings = (SDL_GPUVertexBinding[]){{
				.binding = 0,
				.inputRate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
				.instanceStepRate = 0,
				.stride = sizeof(PositionColorVertex)
			}},
			.vertexAttributeCount = 2,
			.vertexAttributes = (SDL_GPUVertexAttribute[]){{
				.binding = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
				.location = 0,
				.offset = 0
			}, {
				.binding = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,
				.location = 1,
				.offset = sizeof(float) * 3
			}}
		},
		.multisampleState.sampleMask = 0xFFFF,
		.primitiveType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
		.vertexShader = vertexShader,
		.fragmentShader = fragmentShader
	};

	MaskerPipeline = SDL_CreateGPUGraphicsPipeline(context->Device, &pipelineCreateInfo);
	if (MaskerPipeline == NULL)
	{
		SDL_Log("Failed to create masker pipeline!");
		return -1;
	}

	pipelineCreateInfo.depthStencilState = (SDL_GPUDepthStencilState){
		.stencilTestEnable = SDL_TRUE,
		.frontStencilState = (SDL_GPUStencilOpState){
			.compareOp = SDL_GPU_COMPAREOP_EQUAL
		},
		.reference = 0,
		.compareMask = 0xFF,
		.writeMask = 0
	};

	MaskeePipeline = SDL_CreateGPUGraphicsPipeline(context->Device, &pipelineCreateInfo);
	if (MaskeePipeline == NULL)
	{
		SDL_Log("Failed to create maskee pipeline!");
		return -1;
	}

	SDL_ReleaseGPUShader(context->Device, vertexShader);
	SDL_ReleaseGPUShader(context->Device, fragmentShader);

	VertexBuffer = SDL_CreateGPUBuffer(
		context->Device,
		&(SDL_GPUBufferCreateInfo) {
			.usageFlags = SDL_GPU_BUFFERUSAGE_VERTEX_BIT,
			.sizeInBytes = sizeof(PositionColorVertex) * 6
		}
	);

	int w, h;
	SDL_GetWindowSizeInPixels(context->Window, &w, &h);

	DepthStencilTexture = SDL_CreateGPUTexture(
		context->Device,
		&(SDL_GPUTextureCreateInfo) {
			.type = SDL_GPU_TEXTURETYPE_2D,
			.width = w,
			.height = h,
			.layerCountOrDepth = 1,
			.levelCount = 1,
			.sampleCount = SDL_GPU_SAMPLECOUNT_1,
			.format = depthStencilFormat,
			.usageFlags = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET_BIT
		}
	);

	SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(
		context->Device,
		&(SDL_GPUTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.sizeInBytes = sizeof(PositionColorVertex) * 6
		}
	);

	PositionColorVertex* transferData = SDL_MapGPUTransferBuffer(
		context->Device,
		transferBuffer,
		SDL_FALSE
	);

	transferData[0] = (PositionColorVertex) { -0.5f, -0.5f, 0, 255, 255,   0, 255 };
	transferData[1] = (PositionColorVertex) {  0.5f, -0.5f, 0, 255, 255,   0, 255 };
	transferData[2] = (PositionColorVertex) {     0,  0.5f, 0, 255, 255,   0, 255 };
	transferData[3] = (PositionColorVertex) {    -1,    -1, 0, 255,   0,   0, 255 };
	transferData[4] = (PositionColorVertex) {     1,    -1, 0,   0, 255,   0, 255 };
	transferData[5] = (PositionColorVertex) {     0,     1, 0,   0,   0, 255, 255 };

	SDL_UnmapGPUTransferBuffer(context->Device, transferBuffer);

	SDL_GPUCommandBuffer* uploadCmdBuf = SDL_AcquireGPUCommandBuffer(context->Device);
	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);

	SDL_UploadToGPUBuffer(
		copyPass,
		&(SDL_GPUTransferBufferLocation) {
			.transferBuffer = transferBuffer,
			.offset = 0
		},
		&(SDL_GPUBufferRegion) {
			.buffer = VertexBuffer,
			.offset = 0,
			.size = sizeof(PositionColorVertex) * 6
		},
		SDL_FALSE
	);

	SDL_EndGPUCopyPass(copyPass);
	SDL_SubmitGPUCommandBuffer(uploadCmdBuf);
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
		SDL_Log("GPUAcquireCommandBuffer failed");
		return -1;
	}

	Uint32 w, h;
	SDL_GPUTexture* swapchainTexture = SDL_AcquireGPUSwapchainTexture(cmdbuf, context->Window, &w, &h);
	if (swapchainTexture != NULL)
	{
		SDL_GPUColorAttachmentInfo colorAttachmentInfo = { 0 };
		colorAttachmentInfo.texture = swapchainTexture;
		colorAttachmentInfo.clearColor = (SDL_FColor){ 0.0f, 0.0f, 0.0f, 1.0f };
		colorAttachmentInfo.loadOp = SDL_GPU_LOADOP_CLEAR;
		colorAttachmentInfo.storeOp = SDL_GPU_STOREOP_STORE;

		SDL_GPUDepthStencilAttachmentInfo depthStencilAttachmentInfo = { 0 };
		depthStencilAttachmentInfo.texture = DepthStencilTexture;
		depthStencilAttachmentInfo.cycle = SDL_TRUE;
		depthStencilAttachmentInfo.depthStencilClearValue.depth = 0;
		depthStencilAttachmentInfo.depthStencilClearValue.stencil = 0;
		depthStencilAttachmentInfo.loadOp = SDL_GPU_LOADOP_CLEAR;
		depthStencilAttachmentInfo.storeOp = SDL_GPU_STOREOP_DONT_CARE;
		depthStencilAttachmentInfo.stencilLoadOp = SDL_GPU_LOADOP_CLEAR;
		depthStencilAttachmentInfo.stencilStoreOp = SDL_GPU_STOREOP_DONT_CARE;

		SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(
			cmdbuf,
			&colorAttachmentInfo,
			1,
			&depthStencilAttachmentInfo
		);

		SDL_BindGPUGraphicsPipeline(renderPass, MaskerPipeline);
		SDL_BindGPUVertexBuffers(renderPass, 0, &(SDL_GPUBufferBinding){ .buffer = VertexBuffer, .offset = 0 }, 1);
		SDL_DrawGPUPrimitives(renderPass, 3, 1, 0, 0);
		SDL_BindGPUGraphicsPipeline(renderPass, MaskeePipeline);
		SDL_DrawGPUPrimitives(renderPass, 3, 1, 3, 0);

		SDL_EndGPURenderPass(renderPass);
	}

	SDL_SubmitGPUCommandBuffer(cmdbuf);

	return 0;
}

static void Quit(Context* context)
{
	SDL_ReleaseGPUGraphicsPipeline(context->Device, MaskeePipeline);
	SDL_ReleaseGPUGraphicsPipeline(context->Device, MaskerPipeline);

	SDL_ReleaseGPUTexture(context->Device, DepthStencilTexture);
	SDL_ReleaseGPUBuffer(context->Device, VertexBuffer);

	CommonQuit(context);
}

Example BasicStencil_Example = { "BasicStencil", Init, Update, Draw, Quit };
