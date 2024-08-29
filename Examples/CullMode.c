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

	// Create the pipelines
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
		.fragmentShader = fragmentShader,
	};

	for (int i = 0; i < SDL_arraysize(Pipelines); i += 1)
	{
		pipelineCreateInfo.rasterizerState.cullMode = (SDL_GpuCullMode) (i % 3);
		pipelineCreateInfo.rasterizerState.frontFace = (i > 2) ?
			SDL_GPU_FRONTFACE_CLOCKWISE :
			SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

		Pipelines[i] = SDL_CreateGpuGraphicsPipeline(context->Device, &pipelineCreateInfo);
		if (Pipelines[i] == NULL)
		{
			SDL_Log("Failed to create pipeline!");
			return -1;
		}
	}

	// Clean up shader resources
	SDL_ReleaseGpuShader(context->Device, vertexShader);
	SDL_ReleaseGpuShader(context->Device, fragmentShader);

	// Create the vertex buffers. They're the same except for the vertex order.
	// FIXME: Needs error handling!
	VertexBufferCW = SDL_CreateGpuBuffer(
		context->Device,
		&(SDL_GpuBufferCreateInfo) {
			.usageFlags = SDL_GPU_BUFFERUSAGE_VERTEX_BIT,
			.sizeInBytes = sizeof(PositionColorVertex) * 3
		}
	);
	VertexBufferCCW = SDL_CreateGpuBuffer(
		context->Device,
		&(SDL_GpuBufferCreateInfo) {
			.usageFlags = SDL_GPU_BUFFERUSAGE_VERTEX_BIT,
			.sizeInBytes = sizeof(PositionColorVertex) * 3
		}
	);

	// Set up the transfer buffer
	SDL_GpuTransferBuffer* transferBuffer = SDL_CreateGpuTransferBuffer(
		context->Device,
		&(SDL_GpuTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.sizeInBytes = sizeof(PositionColorVertex) * 6
		}
	);

	PositionColorVertex* transferData = SDL_MapGpuTransferBuffer(
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

	SDL_UnmapGpuTransferBuffer(context->Device, transferBuffer);

	// Upload the transfer data to the vertex buffer
	SDL_GpuCommandBuffer* uploadCmdBuf = SDL_AcquireGpuCommandBuffer(context->Device);
	SDL_GpuCopyPass* copyPass = SDL_BeginGpuCopyPass(uploadCmdBuf);

	SDL_UploadToGpuBuffer(
		copyPass,
		&(SDL_GpuTransferBufferLocation) {
			.transferBuffer = transferBuffer,
			.offset = 0
		},
		&(SDL_GpuBufferRegion) {
			.buffer = VertexBufferCW,
			.offset = 0,
			.size = sizeof(PositionColorVertex) * 3
		},
		SDL_FALSE
	);
	SDL_UploadToGpuBuffer(
		copyPass,
		&(SDL_GpuTransferBufferLocation) {
			.transferBuffer = transferBuffer,
			.offset = sizeof(PositionColorVertex) * 3
		},
		&(SDL_GpuBufferRegion) {
			.buffer = VertexBufferCCW,
			.offset = 0,
			.size = sizeof(PositionColorVertex) * 3
		},
		SDL_FALSE
	);

	SDL_EndGpuCopyPass(copyPass);
	SDL_SubmitGpu(uploadCmdBuf);
	SDL_ReleaseGpuTransferBuffer(context->Device, transferBuffer);

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
		SDL_BindGpuGraphicsPipeline(renderPass, Pipelines[CurrentMode]);
		SDL_SetGpuViewport(renderPass, &(SDL_GpuViewport){ 0, 0, 320, 480 });
		SDL_BindGpuVertexBuffers(renderPass, 0, &(SDL_GpuBufferBinding){ .buffer = VertexBufferCW, .offset = 0 }, 1);
		SDL_DrawGpuPrimitives(renderPass, 3, 1, 0, 0);
		SDL_SetGpuViewport(renderPass, &(SDL_GpuViewport){ 320, 0, 320, 480 });
		SDL_BindGpuVertexBuffers(renderPass, 0, &(SDL_GpuBufferBinding){ .buffer = VertexBufferCCW, .offset = 0 }, 1);
		SDL_DrawGpuPrimitives(renderPass, 3, 1, 0, 0);
		SDL_EndGpuRenderPass(renderPass);
	}

	SDL_SubmitGpu(cmdbuf);

	return 0;
}

static void Quit(Context* context)
{
	for (int i = 0; i < SDL_arraysize(Pipelines); i += 1)
	{
		SDL_ReleaseGpuGraphicsPipeline(context->Device, Pipelines[i]);
	}

	SDL_ReleaseGpuBuffer(context->Device, VertexBufferCW);
	SDL_ReleaseGpuBuffer(context->Device, VertexBufferCCW);

	CurrentMode = 0;

	CommonQuit(context);
}

Example CullMode_Example = { "CullMode", Init, Update, Draw, Quit };
