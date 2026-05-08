#include "Common.h"

static SDL_GPUBuffer* VertexBuffer;
static SDL_GPUBuffer* IndexBuffer;

static SDL_GPUSampler* Sampler;

static SDL_GPUTexture* InputTexture;
static SDL_GPUTexture* IntermediateTextures[5];
static SDL_GPUTexture* OutputTexture;

static SDL_GPUGraphicsPipeline* DownsamplePipeline;
static SDL_GPUGraphicsPipeline* UpsamplePipeline;
static SDL_GPUGraphicsPipeline* BlendPipeline;

static int w, h;

static float Weight = 0.01f;
static float FilterRadius = 0.04f;

static int Init(Context* context)
{
	int result = CommonInit(context, 0);
	if (result < 0)
	{
		return result;
	}

	int img_x, img_y, n;
	float* hdrImageData = LoadHDRImage("memorial.hdr", &img_x, &img_y, &n, 4);

	if (hdrImageData == NULL) {
		SDL_Log("Could not load HDR image data!");
		return -1;
	}

	SDL_GetWindowSizeInPixels(context->Window, &w, &h);

	/* Create the downsample pipeline */
	{
		SDL_GPUShader* vertexShader = LoadShader(context->Device, "Bloom.vert", 0, 0, 0, 0);
		if (vertexShader == NULL) {
			SDL_Log("Failed to create vertex shader!");
			return -1;
		}

		SDL_GPUShader* fragmentShader = LoadShader(context->Device, "BloomDownsample.frag", 1, 0, 0, 0);
		if (fragmentShader == NULL) {
			SDL_Log("Failed to create fragment shader!");
			return -1;
		}

		SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {
			.target_info = {
				.num_color_targets = 1,
				.color_target_descriptions = (SDL_GPUColorTargetDescription[]){{
					.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
					.blend_state = {
						.enable_blend = true,
						.alpha_blend_op = SDL_GPU_BLENDOP_ADD,
						.color_blend_op = SDL_GPU_BLENDOP_ADD,
						.color_write_mask = 0xF,
						.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
						.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
						.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ZERO,
						.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO
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
			.vertex_shader = vertexShader,
			.fragment_shader = fragmentShader
		};

		SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(context->Device, &pipelineCreateInfo);
		if (pipeline == NULL) {
			SDL_Log("Failed to create pipeline!");
			return -1;
		}

		SDL_ReleaseGPUShader(context->Device, vertexShader);
		SDL_ReleaseGPUShader(context->Device, fragmentShader);

		DownsamplePipeline = pipeline;
	}

	/* Create the upsample pipeline */
	{
		SDL_GPUShader* vertexShader = LoadShader(context->Device, "Bloom.vert", 0, 0, 0, 0);
		if (vertexShader == NULL) {
			SDL_Log("Failed to create vertex shader!");
			return -1;
		}

		SDL_GPUShader* fragmentShader = LoadShader(context->Device, "BloomUpsample.frag", 1, 1, 0, 0);
		if (fragmentShader == NULL) {
			SDL_Log("Failed to create fragment shader!");
			return -1;
		}

		/* Note that this has additive blending between source and destination */
		SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {
			.target_info = {
				.num_color_targets = 1,
				.color_target_descriptions = (SDL_GPUColorTargetDescription[]){{
					.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
					.blend_state = {
						.enable_blend = true,
						.alpha_blend_op = SDL_GPU_BLENDOP_ADD,
						.color_blend_op = SDL_GPU_BLENDOP_ADD,
						.color_write_mask = 0xF,
						.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
						.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
						.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
						.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE
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
			.vertex_shader = vertexShader,
			.fragment_shader = fragmentShader
		};

		SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(context->Device, &pipelineCreateInfo);
		if (pipeline == NULL) {
			SDL_Log("Failed to create pipeline!");
			return -1;
		}

		SDL_ReleaseGPUShader(context->Device, vertexShader);
		SDL_ReleaseGPUShader(context->Device, fragmentShader);

		UpsamplePipeline = pipeline;
	}

	/* Create the blend pipeline */
	{
		SDL_GPUShader* vertexShader = LoadShader(context->Device, "Bloom.vert", 0, 0, 0, 0);
		if (vertexShader == NULL) {
			SDL_Log("Failed to create vertex shader!");
			return -1;
		}

		SDL_GPUShader* fragmentShader = LoadShader(context->Device, "LerpBlend.frag", 2, 1, 0, 0);
		if (fragmentShader == NULL) {
			SDL_Log("Failed to create fragment shader!");
			return -1;
		}

		SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {
			.target_info = {
				.num_color_targets = 1,
				.color_target_descriptions = (SDL_GPUColorTargetDescription[]){{
					.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
					.blend_state = {
						.enable_blend = true,
						.alpha_blend_op = SDL_GPU_BLENDOP_ADD,
						.color_blend_op = SDL_GPU_BLENDOP_ADD,
						.color_write_mask = 0xF,
						.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
						.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
						.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ZERO,
						.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO
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
			.vertex_shader = vertexShader,
			.fragment_shader = fragmentShader
		};

		SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(context->Device, &pipelineCreateInfo);
		if (pipeline == NULL) {
			SDL_Log("Failed to create pipeline!");
			return -1;
		}

		SDL_ReleaseGPUShader(context->Device, vertexShader);
		SDL_ReleaseGPUShader(context->Device, fragmentShader);

		BlendPipeline = pipeline;
	}

	Sampler = SDL_CreateGPUSampler(context->Device, &(SDL_GPUSamplerCreateInfo){
		.min_filter = SDL_GPU_FILTER_LINEAR,
		.mag_filter = SDL_GPU_FILTER_LINEAR,
		.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
		.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
	});

	SDL_PropertiesID props = SDL_CreateProperties();
	SDL_SetStringProperty(props, SDL_PROP_GPU_BUFFER_CREATE_NAME_STRING, "Bloom Vertex Buffer");

	// Create the GPU resources
	VertexBuffer = SDL_CreateGPUBuffer(
		context->Device,
		&(SDL_GPUBufferCreateInfo) {
			.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
			.size = sizeof(PositionTextureVertex) * 4,
			.props = props
		}
	);

	SDL_DestroyProperties(props);

	IndexBuffer = SDL_CreateGPUBuffer(
		context->Device,
		&(SDL_GPUBufferCreateInfo) {
			.usage = SDL_GPU_BUFFERUSAGE_INDEX,
			.size = sizeof(Uint16) * 6,
			.props = props
		}
	);

	props = SDL_CreateProperties();
	SDL_SetStringProperty(props, SDL_PROP_GPU_TEXTURE_CREATE_NAME_STRING, "Input Texture");

	InputTexture = SDL_CreateGPUTexture(context->Device, &(SDL_GPUTextureCreateInfo){
		.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
			.width = img_x,
			.height = img_y,
			.layer_count_or_depth = 1,
			.num_levels = 1,
			.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
			.props = props
	});


	int mipSizeX = img_x;
	int mipSizeY = img_y;
	for (int i = 0; i < SDL_arraysize(IntermediateTextures); i++) {
		mipSizeX /= 2;
		mipSizeY /= 2;

		IntermediateTextures[i] = SDL_CreateGPUTexture(context->Device, &(SDL_GPUTextureCreateInfo){
			.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
				.width = mipSizeX,
				.height = mipSizeY,
				.layer_count_or_depth = 1,
				.num_levels = 1,
				.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER
		});
	}

	SDL_SetStringProperty(props, SDL_PROP_GPU_TEXTURE_CREATE_NAME_STRING, "Output Texture");

	OutputTexture = SDL_CreateGPUTexture(context->Device, &(SDL_GPUTextureCreateInfo){
		.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT,
			.width = img_x,
			.height = img_y,
			.layer_count_or_depth = 1,
			.num_levels = 1,
			.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
			.props = props
	});

	SDL_DestroyProperties(props);

	// Set up buffer data
	SDL_GPUTransferBuffer* bufferTransferBuffer = SDL_CreateGPUTransferBuffer(
		context->Device,
		&(SDL_GPUTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = (sizeof(PositionTextureVertex) * 4) + (sizeof(Uint16) * 6)
		}
	);

	PositionTextureVertex* transferData =
		SDL_MapGPUTransferBuffer(
			context->Device,
			bufferTransferBuffer,
			false);

	transferData[0] = (PositionTextureVertex){ -1,  1, 0, 0, 0 };
	transferData[1] = (PositionTextureVertex){ 1,  1, 0, 1, 0 };
	transferData[2] = (PositionTextureVertex){ 1, -1, 0, 1, 1 };
	transferData[3] = (PositionTextureVertex){ -1, -1, 0, 0, 1 };

	Uint16* indexData = (Uint16*)&transferData[4];
	indexData[0] = 0;
	indexData[1] = 1;
	indexData[2] = 2;
	indexData[3] = 0;
	indexData[4] = 2;
	indexData[5] = 3;

	SDL_UnmapGPUTransferBuffer(context->Device, bufferTransferBuffer);

	// Set up texture data
	SDL_GPUTransferBuffer* imageDataTransferBuffer = SDL_CreateGPUTransferBuffer(
		context->Device,
		&(SDL_GPUTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = sizeof(float) * 4 * img_x * img_y
		}
	);

	transferData =
		SDL_MapGPUTransferBuffer(
			context->Device,
			imageDataTransferBuffer,
			false);

	SDL_memcpy(transferData, hdrImageData, sizeof(float) * 4 * img_x * img_y);

	SDL_UnmapGPUTransferBuffer(context->Device, imageDataTransferBuffer);
	SDL_free(hdrImageData);

	// Upload the transfer data to the GPU resources
	SDL_GPUCommandBuffer* uploadCmdBuf = SDL_AcquireGPUCommandBuffer(context->Device);
	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);

	SDL_UploadToGPUBuffer(
		copyPass,
		&(SDL_GPUTransferBufferLocation) {
			.transfer_buffer = bufferTransferBuffer,
			.offset = 0
		},
		&(SDL_GPUBufferRegion) {
			.buffer = VertexBuffer,
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
			.buffer = IndexBuffer,
			.offset = 0,
			.size = sizeof(Uint16) * 6
		},
		false
	);

	SDL_UploadToGPUTexture(
		copyPass,
		&(SDL_GPUTextureTransferInfo) {
			.transfer_buffer = imageDataTransferBuffer,
			.offset = 0, /* Zeroes out the rest */
		},
		&(SDL_GPUTextureRegion) {
			.texture = InputTexture,
			.w = img_x,
			.h = img_y,
			.d = 1
		},
		false
	);

	SDL_EndGPUCopyPass(copyPass);
	SDL_SubmitGPUCommandBuffer(uploadCmdBuf);
	SDL_ReleaseGPUTransferBuffer(context->Device, bufferTransferBuffer);
	SDL_ReleaseGPUTransferBuffer(context->Device, imageDataTransferBuffer);

	SDL_Log("Press Left/Right to increase/decrease the blur radius (Current: %f)", FilterRadius);
	SDL_Log("Press Up/Down to increase/decrease the final blend weight (Current: %f)", Weight);

	return 0;
}

static int Update(Context* context) {
	if (context->LeftPressed) {
		FilterRadius -= 0.01f;
		if (FilterRadius < 0.01f) {
			FilterRadius = 0.01f;
		}
		SDL_Log("Blur Radius: %f", FilterRadius);
	} else if (context->RightPressed) {
		FilterRadius += 0.01f;
		SDL_Log("Blur Radius: %f", FilterRadius);
	}

	if (context->UpPressed) {
		Weight += 0.001f;
		SDL_Log("Blend Weight: %f", Weight);
	} else if (context->DownPressed) {
		Weight -= 0.001f;
		if (Weight < 0) {
			Weight = 0;
		}
		SDL_Log("Blend Weight: %f", Weight);
	}

	return 0;
}

static int Draw(Context* context) {
	SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(context->Device);
	if (cmdbuf == NULL) {
		SDL_Log("SDL_AcquireGPUCommandBuffer failed");
		return -1;
	}

	Uint32 w, h;
	SDL_GPUTexture *swapchainTexture;
	if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdbuf, context->Window, &swapchainTexture, &w, &h)) {
		SDL_Log("WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
		return -1;
	}

	if (swapchainTexture == NULL) {
		SDL_SubmitGPUCommandBuffer(cmdbuf);
		return 0;
	}

	/* Down sample the original texture for each layer */
	SDL_GPUTexture* sourceTexture = InputTexture;
	for (int i = 0; i < SDL_arraysize(IntermediateTextures); i++) {

		SDL_GPUColorTargetInfo colorTargetInfo = { 0 };
		colorTargetInfo.texture = IntermediateTextures[i];
		colorTargetInfo.clear_color = (SDL_FColor){ 0.0f, 0.0f, 0.0f, 1.0f };
		colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
		colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

		SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTargetInfo, 1, NULL);

		SDL_BindGPUGraphicsPipeline(renderPass, DownsamplePipeline);
		SDL_BindGPUVertexBuffers(renderPass, 0, &(SDL_GPUBufferBinding){.buffer = VertexBuffer, .offset = 0 }, 1);
		SDL_BindGPUIndexBuffer(renderPass, &(SDL_GPUBufferBinding){.buffer = IndexBuffer, .offset = 0 }, SDL_GPU_INDEXELEMENTSIZE_16BIT);
		SDL_BindGPUFragmentSamplers(renderPass, 0, &(SDL_GPUTextureSamplerBinding){.texture = sourceTexture, .sampler = Sampler }, 1);
		SDL_DrawGPUIndexedPrimitives(renderPass, 6, 1, 0, 0, 0);

		SDL_EndGPURenderPass(renderPass);

		sourceTexture = IntermediateTextures[i];
	}

	/* Up-sample in reverse, blending with the previous texture */
	for (int i = SDL_arraysize(IntermediateTextures) - 1; i > 0; i--) {
		SDL_GPUColorTargetInfo colorTargetInfo = { 0 };
		colorTargetInfo.texture = IntermediateTextures[i - 1];
		colorTargetInfo.clear_color = (SDL_FColor){ 0.0f, 0.0f, 0.0f, 1.0f };
		colorTargetInfo.load_op = SDL_GPU_LOADOP_LOAD;
		colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

		SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTargetInfo, 1, NULL);

		SDL_BindGPUGraphicsPipeline(renderPass, UpsamplePipeline);
		SDL_BindGPUVertexBuffers(renderPass, 0, &(SDL_GPUBufferBinding){.buffer = VertexBuffer, .offset = 0 }, 1);
		SDL_BindGPUIndexBuffer(renderPass, &(SDL_GPUBufferBinding){.buffer = IndexBuffer, .offset = 0 }, SDL_GPU_INDEXELEMENTSIZE_16BIT);
		SDL_BindGPUFragmentSamplers(renderPass, 0, &(SDL_GPUTextureSamplerBinding){.texture = IntermediateTextures[i], .sampler = Sampler }, 1);
		SDL_PushGPUFragmentUniformData(cmdbuf, 0, &FilterRadius, sizeof(FilterRadius));
		SDL_DrawGPUIndexedPrimitives(renderPass, 6, 1, 0, 0, 0);

		SDL_EndGPURenderPass(renderPass);
	}

	/* Blend the final texture into the original texture */
	{
		SDL_GPUColorTargetInfo colorTargetInfo = { 0 };
		colorTargetInfo.texture = OutputTexture;
		colorTargetInfo.clear_color = (SDL_FColor){ 0.0f, 0.0f, 0.0f, 1.0f };
		colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
		colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

		SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTargetInfo, 1, NULL);

		SDL_BindGPUGraphicsPipeline(renderPass, BlendPipeline);
		SDL_BindGPUVertexBuffers(renderPass, 0, &(SDL_GPUBufferBinding){.buffer = VertexBuffer, .offset = 0 }, 1);
		SDL_BindGPUIndexBuffer(renderPass, &(SDL_GPUBufferBinding){.buffer = IndexBuffer, .offset = 0 }, SDL_GPU_INDEXELEMENTSIZE_16BIT);
		SDL_BindGPUFragmentSamplers(renderPass, 0,
			(SDL_GPUTextureSamplerBinding[]){
				{ .texture = InputTexture, .sampler = Sampler },
				{ .texture = IntermediateTextures[0], .sampler = Sampler }
		}, 2);
		SDL_PushGPUFragmentUniformData(cmdbuf, 0, &Weight, sizeof(Weight));
		SDL_DrawGPUIndexedPrimitives(renderPass, 6, 1, 0, 0, 0);

		SDL_EndGPURenderPass(renderPass);
	}

	/* Finally, blit the output directly to the swapchain texture. In a real render pipeline, it would be used as the input
	 * to a tonemapping pass. */
	SDL_BlitGPUTexture(
		cmdbuf,
		&(SDL_GPUBlitInfo){
			.load_op = SDL_GPU_LOADOP_DONT_CARE,
			.source = (SDL_GPUBlitRegion){
				.texture = OutputTexture,
				.w = w,
				.h = h
			},
			.destination = (SDL_GPUBlitRegion) {
				.texture = swapchainTexture,
				.w = w,
				.h = h
			},
			.filter = SDL_GPU_FILTER_LINEAR
		}
	);

	SDL_SubmitGPUCommandBuffer(cmdbuf);

	return 0;
}

static void Quit(Context* context) {
	SDL_ReleaseGPUBuffer(context->Device, VertexBuffer);
	SDL_ReleaseGPUBuffer(context->Device, IndexBuffer);
	SDL_ReleaseGPUSampler(context->Device, Sampler);

	SDL_ReleaseGPUGraphicsPipeline(context->Device, DownsamplePipeline);
	SDL_ReleaseGPUGraphicsPipeline(context->Device, UpsamplePipeline);
	SDL_ReleaseGPUGraphicsPipeline(context->Device, BlendPipeline);

	SDL_ReleaseGPUTexture(context->Device, InputTexture);
	for (int i = 0; i < SDL_arraysize(IntermediateTextures); i++) {
		SDL_ReleaseGPUTexture(context->Device, IntermediateTextures[i]);
	}
	SDL_ReleaseGPUTexture(context->Device, OutputTexture);

	CommonQuit(context);
}

Example Bloom_Example = { "Bloom", Init, Update, Draw, Quit };
