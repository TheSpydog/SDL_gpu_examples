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
		SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET
	))
	{
		depthStencilFormat = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
	}
	else if (SDL_GPUTextureSupportsFormat(
		context->Device,
		SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT,
		SDL_GPU_TEXTURETYPE_2D,
		SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET
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
		.target_info = {
			.num_color_targets = 1,
			.color_target_descriptions = (SDL_GPUColorTargetDescription[]){{
				.format = SDL_GetGPUSwapchainTextureFormat(context->Device, context->Window)
			}},
			.has_depth_stencil_target = true,
			.depth_stencil_format = depthStencilFormat
		},
		.depth_stencil_state = (SDL_GPUDepthStencilState){
			.enable_stencil_test = true,
			.front_stencil_state = (SDL_GPUStencilOpState){
				.compare_op = SDL_GPU_COMPAREOP_NEVER,
				.fail_op = SDL_GPU_STENCILOP_REPLACE,
				.pass_op = SDL_GPU_STENCILOP_KEEP,
				.depth_fail_op = SDL_GPU_STENCILOP_KEEP,
			},
			.back_stencil_state = (SDL_GPUStencilOpState){
				.compare_op = SDL_GPU_COMPAREOP_NEVER,
				.fail_op = SDL_GPU_STENCILOP_REPLACE,
				.pass_op = SDL_GPU_STENCILOP_KEEP,
				.depth_fail_op = SDL_GPU_STENCILOP_KEEP,
			},
			.write_mask = 0xFF
		},
		.rasterizer_state = (SDL_GPURasterizerState){
			.cull_mode = SDL_GPU_CULLMODE_NONE,
			.fill_mode = SDL_GPU_FILLMODE_FILL,
			.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE
		},
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

	MaskerPipeline = SDL_CreateGPUGraphicsPipeline(context->Device, &pipelineCreateInfo);
	if (MaskerPipeline == NULL)
	{
		SDL_Log("Failed to create masker pipeline!");
		return -1;
	}

	pipelineCreateInfo.depth_stencil_state = (SDL_GPUDepthStencilState){
		.enable_stencil_test = true,
		.front_stencil_state = (SDL_GPUStencilOpState){
			.compare_op = SDL_GPU_COMPAREOP_EQUAL,
			.fail_op = SDL_GPU_STENCILOP_KEEP,
			.pass_op = SDL_GPU_STENCILOP_KEEP,
			.depth_fail_op = SDL_GPU_STENCILOP_KEEP,
		},
		.back_stencil_state = (SDL_GPUStencilOpState){
			.compare_op = SDL_GPU_COMPAREOP_NEVER,
			.fail_op = SDL_GPU_STENCILOP_KEEP,
			.pass_op = SDL_GPU_STENCILOP_KEEP,
			.depth_fail_op = SDL_GPU_STENCILOP_KEEP,
		},
		.compare_mask = 0xFF,
		.write_mask = 0
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
			.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
			.size = sizeof(PositionColorVertex) * 6
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
			.layer_count_or_depth = 1,
			.num_levels = 1,
			.sample_count = SDL_GPU_SAMPLECOUNT_1,
			.format = depthStencilFormat,
			.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET
		}
	);

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
		false
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
			.transfer_buffer = transferBuffer,
			.offset = 0
		},
		&(SDL_GPUBufferRegion) {
			.buffer = VertexBuffer,
			.offset = 0,
			.size = sizeof(PositionColorVertex) * 6
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
        SDL_Log("AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return -1;
    }

    SDL_GPUTexture* swapchainTexture;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdbuf, context->Window, &swapchainTexture, NULL, NULL)) {
        SDL_Log("WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
        return -1;
    }

	if (swapchainTexture != NULL)
	{
		SDL_GPUColorTargetInfo colorTargetInfo = { 0 };
		colorTargetInfo.texture = swapchainTexture;
		colorTargetInfo.clear_color = (SDL_FColor){ 0.0f, 0.0f, 0.0f, 1.0f };
		colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
		colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

		SDL_GPUDepthStencilTargetInfo depthStencilTargetInfo = { 0 };
		depthStencilTargetInfo.texture = DepthStencilTexture;
		depthStencilTargetInfo.cycle = true;
		depthStencilTargetInfo.clear_depth = 0;
		depthStencilTargetInfo.clear_stencil = 0;
		depthStencilTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
		depthStencilTargetInfo.store_op = SDL_GPU_STOREOP_DONT_CARE;
		depthStencilTargetInfo.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
		depthStencilTargetInfo.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

		SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(
			cmdbuf,
			&colorTargetInfo,
			1,
			&depthStencilTargetInfo
		);

		SDL_BindGPUVertexBuffers(renderPass, 0, &(SDL_GPUBufferBinding){.buffer = VertexBuffer, .offset = 0 }, 1);

		SDL_SetGPUStencilReference(renderPass, 1);
		SDL_BindGPUGraphicsPipeline(renderPass, MaskerPipeline);
		SDL_DrawGPUPrimitives(renderPass, 3, 1, 0, 0);

		SDL_SetGPUStencilReference(renderPass, 0);
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
