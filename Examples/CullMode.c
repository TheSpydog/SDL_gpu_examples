#include "Common.h"

static const char* ModeNames[] =
{
	"CW_CullNone",
	"CW_CullFront",
	"CW_CullBack",
	"CCW_CullNone",
	"CCW_CullFront",
	"CCW_CullBack"
};

static SDL_GPUGraphicsPipeline* Pipelines[SDL_arraysize(ModeNames)];
static int CurrentMode = 0;

static SDL_GPUBuffer* VertexBufferCW;
static SDL_GPUBuffer* VertexBufferCCW;

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

	// Create the pipelines
	SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {
		.target_info = {
			.num_color_targets = 1,
			.color_target_descriptions = (SDL_GPUColorTargetDescription[]){{
				.format = SDL_GetGPUSwapchainTextureFormat(context->Device, context->Window)
			}},
		},
		.vertex_input_state = (SDL_GPUVertexInputState){
			.num_vertex_bindings = 1,
			.vertex_bindings = (SDL_GPUVertexBinding[]){{
				.index = 0,
				.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
				.instance_step_rate = 0,
				.pitch = sizeof(PositionColorVertex)
			}},
			.num_vertex_attributes = 2,
			.vertex_attributes = (SDL_GPUVertexAttribute[]){{
				.binding_index = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
				.location = 0,
				.offset = 0
			}, {
				.binding_index = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,
				.location = 1,
				.offset = sizeof(float) * 3
			}}
		},
		.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
		.vertex_shader = vertexShader,
		.fragment_shader = fragmentShader,
	};

	for (int i = 0; i < SDL_arraysize(Pipelines); i += 1)
	{
		pipelineCreateInfo.rasterizer_state.cull_mode = (SDL_GPUCullMode) (i % 3);
		pipelineCreateInfo.rasterizer_state.front_face = (i > 2) ?
			SDL_GPU_FRONTFACE_CLOCKWISE :
			SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

		Pipelines[i] = SDL_CreateGPUGraphicsPipeline(context->Device, &pipelineCreateInfo);
		if (Pipelines[i] == NULL)
		{
			SDL_Log("Failed to create pipeline!");
			return -1;
		}
	}

	// Clean up shader resources
	SDL_ReleaseGPUShader(context->Device, vertexShader);
	SDL_ReleaseGPUShader(context->Device, fragmentShader);

	// Create the vertex buffers. They're the same except for the vertex order.
	// FIXME: Needs error handling!
	VertexBufferCW = SDL_CreateGPUBuffer(
		context->Device,
		&(SDL_GPUBufferCreateInfo) {
			.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
			.size = sizeof(PositionColorVertex) * 3
		}
	);
	VertexBufferCCW = SDL_CreateGPUBuffer(
		context->Device,
		&(SDL_GPUBufferCreateInfo) {
			.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
			.size = sizeof(PositionColorVertex) * 3
		}
	);

	// Set up the transfer buffer
	SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(
		context->Device,
		&(SDL_GPUTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = sizeof(PositionColorVertex) * 6
		}
	);

	PositionColorVertex* transferData = SDL_MapGPUTransferBuffer(
		context->Device,
		transferBuffer,
		SDL_FALSE
	);

	transferData[0] = (PositionColorVertex) {    -1,    -1, 0, 255,   0,   0, 255 };
	transferData[1] = (PositionColorVertex) {     1,    -1, 0,   0, 255,   0, 255 };
	transferData[2] = (PositionColorVertex) {     0,     1, 0,   0,   0, 255, 255 };
	transferData[3] = (PositionColorVertex) {     0,     1, 0, 255,   0,   0, 255 };
	transferData[4] = (PositionColorVertex) {     1,    -1, 0,   0, 255,   0, 255 };
	transferData[5] = (PositionColorVertex) {    -1,    -1, 0,   0,   0, 255, 255 };

	SDL_UnmapGPUTransferBuffer(context->Device, transferBuffer);

	// Upload the transfer data to the vertex buffer
	SDL_GPUCommandBuffer* uploadCmdBuf = SDL_AcquireGPUCommandBuffer(context->Device);
	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);

	SDL_UploadToGPUBuffer(
		copyPass,
		&(SDL_GPUTransferBufferLocation) {
			.transfer_buffer = transferBuffer,
			.offset = 0
		},
		&(SDL_GPUBufferRegion) {
			.buffer = VertexBufferCW,
			.offset = 0,
			.size = sizeof(PositionColorVertex) * 3
		},
		SDL_FALSE
	);
	SDL_UploadToGPUBuffer(
		copyPass,
		&(SDL_GPUTransferBufferLocation) {
			.transfer_buffer = transferBuffer,
			.offset = sizeof(PositionColorVertex) * 3
		},
		&(SDL_GPUBufferRegion) {
			.buffer = VertexBufferCCW,
			.offset = 0,
			.size = sizeof(PositionColorVertex) * 3
		},
		SDL_FALSE
	);

	SDL_EndGPUCopyPass(copyPass);
	SDL_SubmitGPUCommandBuffer(uploadCmdBuf);
	SDL_ReleaseGPUTransferBuffer(context->Device, transferBuffer);

	// Finally, print instructions!
	SDL_Log("Press Left/Right to switch between modes");
	SDL_Log("Current Mode: %s", ModeNames[0]);

	return 0;
}

static int Update(Context* context)
{
	if (context->LeftPressed)
	{
		CurrentMode -= 1;
		if (CurrentMode < 0)
		{
			CurrentMode = SDL_arraysize(Pipelines) - 1;
		}
		SDL_Log("Current Mode: %s", ModeNames[CurrentMode]);
	}

	if (context->RightPressed)
	{
		CurrentMode = (CurrentMode + 1) % SDL_arraysize(Pipelines);
		SDL_Log("Current Mode: %s", ModeNames[CurrentMode]);
	}

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
		SDL_BindGPUGraphicsPipeline(renderPass, Pipelines[CurrentMode]);
		SDL_SetGPUViewport(renderPass, &(SDL_GPUViewport){ 0, 0, 320, 480 });
		SDL_BindGPUVertexBuffers(renderPass, 0, &(SDL_GPUBufferBinding){ .buffer = VertexBufferCW, .offset = 0 }, 1);
		SDL_DrawGPUPrimitives(renderPass, 3, 1, 0, 0);
		SDL_SetGPUViewport(renderPass, &(SDL_GPUViewport){ 320, 0, 320, 480 });
		SDL_BindGPUVertexBuffers(renderPass, 0, &(SDL_GPUBufferBinding){ .buffer = VertexBufferCCW, .offset = 0 }, 1);
		SDL_DrawGPUPrimitives(renderPass, 3, 1, 0, 0);
		SDL_EndGPURenderPass(renderPass);
	}

	SDL_SubmitGPUCommandBuffer(cmdbuf);

	return 0;
}

static void Quit(Context* context)
{
	for (int i = 0; i < SDL_arraysize(Pipelines); i += 1)
	{
		SDL_ReleaseGPUGraphicsPipeline(context->Device, Pipelines[i]);
	}

	SDL_ReleaseGPUBuffer(context->Device, VertexBufferCW);
	SDL_ReleaseGPUBuffer(context->Device, VertexBufferCCW);

	CurrentMode = 0;

	CommonQuit(context);
}

Example CullMode_Example = { "CullMode", Init, Update, Draw, Quit };
