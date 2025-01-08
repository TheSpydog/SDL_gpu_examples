#include "Common.h"

static SDL_GPUGraphicsPipeline* ScenePipeline;
static SDL_GPUBuffer* SceneVertexBuffer;
static SDL_GPUBuffer* SceneIndexBuffer;
static SDL_GPUTexture* SceneColorTexture;
static SDL_GPUTexture* SceneDepthTexture;

static SDL_GPUGraphicsPipeline* EffectPipeline;
static SDL_GPUBuffer* EffectVertexBuffer;
static SDL_GPUBuffer* EffectIndexBuffer;
static SDL_GPUSampler* EffectSampler;

static float Time;
static int SceneWidth, SceneHeight;

static int Init(Context* context)
{
	int result = CommonInit(context, 0);
	if (result < 0)
	{
		return result;
	}

	// Creates the Shaders & Pipelines
	{
		SDL_GPUShader* sceneVertexShader = LoadShader(context->Device, "PositionColorTransform.vert", 0, 1, 0, 0);
		if (sceneVertexShader == NULL)
		{
			SDL_Log("Failed to create 'PositionColorTransform' vertex shader!");
			return -1;
		}

		SDL_GPUShader* sceneFragmentShader = LoadShader(context->Device, "SolidColorDepth.frag", 0, 1, 0, 0);
		if (sceneFragmentShader == NULL)
		{
			SDL_Log("Failed to create 'SolidColorDepth' fragment shader!");
			return -1;
		}

		SDL_GPUShader* effectVertexShader = LoadShader(context->Device, "TexturedQuad.vert", 0, 0, 0, 0);
		if (effectVertexShader == NULL)
		{
			SDL_Log("Failed to create 'TexturedQuad' vertex shader!");
			return -1;
		}

		SDL_GPUShader* effectFragmentShader = LoadShader(context->Device, "DepthOutline.frag", 2, 1, 0, 0);
		if (effectFragmentShader == NULL)
		{
			SDL_Log("Failed to create 'DepthOutline' fragment shader!");
			return -1;
		}

		SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {
			.target_info = {
				.num_color_targets = 1,
				.color_target_descriptions = (SDL_GPUColorTargetDescription[]){{
					.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM
				}},
				.has_depth_stencil_target = true,
				.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM
			},
			.depth_stencil_state = (SDL_GPUDepthStencilState){
				.enable_depth_test = true,
				.enable_depth_write = true,
				.enable_stencil_test = false,
				.compare_op = SDL_GPU_COMPAREOP_LESS,
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
			.vertex_shader = sceneVertexShader,
			.fragment_shader = sceneFragmentShader
		};

		ScenePipeline = SDL_CreateGPUGraphicsPipeline(context->Device, &pipelineCreateInfo);
		if (ScenePipeline == NULL)
		{
			SDL_Log("Failed to create Scene pipeline!");
			return -1;
		}

		pipelineCreateInfo = (SDL_GPUGraphicsPipelineCreateInfo){
			.target_info = {
				.num_color_targets = 1,
				.color_target_descriptions = (SDL_GPUColorTargetDescription[]){{
					.format = SDL_GetGPUSwapchainTextureFormat(context->Device, context->Window),
					.blend_state = (SDL_GPUColorTargetBlendState) {
						.enable_blend = true,
						.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
						.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
						.color_blend_op = SDL_GPU_BLENDOP_ADD,
						.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
						.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
						.alpha_blend_op = SDL_GPU_BLENDOP_ADD,
					}
				}},
			},
			.vertex_input_state = (SDL_GPUVertexInputState){
				.num_vertex_buffers = 1,
				.vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]){{
					.slot = 0,
					.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
					.instance_step_rate = 0,
					.pitch = sizeof(PositionTextureVertex)
				}},
				.num_vertex_attributes = 2,
				.vertex_attributes = (SDL_GPUVertexAttribute[]){{
					.buffer_slot = 0,
					.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
					.location = 0,
					.offset = 0
				}, {
					.buffer_slot = 0,
					.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
					.location = 1,
					.offset = sizeof(float) * 3
				}}
			},
			.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
			.vertex_shader = effectVertexShader,
			.fragment_shader = effectFragmentShader
		};

		EffectPipeline = SDL_CreateGPUGraphicsPipeline(context->Device, &pipelineCreateInfo);
		if (EffectPipeline == NULL)
		{
			SDL_Log("Failed to create Outline Effect pipeline!");
			return -1;
		}

		SDL_ReleaseGPUShader(context->Device, effectVertexShader);
		SDL_ReleaseGPUShader(context->Device, effectFragmentShader);

		SDL_ReleaseGPUShader(context->Device, sceneVertexShader);
		SDL_ReleaseGPUShader(context->Device, sceneFragmentShader);
	}

	// Create the Scene Textures
	{
		// Make them smaller so pixels stand out more
		int w, h;
		SDL_GetWindowSizeInPixels(context->Window, &w, &h);
		SceneWidth = w / 4;
		SceneHeight = h / 4;

		SceneColorTexture = SDL_CreateGPUTexture(
			context->Device,
			&(SDL_GPUTextureCreateInfo) {
				.type = SDL_GPU_TEXTURETYPE_2D,
				.width = SceneWidth,
				.height = SceneHeight,
				.layer_count_or_depth = 1,
				.num_levels = 1,
				.sample_count = SDL_GPU_SAMPLECOUNT_1,
				.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
				.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET
			}
		);

		SceneDepthTexture = SDL_CreateGPUTexture(
			context->Device,
			&(SDL_GPUTextureCreateInfo) {
				.type = SDL_GPU_TEXTURETYPE_2D,
				.width = SceneWidth,
				.height = SceneHeight,
				.layer_count_or_depth = 1,
				.num_levels = 1,
				.sample_count = SDL_GPU_SAMPLECOUNT_1,
				.format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
				.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET
			}
		);
	}

	// Create Outline Effect Sampler
	EffectSampler = SDL_CreateGPUSampler(context->Device, &(SDL_GPUSamplerCreateInfo){
		.min_filter = SDL_GPU_FILTER_NEAREST,
		.mag_filter = SDL_GPU_FILTER_NEAREST,
		.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
		.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
		.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
		.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
	});

	// Create & Upload Scene Index and Vertex Buffers
	{
		SceneVertexBuffer = SDL_CreateGPUBuffer(
			context->Device,
			&(SDL_GPUBufferCreateInfo) {
				.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
				.size = sizeof(PositionColorVertex) * 24
			}
		);

		SceneIndexBuffer = SDL_CreateGPUBuffer(
			context->Device,
			&(SDL_GPUBufferCreateInfo) {
				.usage = SDL_GPU_BUFFERUSAGE_INDEX,
				.size = sizeof(Uint16) * 36
			}
		);

		SDL_GPUTransferBuffer* bufferTransferBuffer = SDL_CreateGPUTransferBuffer(
			context->Device,
			&(SDL_GPUTransferBufferCreateInfo) {
				.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
				.size = (sizeof(PositionColorVertex) * 24) + (sizeof(Uint16) * 36)
			}
		);

		PositionColorVertex* transferData = SDL_MapGPUTransferBuffer(
			context->Device,
			bufferTransferBuffer,
			false
		);

		transferData[0] = (PositionColorVertex) { -10, -10, -10, 255, 0, 0, 255 };
		transferData[1] = (PositionColorVertex) { 10, -10, -10, 255, 0, 0, 255 };
		transferData[2] = (PositionColorVertex) { 10, 10, -10, 255, 0, 0, 255 };
		transferData[3] = (PositionColorVertex) { -10, 10, -10, 255, 0, 0, 255 };

		transferData[4] = (PositionColorVertex) { -10, -10, 10, 255, 255, 0, 255 };
		transferData[5] = (PositionColorVertex) { 10, -10, 10, 255, 255, 0, 255 };
		transferData[6] = (PositionColorVertex) { 10, 10, 10, 255, 255, 0, 255 };
		transferData[7] = (PositionColorVertex) { -10, 10, 10, 255, 255, 0, 255 };

		transferData[8] = (PositionColorVertex) { -10, -10, -10, 255, 0, 255, 255 };
		transferData[9] = (PositionColorVertex) { -10, 10, -10, 255, 0, 255, 255 };
		transferData[10] = (PositionColorVertex) { -10, 10, 10, 255, 0, 255, 255 };
		transferData[11] = (PositionColorVertex) { -10, -10, 10, 255, 0, 255, 255 };

		transferData[12] = (PositionColorVertex) { 10, -10, -10, 0, 255, 0, 255 };
		transferData[13] = (PositionColorVertex) { 10, 10, -10, 0, 255, 0, 255 };
		transferData[14] = (PositionColorVertex) { 10, 10, 10, 0, 255, 0, 255 };
		transferData[15] = (PositionColorVertex) { 10, -10, 10, 0, 255, 0, 255 };

		transferData[16] = (PositionColorVertex) { -10, -10, -10, 0, 255, 255, 255 };
		transferData[17] = (PositionColorVertex) { -10, -10, 10, 0, 255, 255, 255 };
		transferData[18] = (PositionColorVertex) { 10, -10, 10, 0, 255, 255, 255 };
		transferData[19] = (PositionColorVertex) { 10, -10, -10, 0, 255, 255, 255 };

		transferData[20] = (PositionColorVertex) { -10, 10, -10, 0, 0, 255, 255 };
		transferData[21] = (PositionColorVertex) { -10, 10, 10, 0, 0, 255, 255 };
		transferData[22] = (PositionColorVertex) { 10, 10, 10, 0, 0, 255, 255 };
		transferData[23] = (PositionColorVertex) { 10, 10, -10, 0, 0, 255, 255 };

		Uint16* indexData = (Uint16*) &transferData[24];
		Uint16 indices[] = {
			0, 1, 2, 0, 2, 3,
			4, 5, 6, 4, 6, 7,
			8, 9, 10, 8, 10, 11,
			12, 13, 14, 12, 14, 15,
			16, 17, 18, 16, 18, 19,
			20, 21, 22, 20, 22, 23
		};
		SDL_memcpy(indexData, indices, sizeof(indices));

		SDL_UnmapGPUTransferBuffer(context->Device, bufferTransferBuffer);

		// Upload the transfer data to the GPU buffers
		SDL_GPUCommandBuffer* uploadCmdBuf = SDL_AcquireGPUCommandBuffer(context->Device);
		SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);

		SDL_UploadToGPUBuffer(
			copyPass,
			&(SDL_GPUTransferBufferLocation) {
				.transfer_buffer = bufferTransferBuffer,
				.offset = 0
			},
			&(SDL_GPUBufferRegion) {
				.buffer = SceneVertexBuffer,
				.offset = 0,
				.size = sizeof(PositionColorVertex) * 24
			},
			false
		);

		SDL_UploadToGPUBuffer(
			copyPass,
			&(SDL_GPUTransferBufferLocation) {
				.transfer_buffer = bufferTransferBuffer,
				.offset = sizeof(PositionColorVertex) * 24
			},
			&(SDL_GPUBufferRegion) {
				.buffer = SceneIndexBuffer,
				.offset = 0,
				.size = sizeof(Uint16) * 36
			},
			false
		);

		SDL_EndGPUCopyPass(copyPass);
		SDL_SubmitGPUCommandBuffer(uploadCmdBuf);
		SDL_ReleaseGPUTransferBuffer(context->Device, bufferTransferBuffer);
	}

	// Create & Upload Outline Effect Vertex and Index buffers
	{
		EffectVertexBuffer = SDL_CreateGPUBuffer(
			context->Device,
			&(SDL_GPUBufferCreateInfo) {
				.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
				.size = sizeof(PositionTextureVertex) * 4
			}
		);

		EffectIndexBuffer = SDL_CreateGPUBuffer(
			context->Device,
			&(SDL_GPUBufferCreateInfo) {
				.usage = SDL_GPU_BUFFERUSAGE_INDEX,
				.size = sizeof(Uint16) * 6
			}
		);

		SDL_GPUTransferBuffer* bufferTransferBuffer = SDL_CreateGPUTransferBuffer(
			context->Device,
			&(SDL_GPUTransferBufferCreateInfo) {
				.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
				.size = (sizeof(PositionTextureVertex) * 4) + (sizeof(Uint16) * 6)
			}
		);

		PositionTextureVertex* transferData = SDL_MapGPUTransferBuffer(
			context->Device,
			bufferTransferBuffer,
			false
		);

		transferData[0] = (PositionTextureVertex) { -1,  1, 0, 0, 0 };
		transferData[1] = (PositionTextureVertex) {  1,  1, 0, 1, 0 };
		transferData[2] = (PositionTextureVertex) {  1, -1, 0, 1, 1 };
		transferData[3] = (PositionTextureVertex) { -1, -1, 0, 0, 1 };

		Uint16* indexData = (Uint16*) &transferData[4];
		indexData[0] = 0;
		indexData[1] = 1;
		indexData[2] = 2;
		indexData[3] = 0;
		indexData[4] = 2;
		indexData[5] = 3;

		SDL_UnmapGPUTransferBuffer(context->Device, bufferTransferBuffer);

		SDL_GPUCommandBuffer* uploadCmdBuf = SDL_AcquireGPUCommandBuffer(context->Device);
		SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);

		SDL_UploadToGPUBuffer(
			copyPass,
			&(SDL_GPUTransferBufferLocation) {
				.transfer_buffer = bufferTransferBuffer,
				.offset = 0
			},
			&(SDL_GPUBufferRegion) {
				.buffer = EffectVertexBuffer,
				.offset = 0,
				.size = sizeof(PositionTextureVertex) * 4
			},
			false
		);

		SDL_UploadToGPUBuffer(
			copyPass,
			&(SDL_GPUTransferBufferLocation) {
				.transfer_buffer = bufferTransferBuffer,
				.offset = sizeof(PositionTextureVertex) * 4
			},
			&(SDL_GPUBufferRegion) {
				.buffer = EffectIndexBuffer,
				.offset = 0,
				.size = sizeof(Uint16) * 6
			},
			false
		);

		SDL_EndGPUCopyPass(copyPass);
		SDL_SubmitGPUCommandBuffer(uploadCmdBuf);
		SDL_ReleaseGPUTransferBuffer(context->Device, bufferTransferBuffer);
	}

	Time = 0;
	return 0;
}

static int Update(Context* context)
{
	Time += context->DeltaTime;
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

	// Render the 3D Scene (Color and Depth pass)
	{
		float nearPlane = 20.0f;
		float farPlane = 60.0f;

		Matrix4x4 proj = Matrix4x4_CreatePerspectiveFieldOfView(
			75.0f * SDL_PI_F / 180.0f,
			SceneWidth / (float)SceneHeight,
			nearPlane,
			farPlane
		);
		Matrix4x4 view = Matrix4x4_CreateLookAt(
			(Vector3) { SDL_cosf(Time) * 30, 30, SDL_sinf(Time) * 30 },
			(Vector3) { 0, 0, 0 },
			(Vector3) { 0, 1, 0 }
		);

		Matrix4x4 viewproj = Matrix4x4_Multiply(view, proj);

		SDL_GPUColorTargetInfo colorTargetInfo = { 0 };
		colorTargetInfo.texture = SceneColorTexture;
		colorTargetInfo.clear_color = (SDL_FColor){ 0.0f, 0.0f, 0.0f, 0.0f };
		colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
		colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

		SDL_GPUDepthStencilTargetInfo depthStencilTargetInfo = { 0 };
		depthStencilTargetInfo.texture = SceneDepthTexture;
		depthStencilTargetInfo.cycle = true;
		depthStencilTargetInfo.clear_depth = 1;
		depthStencilTargetInfo.clear_stencil = 0;
		depthStencilTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
		depthStencilTargetInfo.store_op = SDL_GPU_STOREOP_STORE;
		depthStencilTargetInfo.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
		depthStencilTargetInfo.stencil_store_op = SDL_GPU_STOREOP_STORE;

		SDL_PushGPUVertexUniformData(cmdbuf, 0, &viewproj, sizeof(viewproj));
		SDL_PushGPUFragmentUniformData(cmdbuf, 0, (float[]) { nearPlane, farPlane }, 8);

		SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTargetInfo, 1, &depthStencilTargetInfo);
		SDL_BindGPUVertexBuffers(renderPass, 0, &(SDL_GPUBufferBinding){.buffer = SceneVertexBuffer, .offset = 0 }, 1);
		SDL_BindGPUIndexBuffer(renderPass, &(SDL_GPUBufferBinding){ .buffer = SceneIndexBuffer, .offset = 0 }, SDL_GPU_INDEXELEMENTSIZE_16BIT);
		SDL_BindGPUGraphicsPipeline(renderPass, ScenePipeline);
		SDL_DrawGPUIndexedPrimitives(renderPass, 36, 1, 0, 0, 0);
		SDL_EndGPURenderPass(renderPass);
	}

	// Render the Outline Effect that samples from the Color/Depth textures
	if (swapchainTexture != NULL)
	{
		SDL_GPUColorTargetInfo colorTargetInfo = { 0 };
		colorTargetInfo.texture = swapchainTexture;
		colorTargetInfo.clear_color = (SDL_FColor){ 0.2f, 0.5f, 0.4f, 1.0f };
		colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
		colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

		SDL_PushGPUFragmentUniformData(cmdbuf, 0, (float[]) { 1.0f / SceneWidth, 1.0f / SceneHeight }, 8);

		SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTargetInfo, 1, NULL);
		SDL_BindGPUGraphicsPipeline(renderPass, EffectPipeline);
		SDL_BindGPUVertexBuffers(renderPass, 0, &(SDL_GPUBufferBinding){ .buffer = EffectVertexBuffer, .offset = 0 }, 1);
		SDL_BindGPUIndexBuffer(renderPass, &(SDL_GPUBufferBinding){ .buffer = EffectIndexBuffer, .offset = 0 }, SDL_GPU_INDEXELEMENTSIZE_16BIT);
		SDL_BindGPUFragmentSamplers(renderPass, 0, (SDL_GPUTextureSamplerBinding[]){
			{ .texture = SceneColorTexture, .sampler = EffectSampler },
			{ .texture = SceneDepthTexture, .sampler = EffectSampler }
		}, 2);
		SDL_DrawGPUIndexedPrimitives(renderPass, 6, 1, 0, 0, 0);
		SDL_EndGPURenderPass(renderPass);
	}

	SDL_SubmitGPUCommandBuffer(cmdbuf);

	return 0;
}

static void Quit(Context* context)
{
	SDL_ReleaseGPUGraphicsPipeline(context->Device, ScenePipeline);
	SDL_ReleaseGPUTexture(context->Device, SceneColorTexture);
	SDL_ReleaseGPUTexture(context->Device, SceneDepthTexture);
	SDL_ReleaseGPUBuffer(context->Device, SceneVertexBuffer);
	SDL_ReleaseGPUBuffer(context->Device, SceneIndexBuffer);

	SDL_ReleaseGPUGraphicsPipeline(context->Device, EffectPipeline);
	SDL_ReleaseGPUBuffer(context->Device, EffectVertexBuffer);
	SDL_ReleaseGPUBuffer(context->Device, EffectIndexBuffer);
	SDL_ReleaseGPUSampler(context->Device, EffectSampler);

	CommonQuit(context);
}

Example DepthSampler_Example = { "DepthSampler", Init, Update, Draw, Quit };
