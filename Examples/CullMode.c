#include <SDL_gpu_examples.h>

static const char* ModeNames[] =
{
	"CW_CullNone",
	"CW_CullFront",
	"CW_CullBack",
	"CCW_CullNone",
	"CCW_CullFront",
	"CCW_CullBack"
};

static SDL_GpuGraphicsPipeline* Pipelines[SDL_arraysize(ModeNames)];
static int CurrentMode = 0;

static SDL_GpuBuffer* VertexBufferCW;
static SDL_GpuBuffer* VertexBufferCCW;

static int Init(Context* context)
{
	int result = CommonInit(context, 0);
	if (result < 0)
	{
		return result;
	}

	// Create the shaders
	SDL_GpuShader* vertexShader = LoadShader(context->Device, "PositionColor.vert");
	if (vertexShader == NULL)
	{
		SDL_Log("Failed to create vertex shader!");
		return -1;
	}

	SDL_GpuShader* fragmentShader = LoadShader(context->Device, "SolidColor.frag");
	if (fragmentShader == NULL)
	{
		SDL_Log("Failed to create fragment shader!");
		return -1;
	}

	// Create the pipelines
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
		.fragmentShader = fragmentShader,
	};

	for (int i = 0; i < SDL_arraysize(Pipelines); i += 1)
	{
		pipelineCreateInfo.rasterizerState.cullMode = (SDL_GpuCullMode) (i % 3);
		pipelineCreateInfo.rasterizerState.frontFace = (i > 2) ?
			SDL_GPU_FRONTFACE_CLOCKWISE :
			SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

		Pipelines[i] = SDL_GpuCreateGraphicsPipeline(context->Device, &pipelineCreateInfo);
		if (Pipelines[i] == NULL)
		{
			SDL_Log("Failed to create pipeline!");
			return -1;
		}
	}

	// Clean up shader resources
	SDL_GpuReleaseShader(context->Device, vertexShader);
	SDL_GpuReleaseShader(context->Device, fragmentShader);

	// Create the vertex buffers. They're the same except for the vertex order.
	// FIXME: Needs error handling!
	VertexBufferCW = SDL_GpuCreateBuffer(
		context->Device,
		SDL_GPU_BUFFERUSAGE_VERTEX_BIT,
		sizeof(PositionColorVertex) * 3
	);
	VertexBufferCCW = SDL_GpuCreateBuffer(
		context->Device,
		SDL_GPU_BUFFERUSAGE_VERTEX_BIT,
		sizeof(PositionColorVertex) * 3
	);

	// Set up the transfer buffer
	SDL_GpuTransferBuffer* transferBuffer = SDL_GpuCreateTransferBuffer(
		context->Device,
		SDL_GPU_TRANSFERUSAGE_BUFFER,
		SDL_GPU_TRANSFER_MAP_WRITE,
		sizeof(PositionColorVertex) * 6
	);

	PositionColorVertex* transferData;
	SDL_GpuMapTransferBuffer(
		context->Device,
		transferBuffer,
		SDL_FALSE,
		(void**) &transferData
	);

	transferData[0] = (PositionColorVertex) {    -1,    -1, 0, 255,   0,   0, 255 };
	transferData[1] = (PositionColorVertex) {     1,    -1, 0,   0, 255,   0, 255 };
	transferData[2] = (PositionColorVertex) {     0,     1, 0,   0,   0, 255, 255 };
	transferData[3] = (PositionColorVertex) {     0,     1, 0, 255,   0,   0, 255 };
	transferData[4] = (PositionColorVertex) {     1,    -1, 0,   0, 255,   0, 255 };
	transferData[5] = (PositionColorVertex) {    -1,    -1, 0,   0,   0, 255, 255 };

	SDL_GpuUnmapTransferBuffer(context->Device, transferBuffer);

	// Upload the transfer data to the vertex buffer
	SDL_GpuCommandBuffer* uploadCmdBuf = SDL_GpuAcquireCommandBuffer(context->Device);
	SDL_GpuCopyPass* copyPass = SDL_GpuBeginCopyPass(uploadCmdBuf);

	SDL_GpuUploadToBuffer(
		copyPass,
		transferBuffer,
		VertexBufferCW,
		&(SDL_GpuBufferCopy) {
			.srcOffset = 0,
			.dstOffset = 0,
			.size = sizeof(PositionColorVertex) * 3
		},
		SDL_FALSE
	);
	SDL_GpuUploadToBuffer(
		copyPass,
		transferBuffer,
		VertexBufferCCW,
		&(SDL_GpuBufferCopy) {
			.srcOffset = sizeof(PositionColorVertex) * 3,
			.dstOffset = 0,
			.size = sizeof(PositionColorVertex) * 3
		},
		SDL_FALSE
	);

	SDL_GpuEndCopyPass(copyPass);
	SDL_GpuSubmit(uploadCmdBuf);
	SDL_GpuReleaseTransferBuffer(context->Device, transferBuffer);

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
		SDL_GpuBindGraphicsPipeline(renderPass, Pipelines[CurrentMode]);
		SDL_GpuSetViewport(renderPass, &(SDL_GpuViewport){ 0, 0, 320, 480 });
		SDL_GpuBindVertexBuffers(renderPass, 0, &(SDL_GpuBufferBinding){ .buffer = VertexBufferCW, .offset = 0 }, 1);
		SDL_GpuDrawPrimitives(renderPass, 0, 1);
		SDL_GpuSetViewport(renderPass, &(SDL_GpuViewport){ 320, 0, 320, 480 });
		SDL_GpuBindVertexBuffers(renderPass, 0, &(SDL_GpuBufferBinding){ .buffer = VertexBufferCCW, .offset = 0 }, 1);
		SDL_GpuDrawPrimitives(renderPass, 0, 1);
		SDL_GpuEndRenderPass(renderPass);
	}

	SDL_GpuSubmit(cmdbuf);

	return 0;
}

static void Quit(Context* context)
{
	for (int i = 0; i < SDL_arraysize(Pipelines); i += 1)
	{
		SDL_GpuReleaseGraphicsPipeline(context->Device, Pipelines[i]);
	}

	SDL_GpuReleaseBuffer(context->Device, VertexBufferCW);
	SDL_GpuReleaseBuffer(context->Device, VertexBufferCCW);

	CurrentMode = 0;

	CommonQuit(context);
}

Example CullMode_Example = { "CullMode", Init, Update, Draw, Quit };
