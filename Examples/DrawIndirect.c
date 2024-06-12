#include <SDL_gpu_examples.h>

static SDL_GpuGraphicsPipeline* Pipeline;
static SDL_GpuBuffer* VertexBuffer;
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
			}},
		},
		// This is set up to match the vertex shader layout!
		.vertexInputState = (SDL_GpuVertexInputState){
			.vertexBindingCount = 1,
			.vertexBindings = (SDL_GpuVertexBinding[]){{
				.binding = 0,
				.inputRate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
				.stepRate = 0,
				.stride = sizeof(PositionColorVertex)
			}},
			.vertexAttributeCount = 2,
			.vertexAttributes = (SDL_GpuVertexAttribute[]){{
				.binding = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_VECTOR3,
				.location = 0,
				.offset = 0
			}, {
				.binding = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_COLOR,
				.location = 1,
				.offset = sizeof(float) * 3
			}}
		},
		.multisampleState.sampleMask = 0xFFFF,
		.primitiveType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
		.vertexShader = vertexShader,
		.fragmentShader = fragmentShader
	};

	Pipeline = SDL_GpuCreateGraphicsPipeline(context->Device, &pipelineCreateInfo);
	if (Pipeline == NULL)
	{
		SDL_Log("Failed to create pipeline!");
		return -1;
	}

	SDL_GpuReleaseShader(context->Device, vertexShader);
	SDL_GpuReleaseShader(context->Device, fragmentShader);

	// Create the buffers
	VertexBuffer = SDL_GpuCreateBuffer(
		context->Device,
		SDL_GPU_BUFFERUSAGE_VERTEX_BIT,
		sizeof(PositionColorVertex) * 6
	);

	DrawBuffer = SDL_GpuCreateBuffer(
		context->Device,
		SDL_GPU_BUFFERUSAGE_INDIRECT_BIT,
		sizeof(SDL_GpuIndirectDrawCommand) * 2
	);

	// Set the buffer data
	SDL_GpuTransferBuffer* transferBuffer = SDL_GpuCreateTransferBuffer(
		context->Device,
		SDL_GPU_TRANSFERUSAGE_BUFFER,
		SDL_GPU_TRANSFER_MAP_WRITE,
		(sizeof(PositionColorVertex) * 6) + (sizeof(SDL_GpuIndirectDrawCommand) * 2)
	);

	PositionColorVertex* transferData;
	SDL_GpuMapTransferBuffer(
		context->Device,
		transferBuffer,
		SDL_FALSE,
		(void**) &transferData
	);

	transferData[0] = (PositionColorVertex) { -0.5f,  1, 0,	 255,   0,   0, 255 };
	transferData[1] = (PositionColorVertex) {	-1, -1, 0,	   0, 255,   0, 255 };
	transferData[2] = (PositionColorVertex) {	 0, -1, 0,	   0,   0, 255, 255 };
	transferData[3] = (PositionColorVertex) {  0.5f,  1, 0,	 255,   0,   0, 255 };
	transferData[4] = (PositionColorVertex) {	 1, -1, 0,	   0, 255,   0, 255 };
	transferData[5] = (PositionColorVertex) {	 0, -1, 0,	   0,   0, 255, 255 };

	SDL_GpuIndirectDrawCommand* drawCommands = (SDL_GpuIndirectDrawCommand*) &transferData[6];
	drawCommands[0] = (SDL_GpuIndirectDrawCommand) { 3, 1, 0, 0 };
	drawCommands[1] = (SDL_GpuIndirectDrawCommand) { 3, 1, 3, 0 };

	SDL_GpuUnmapTransferBuffer(context->Device, transferBuffer);

	// Upload the transfer data to the GPU buffers
	SDL_GpuCommandBuffer* uploadCmdBuf = SDL_GpuAcquireCommandBuffer(context->Device);
	SDL_GpuCopyPass* copyPass = SDL_GpuBeginCopyPass(uploadCmdBuf);

	SDL_GpuUploadToBuffer(
		copyPass,
		transferBuffer,
		VertexBuffer,
		&(SDL_GpuBufferCopy) {
			.srcOffset = 0,
			.dstOffset = 0,
			.size = sizeof(PositionColorVertex) * 6
		},
		SDL_FALSE
	);

	SDL_GpuUploadToBuffer(
		copyPass,
		transferBuffer,
		DrawBuffer,
		&(SDL_GpuBufferCopy) {
			.srcOffset = sizeof(PositionColorVertex) * 6,
			.dstOffset = 0,
			.size = sizeof(SDL_GpuIndirectDrawCommand) * 2
		},
		SDL_FALSE
	);

	SDL_GpuEndCopyPass(copyPass);
	SDL_GpuSubmit(uploadCmdBuf);
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
		SDL_Log("GpuAcquireCommandBuffer failed");
		return -1;
	}

	Uint32 w, h;
	SDL_GpuTexture* swapchainTexture = SDL_GpuAcquireSwapchainTexture(cmdbuf, context->Window, &w, &h);
	if (swapchainTexture != NULL)
	{
		SDL_GpuColorAttachmentInfo colorAttachmentInfo = { 0 };
		colorAttachmentInfo.textureSlice.texture = swapchainTexture;
		colorAttachmentInfo.clearColor = (SDL_GpuColor){ 0.0f, 0.0f, 0.0f, 1.0f };
		colorAttachmentInfo.loadOp = SDL_GPU_LOADOP_CLEAR;
		colorAttachmentInfo.storeOp = SDL_GPU_STOREOP_STORE;

		SDL_GpuRenderPass* renderPass = SDL_GpuBeginRenderPass(cmdbuf, &colorAttachmentInfo, 1, NULL);

		SDL_GpuBindGraphicsPipeline(renderPass, Pipeline);
		SDL_GpuBindVertexBuffers(renderPass, 0, &(SDL_GpuBufferBinding){ .buffer = VertexBuffer, .offset = 0 }, 1);
		SDL_GpuDrawPrimitivesIndirect(renderPass, DrawBuffer, 0, 2, sizeof(SDL_GpuIndirectDrawCommand));

		SDL_GpuEndRenderPass(renderPass);
	}

	SDL_GpuSubmit(cmdbuf);

	return 0;
}

static void Quit(Context* context)
{
	SDL_GpuReleaseGraphicsPipeline(context->Device, Pipeline);
	SDL_GpuReleaseBuffer(context->Device, VertexBuffer);
	SDL_GpuReleaseBuffer(context->Device, DrawBuffer);

	CommonQuit(context);
}

Example DrawIndirect_Example = { "DrawIndirect", Init, Update, Draw, Quit };
