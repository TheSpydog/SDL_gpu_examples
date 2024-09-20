#include "Common.h"

static SDL_GPUGraphicsPipeline* Pipeline;
static SDL_GPUBuffer* VertexBuffer;
static SDL_GPUBuffer* IndexBuffer;
static SDL_GPUBuffer* DrawBuffer;

static int Init(Context* context)
{
	int result = CommonInit(context, 0);
	if (result < 0)
	{
		return result;
	}

	// Create the shaders
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

	// Create the pipeline
	SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {
		.target_info = {
			.num_color_targets = 1,
			.color_target_descriptions = (SDL_GPUColorTargetDescription[]){{
				.format = SDL_GetGPUSwapchainTextureFormat(context->Device, context->Window)
			}},
		},
		// This is set up to match the vertex shader layout!
		.vertex_input_state = (SDL_GPUVertexInputState){
			.num_vertex_buffers = 1,
			.vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]){{
				.slot = 0,
				.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
				.instance_step_rate = 0,
				.pitch = sizeof(PositionColorVertex)
			}},
			.num_vertex_attributes = 2,
			.vertex_attributes = (SDL_GPUVertexAttribute[]){{
				.buffer_slot = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
				.location = 0,
				.offset = 0
			}, {
				.buffer_slot = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,
				.location = 1,
				.offset = sizeof(float) * 3
			}}
		},
		.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
		.vertex_shader = vertexShader,
		.fragment_shader = fragmentShader
	};

	Pipeline = SDL_CreateGPUGraphicsPipeline(context->Device, &pipelineCreateInfo);
	if (Pipeline == NULL)
	{
		SDL_Log("Failed to create pipeline!");
		return -1;
	}

	SDL_ReleaseGPUShader(context->Device, vertexShader);
	SDL_ReleaseGPUShader(context->Device, fragmentShader);

	// Create the buffers
	const Uint32 vertexBufferSize = sizeof(PositionColorVertex) * 10;
	VertexBuffer = SDL_CreateGPUBuffer(
		context->Device,
		&(SDL_GPUBufferCreateInfo) {
			.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
			.size = vertexBufferSize
		}
	);

	const Uint32 indexBufferSize = sizeof(Uint16) * 6;
	IndexBuffer = SDL_CreateGPUBuffer(
		context->Device,
		&(SDL_GPUBufferCreateInfo) {
			.usage = SDL_GPU_BUFFERUSAGE_INDEX,
			.size = indexBufferSize
		}
	);

	const Uint32 drawBufferSize = (sizeof(SDL_GPUIndexedIndirectDrawCommand) * 1) + (sizeof(SDL_GPUIndirectDrawCommand) * 2);
	DrawBuffer = SDL_CreateGPUBuffer(
		context->Device,
		&(SDL_GPUBufferCreateInfo) {
			.usage = SDL_GPU_BUFFERUSAGE_INDIRECT,
			.size = drawBufferSize
		}
	);

	// Set the buffer data
	SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(
		context->Device,
		&(SDL_GPUTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = vertexBufferSize + indexBufferSize + drawBufferSize
		}
	);

	PositionColorVertex* transferData = SDL_MapGPUTransferBuffer(
		context->Device,
		transferBuffer,
		false
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

	SDL_GPUIndexedIndirectDrawCommand* indexedDrawCommand = (SDL_GPUIndexedIndirectDrawCommand*) &indexData[6];
	indexedDrawCommand[0] = (SDL_GPUIndexedIndirectDrawCommand) { 6, 1, 0, 0, 0 };

	SDL_GPUIndirectDrawCommand* drawCommands = (SDL_GPUIndirectDrawCommand*) &indexedDrawCommand[1];
	drawCommands[0] = (SDL_GPUIndirectDrawCommand) { 3, 1, 4, 0 };
	drawCommands[1] = (SDL_GPUIndirectDrawCommand) { 3, 1, 7, 0 };

	SDL_UnmapGPUTransferBuffer(context->Device, transferBuffer);

	// Upload the transfer data to the GPU buffers
	SDL_GPUCommandBuffer* uploadCmdBuf = SDL_AcquireGPUCommandBuffer(context->Device);
	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);

	SDL_UploadToGPUBuffer(
		copyPass,
		&(SDL_GPUTransferBufferLocation) {
			.transfer_buffer = transferBuffer,
			.offset = 0
		},
		&(SDL_GPUBufferRegion) {
			.buffer = VertexBuffer,
			.offset = 0,
			.size = vertexBufferSize
		},
		false
	);

	SDL_UploadToGPUBuffer(
		copyPass,
		&(SDL_GPUTransferBufferLocation) {
			.transfer_buffer = transferBuffer,
			.offset = vertexBufferSize
		},
		&(SDL_GPUBufferRegion) {
			.buffer = IndexBuffer,
			.offset = 0,
			.size = indexBufferSize
		},
		false
	);

	SDL_UploadToGPUBuffer(
		copyPass,
		&(SDL_GPUTransferBufferLocation) {
			.transfer_buffer = transferBuffer,
			.offset = vertexBufferSize + indexBufferSize
		},
		&(SDL_GPUBufferRegion) {
			.buffer = DrawBuffer,
			.offset = 0,
			.size = drawBufferSize
		},
		false
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
		SDL_GPUColorTargetInfo colorTargetInfo = { 0 };
		colorTargetInfo.texture = swapchainTexture;
		colorTargetInfo.clear_color = (SDL_FColor){ 0.0f, 0.0f, 0.0f, 1.0f };
		colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
		colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

		SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTargetInfo, 1, NULL);

		SDL_BindGPUGraphicsPipeline(renderPass, Pipeline);
		SDL_BindGPUVertexBuffers(renderPass, 0, &(SDL_GPUBufferBinding){ .buffer = VertexBuffer, .offset = 0 }, 1);
		SDL_BindGPUIndexBuffer(renderPass, &(SDL_GPUBufferBinding) {.buffer = IndexBuffer }, SDL_GPU_INDEXELEMENTSIZE_16BIT);
		SDL_DrawGPUIndexedPrimitivesIndirect(renderPass, DrawBuffer, 0, 1);
		SDL_DrawGPUPrimitivesIndirect(renderPass, DrawBuffer, sizeof(SDL_GPUIndexedIndirectDrawCommand), 2);

		SDL_EndGPURenderPass(renderPass);
	}

	SDL_SubmitGPUCommandBuffer(cmdbuf);

	return 0;
}

static void Quit(Context* context)
{
	SDL_ReleaseGPUGraphicsPipeline(context->Device, Pipeline);
	SDL_ReleaseGPUBuffer(context->Device, VertexBuffer);
	SDL_ReleaseGPUBuffer(context->Device, IndexBuffer);
	SDL_ReleaseGPUBuffer(context->Device, DrawBuffer);

	CommonQuit(context);
}

Example DrawIndirect_Example = { "DrawIndirect", Init, Update, Draw, Quit };
