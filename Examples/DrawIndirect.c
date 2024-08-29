#include "Common.h"

static SDL_GpuGraphicsPipeline* Pipeline;
static SDL_GpuBuffer* VertexBuffer;
static SDL_GpuBuffer* IndexBuffer;
static SDL_GpuBuffer* DrawBuffer;

static int Init(Context* context)
{
	int result = CommonInit(context, 0);
	if (result < 0)
	{
		return result;
	}

	// Create the shaders
	SDL_GpuShader* vertexShader = LoadShader(context->Device, "PositionColor.vert", 0, 0, 0, 0);
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

	// Create the pipeline
	SDL_GpuGraphicsPipelineCreateInfo pipelineCreateInfo = {
		.attachmentInfo = {
			.colorAttachmentCount = 1,
			.colorAttachmentDescriptions = (SDL_GpuColorAttachmentDescription[]){{
				.format = SDL_GetGpuSwapchainTextureFormat(context->Device, context->Window),
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
		},
		// This is set up to match the vertex shader layout!
		.vertexInputState = (SDL_GpuVertexInputState){
			.vertexBindingCount = 1,
			.vertexBindings = (SDL_GpuVertexBinding[]){{
				.binding = 0,
				.inputRate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
				.instanceStepRate = 0,
				.stride = sizeof(PositionColorVertex)
			}},
			.vertexAttributeCount = 2,
			.vertexAttributes = (SDL_GpuVertexAttribute[]){{
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

	Pipeline = SDL_CreateGpuGraphicsPipeline(context->Device, &pipelineCreateInfo);
	if (Pipeline == NULL)
	{
		SDL_Log("Failed to create pipeline!");
		return -1;
	}

	SDL_ReleaseGpuShader(context->Device, vertexShader);
	SDL_ReleaseGpuShader(context->Device, fragmentShader);

	// Create the buffers
	const Uint32 vertexBufferSize = sizeof(PositionColorVertex) * 10;
	VertexBuffer = SDL_CreateGpuBuffer(
		context->Device,
		&(SDL_GpuBufferCreateInfo) {
			.usageFlags = SDL_GPU_BUFFERUSAGE_VERTEX_BIT,
			.sizeInBytes = vertexBufferSize
		}
	);

	const Uint32 indexBufferSize = sizeof(Uint16) * 6;
	IndexBuffer = SDL_CreateGpuBuffer(
		context->Device,
		&(SDL_GpuBufferCreateInfo) {
			.usageFlags = SDL_GPU_BUFFERUSAGE_INDEX_BIT,
			.sizeInBytes = indexBufferSize
		}
	);

	const Uint32 drawBufferSize = (sizeof(SDL_GpuIndexedIndirectDrawCommand) * 1) + (sizeof(SDL_GpuIndirectDrawCommand) * 2);
	DrawBuffer = SDL_CreateGpuBuffer(
		context->Device,
		&(SDL_GpuBufferCreateInfo) {
			.usageFlags = SDL_GPU_BUFFERUSAGE_INDIRECT_BIT,
			.sizeInBytes = drawBufferSize
		}
	);

	// Set the buffer data
	SDL_GpuTransferBuffer* transferBuffer = SDL_CreateGpuTransferBuffer(
		context->Device,
		&(SDL_GpuTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.sizeInBytes = vertexBufferSize + indexBufferSize + drawBufferSize
		}
	);

	PositionColorVertex* transferData = SDL_MapGpuTransferBuffer(
		context->Device,
		transferBuffer,
		SDL_FALSE
	);

	transferData[0] = (PositionColorVertex) {    -1, -1, 0,	 255,   0,   0, 255 };
	transferData[1] = (PositionColorVertex) {     1, -1, 0,	   0, 255,   0, 255 };
	transferData[2] = (PositionColorVertex) {     1,  1, 0,	   0,   0, 255, 255 };
	transferData[3] = (PositionColorVertex) {    -1,  1, 0,	 255, 255, 255, 255 };

	transferData[4] = (PositionColorVertex) {     1, -1, 0,	   0, 255,   0, 255 };
	transferData[5] = (PositionColorVertex) {     0, -1, 0,	   0,   0, 255, 255 };
	transferData[6] = (PositionColorVertex) {  0.5f,  1, 0,	 255,   0,   0, 255 };
	transferData[7] = (PositionColorVertex) {    -1, -1, 0,	   0, 255,   0, 255 };
	transferData[8] = (PositionColorVertex) {     0, -1, 0,	   0,   0, 255, 255 };
	transferData[9] = (PositionColorVertex) { -0.5f,  1, 0,	 255,   0,   0, 255 };

	Uint16* indexData = (Uint16*) &transferData[10];
	indexData[0] = 0;
	indexData[1] = 1;
	indexData[2] = 2;
	indexData[3] = 0;
	indexData[4] = 2;
	indexData[5] = 3;

	SDL_GpuIndexedIndirectDrawCommand* indexedDrawCommand = (SDL_GpuIndexedIndirectDrawCommand*) &indexData[6];
	indexedDrawCommand[0] = (SDL_GpuIndexedIndirectDrawCommand) { 6, 1, 0, 0, 0 };

	SDL_GpuIndirectDrawCommand* drawCommands = (SDL_GpuIndirectDrawCommand*) &indexedDrawCommand[1];
	drawCommands[0] = (SDL_GpuIndirectDrawCommand) { 3, 1, 4, 0 };
	drawCommands[1] = (SDL_GpuIndirectDrawCommand) { 3, 1, 7, 0 };

	SDL_UnmapGpuTransferBuffer(context->Device, transferBuffer);

	// Upload the transfer data to the GPU buffers
	SDL_GpuCommandBuffer* uploadCmdBuf = SDL_AcquireGpuCommandBuffer(context->Device);
	SDL_GpuCopyPass* copyPass = SDL_BeginGpuCopyPass(uploadCmdBuf);

	SDL_UploadToGpuBuffer(
		copyPass,
		&(SDL_GpuTransferBufferLocation) {
			.transferBuffer = transferBuffer,
			.offset = 0
		},
		&(SDL_GpuBufferRegion) {
			.buffer = VertexBuffer,
			.offset = 0,
			.size = vertexBufferSize
		},
		SDL_FALSE
	);

	SDL_UploadToGpuBuffer(
		copyPass,
		&(SDL_GpuTransferBufferLocation) {
			.transferBuffer = transferBuffer,
			.offset = vertexBufferSize
		},
		&(SDL_GpuBufferRegion) {
			.buffer = IndexBuffer,
			.offset = 0,
			.size = indexBufferSize
		},
		SDL_FALSE
	);

	SDL_UploadToGpuBuffer(
		copyPass,
		&(SDL_GpuTransferBufferLocation) {
			.transferBuffer = transferBuffer,
			.offset = vertexBufferSize + indexBufferSize
		},
		&(SDL_GpuBufferRegion) {
			.buffer = DrawBuffer,
			.offset = 0,
			.size = drawBufferSize
		},
		SDL_FALSE
	);

	SDL_EndGpuCopyPass(copyPass);
	SDL_SubmitGpu(uploadCmdBuf);
	SDL_ReleaseGpuTransferBuffer(context->Device, transferBuffer);

	return 0;
}

static int Update(Context* context)
{
	return 0;
}

static int Draw(Context* context)
{
	SDL_GpuCommandBuffer* cmdbuf = SDL_AcquireGpuCommandBuffer(context->Device);
	if (cmdbuf == NULL)
	{
		SDL_Log("GpuAcquireCommandBuffer failed");
		return -1;
	}

	Uint32 w, h;
	SDL_GpuTexture* swapchainTexture = SDL_AcquireGpuSwapchainTexture(cmdbuf, context->Window, &w, &h);
	if (swapchainTexture != NULL)
	{
		SDL_GpuColorAttachmentInfo colorAttachmentInfo = { 0 };
		colorAttachmentInfo.texture = swapchainTexture;
		colorAttachmentInfo.clearColor = (SDL_FColor){ 0.0f, 0.0f, 0.0f, 1.0f };
		colorAttachmentInfo.loadOp = SDL_GPU_LOADOP_CLEAR;
		colorAttachmentInfo.storeOp = SDL_GPU_STOREOP_STORE;

		SDL_GpuRenderPass* renderPass = SDL_BeginGpuRenderPass(cmdbuf, &colorAttachmentInfo, 1, NULL);

		SDL_BindGpuGraphicsPipeline(renderPass, Pipeline);
		SDL_BindGpuVertexBuffers(renderPass, 0, &(SDL_GpuBufferBinding){ .buffer = VertexBuffer, .offset = 0 }, 1);
		SDL_BindGpuIndexBuffer(renderPass, &(SDL_GpuBufferBinding) {.buffer = IndexBuffer }, SDL_GPU_INDEXELEMENTSIZE_16BIT);
		SDL_DrawGpuIndexedPrimitivesIndirect(renderPass, DrawBuffer, 0, 1, sizeof(SDL_GpuIndexedIndirectDrawCommand));
		SDL_DrawGpuPrimitivesIndirect(renderPass, DrawBuffer, sizeof(SDL_GpuIndexedIndirectDrawCommand), 2, sizeof(SDL_GpuIndirectDrawCommand));

		SDL_EndGpuRenderPass(renderPass);
	}

	SDL_SubmitGpu(cmdbuf);

	return 0;
}

static void Quit(Context* context)
{
	SDL_ReleaseGpuGraphicsPipeline(context->Device, Pipeline);
	SDL_ReleaseGpuBuffer(context->Device, VertexBuffer);
	SDL_ReleaseGpuBuffer(context->Device, IndexBuffer);
	SDL_ReleaseGpuBuffer(context->Device, DrawBuffer);

	CommonQuit(context);
}

Example DrawIndirect_Example = { "DrawIndirect", Init, Update, Draw, Quit };
