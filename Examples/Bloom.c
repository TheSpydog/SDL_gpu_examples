#include <SDL_gpu_examples.h>

#include <stdbool.h>

static SDL_GpuBuffer* VertexBuffer;
static SDL_GpuBuffer* IndexBuffer;

static SDL_GpuSampler* Sampler;

static SDL_GpuTexture* InputTexture;
static SDL_GpuTexture* IntermediateTextures[5];
static SDL_GpuTexture* OutputTexture;

static SDL_GpuGraphicsPipeline* DownsamplePipeline;
static SDL_GpuGraphicsPipeline* UpsamplePipeline;
static SDL_GpuGraphicsPipeline* BlendPipeline;

static int w, h;

static float Weight = 0.01f;
static float FilterRadius = 0.04f;

static int Init(Context* context) {
	/* Manually set up example for HDR rendering */
	context->Device = SDL_GpuCreateDevice(SDL_GPU_BACKEND_ALL, SDL_TRUE, SDL_FALSE);
	if (context->Device == NULL) {
		SDL_Log("GpuCreateDevice failed");
		return -1;
	}

	int img_x, img_y, n;
	float* hdrImageData = LoadImage("memorial.hdr", &img_x, &img_y, &n, 4, SDL_TRUE);

	if (hdrImageData == NULL) {
		SDL_Log("Could not load HDR image data!");
		return -1;
	}

	context->Window = SDL_CreateWindow(context->ExampleName, img_x, img_y, 0);
	if (context->Window == NULL) {
		SDL_Log("CreateWindow failed: %s", SDL_GetError());
		return -1;
	}

	if (!SDL_GpuClaimWindow(context->Device, context->Window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC)) {
		SDL_Log("GpuClaimWindow failed");
		return -1;
	}

	SDL_GetWindowSizeInPixels(context->Window, &w, &h);

	/* Create the downsample pipeline */
	{
		SDL_GpuShader* vertexShader = LoadShader(context->Device, "Bloom.vert", 0, 0, 0, 0);
		if (vertexShader == NULL) {
			SDL_Log("Failed to create vertex shader!");
			return -1;
		}

		SDL_GpuShader* fragmentShader = LoadShader(context->Device, "BloomDownsample.frag", 1, 0, 0, 0);
		if (fragmentShader == NULL) {
			SDL_Log("Failed to create fragment shader!");
			return -1;
		}

		SDL_GpuGraphicsPipelineCreateInfo pipelineCreateInfo = {
			.attachmentInfo = {
				.colorAttachmentCount = 1,
				.colorAttachmentDescriptions = (SDL_GpuColorAttachmentDescription[]){{
					.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_SFLOAT,
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
					.stride = sizeof(PositionTextureVertex)
				}},
				.vertexAttributeCount = 2,
				.vertexAttributes = (SDL_GpuVertexAttribute[]){{
					.binding = 0,
					.format = SDL_GPU_VERTEXELEMENTFORMAT_VECTOR3,
					.location = 0,
					.offset = 0
				}, {
					.binding = 0,
					.format = SDL_GPU_VERTEXELEMENTFORMAT_VECTOR2,
					.location = 1,
					.offset = sizeof(float) * 3
				}}
			},
			.multisampleState.sampleMask = 0xFFFF,
			.primitiveType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
			.vertexShader = vertexShader,
			.fragmentShader = fragmentShader
		};

		SDL_GpuGraphicsPipeline* pipeline = SDL_GpuCreateGraphicsPipeline(context->Device, &pipelineCreateInfo);
		if (pipeline == NULL) {
			SDL_Log("Failed to create pipeline!");
			return -1;
		}

		SDL_GpuReleaseShader(context->Device, vertexShader);
		SDL_GpuReleaseShader(context->Device, fragmentShader);

		DownsamplePipeline = pipeline;
	}

	/* Create the upsample pipeline */
	{
		SDL_GpuShader* vertexShader = LoadShader(context->Device, "Bloom.vert", 0, 0, 0, 0);
		if (vertexShader == NULL) {
			SDL_Log("Failed to create vertex shader!");
			return -1;
		}

		SDL_GpuShader* fragmentShader = LoadShader(context->Device, "BloomUpsample.frag", 1, 1, 0, 0);
		if (fragmentShader == NULL) {
			SDL_Log("Failed to create fragment shader!");
			return -1;
		}

		/* Note that this has additive blending between source and destination */
		SDL_GpuGraphicsPipelineCreateInfo pipelineCreateInfo = {
			.attachmentInfo = {
				.colorAttachmentCount = 1,
				.colorAttachmentDescriptions = (SDL_GpuColorAttachmentDescription[]){{
					.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_SFLOAT,
					.blendState = {
						.blendEnable = SDL_TRUE,
						.alphaBlendOp = SDL_GPU_BLENDOP_ADD,
						.colorBlendOp = SDL_GPU_BLENDOP_ADD,
						.colorWriteMask = 0xF,
						.srcColorBlendFactor = SDL_GPU_BLENDFACTOR_ONE,
						.srcAlphaBlendFactor = SDL_GPU_BLENDFACTOR_ONE,
						.dstColorBlendFactor = SDL_GPU_BLENDFACTOR_ONE,
						.dstAlphaBlendFactor = SDL_GPU_BLENDFACTOR_ONE
					}
				}},
			},
			.vertexInputState = (SDL_GpuVertexInputState){
				.vertexBindingCount = 1,
				.vertexBindings = (SDL_GpuVertexBinding[]){{
					.binding = 0,
					.inputRate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
					.stepRate = 0,
					.stride = sizeof(PositionTextureVertex)
				}},
				.vertexAttributeCount = 2,
				.vertexAttributes = (SDL_GpuVertexAttribute[]){{
					.binding = 0,
					.format = SDL_GPU_VERTEXELEMENTFORMAT_VECTOR3,
					.location = 0,
					.offset = 0
				}, {
					.binding = 0,
					.format = SDL_GPU_VERTEXELEMENTFORMAT_VECTOR2,
					.location = 1,
					.offset = sizeof(float) * 3
				}}
			},
			.multisampleState.sampleMask = 0xFFFF,
			.primitiveType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
			.vertexShader = vertexShader,
			.fragmentShader = fragmentShader
		};

		SDL_GpuGraphicsPipeline* pipeline = SDL_GpuCreateGraphicsPipeline(context->Device, &pipelineCreateInfo);
		if (pipeline == NULL) {
			SDL_Log("Failed to create pipeline!");
			return -1;
		}

		SDL_GpuReleaseShader(context->Device, vertexShader);
		SDL_GpuReleaseShader(context->Device, fragmentShader);

		UpsamplePipeline = pipeline;
	}

	/* Create the blend pipeline */
	{
		SDL_GpuShader* vertexShader = LoadShader(context->Device, "Bloom.vert", 0, 0, 0, 0);
		if (vertexShader == NULL) {
			SDL_Log("Failed to create vertex shader!");
			return -1;
		}

		SDL_GpuShader* fragmentShader = LoadShader(context->Device, "LerpBlend.frag", 2, 1, 0, 0);
		if (fragmentShader == NULL) {
			SDL_Log("Failed to create fragment shader!");
			return -1;
		}

		SDL_GpuGraphicsPipelineCreateInfo pipelineCreateInfo = {
			.attachmentInfo = {
				.colorAttachmentCount = 1,
				.colorAttachmentDescriptions = (SDL_GpuColorAttachmentDescription[]){{
					.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_SFLOAT,
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
					.stride = sizeof(PositionTextureVertex)
				}},
				.vertexAttributeCount = 2,
				.vertexAttributes = (SDL_GpuVertexAttribute[]){{
					.binding = 0,
					.format = SDL_GPU_VERTEXELEMENTFORMAT_VECTOR3,
					.location = 0,
					.offset = 0
				}, {
					.binding = 0,
					.format = SDL_GPU_VERTEXELEMENTFORMAT_VECTOR2,
					.location = 1,
					.offset = sizeof(float) * 3
				}}
			},
			.multisampleState.sampleMask = 0xFFFF,
			.primitiveType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
			.vertexShader = vertexShader,
			.fragmentShader = fragmentShader
		};

		SDL_GpuGraphicsPipeline* pipeline = SDL_GpuCreateGraphicsPipeline(context->Device, &pipelineCreateInfo);
		if (pipeline == NULL) {
			SDL_Log("Failed to create pipeline!");
			return -1;
		}

		SDL_GpuReleaseShader(context->Device, vertexShader);
		SDL_GpuReleaseShader(context->Device, fragmentShader);

		BlendPipeline = pipeline;
	}

	Sampler = SDL_GpuCreateSampler(context->Device, &(SDL_GpuSamplerCreateInfo){
		.minFilter = SDL_GPU_FILTER_LINEAR,
		.magFilter = SDL_GPU_FILTER_LINEAR,
		.mipmapMode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
		.addressModeU = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		.addressModeV = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		.addressModeW = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
	});

	// Create the GPU resources
	VertexBuffer = SDL_GpuCreateBuffer(
		context->Device,
		SDL_GPU_BUFFERUSAGE_VERTEX_BIT,
		sizeof(PositionTextureVertex) * 4
	);
	SDL_GpuSetBufferName(
		context->Device,
		VertexBuffer,
		"Bloom Vertex Buffer"
	);

	IndexBuffer = SDL_GpuCreateBuffer(
		context->Device,
		SDL_GPU_BUFFERUSAGE_INDEX_BIT,
		sizeof(Uint16) * 6
	);

	InputTexture = SDL_GpuCreateTexture(context->Device, &(SDL_GpuTextureCreateInfo){
		.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_SFLOAT,
			.width = img_x,
			.height = img_y,
			.depth = 1,
			.layerCount = 1,
			.levelCount = 1,
			.usageFlags = SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT
	});
	SDL_GpuSetTextureName(
		context->Device,
		InputTexture,
		"Input Texture"
	);

	int mipSizeX = img_x;
	int mipSizeY = img_y;
	for (int i = 0; i < SDL_arraysize(IntermediateTextures); i++) {
		mipSizeX /= 2;
		mipSizeY /= 2;

		IntermediateTextures[i] = SDL_GpuCreateTexture(context->Device, &(SDL_GpuTextureCreateInfo){
			.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_SFLOAT,
				.width = mipSizeX,
				.height = mipSizeY,
				.depth = 1,
				.layerCount = 1,
				.levelCount = 1,
				.usageFlags = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT | SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT
		});
	}

	OutputTexture = SDL_GpuCreateTexture(context->Device, &(SDL_GpuTextureCreateInfo){
		.format = SDL_GPU_TEXTUREFORMAT_R32G32B32A32_SFLOAT,
			.width = img_x,
			.height = img_y,
			.depth = 1,
			.layerCount = 1,
			.levelCount = 1,
			.usageFlags = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT | SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT
	});
	SDL_GpuSetTextureName(
		context->Device,
		OutputTexture,
		"Output Texture"
	);

	// Set up buffer data
	SDL_GpuTransferBuffer* bufferTransferBuffer = SDL_GpuCreateTransferBuffer(
		context->Device,
		SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
		(sizeof(PositionTextureVertex) * 4) + (sizeof(Uint16) * 6)
	);

	PositionTextureVertex* transferData;
	SDL_GpuMapTransferBuffer(
		context->Device,
		bufferTransferBuffer,
		SDL_FALSE,
		(void**)&transferData
	);

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

	SDL_GpuUnmapTransferBuffer(context->Device, bufferTransferBuffer);

	// Set up texture data
	SDL_GpuTransferBuffer* imageDataTransferBuffer = SDL_GpuCreateTransferBuffer(
		context->Device,
		SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
		sizeof(float) * 4 * img_x * img_y
	);

	SDL_GpuSetTransferData(
		context->Device,
		hdrImageData,
		&(SDL_GpuTransferBufferRegion) {
		.transferBuffer = imageDataTransferBuffer,
			.offset = 0,
			.size = sizeof(float) * 4 * img_x * img_y
	},
		SDL_FALSE
	);

	SDL_free(hdrImageData);

	// Upload the transfer data to the GPU resources
	SDL_GpuCommandBuffer* uploadCmdBuf = SDL_GpuAcquireCommandBuffer(context->Device);
	SDL_GpuCopyPass* copyPass = SDL_GpuBeginCopyPass(uploadCmdBuf);

	SDL_GpuUploadToBuffer(
		copyPass,
		&(SDL_GpuTransferBufferLocation) {
		.transferBuffer = bufferTransferBuffer,
			.offset = 0
	},
		& (SDL_GpuBufferRegion) {
		.buffer = VertexBuffer,
			.offset = 0,
			.size = sizeof(PositionTextureVertex) * 4
	},
		SDL_FALSE
	);

	SDL_GpuUploadToBuffer(
		copyPass,
		&(SDL_GpuTransferBufferLocation) {
		.transferBuffer = bufferTransferBuffer,
			.offset = sizeof(PositionTextureVertex) * 4
	},
		& (SDL_GpuBufferRegion) {
		.buffer = IndexBuffer,
			.offset = 0,
			.size = sizeof(Uint16) * 6
	},
		SDL_FALSE
	);

	SDL_GpuUploadToTexture(
		copyPass,
		&(SDL_GpuTextureTransferInfo) {
		.transferBuffer = imageDataTransferBuffer,
			.offset = 0, /* Zeroes out the rest */
	},
		& (SDL_GpuTextureRegion) {
		.textureSlice.texture = InputTexture,
			.w = img_x,
			.h = img_y,
			.d = 1
	},
		SDL_FALSE
	);

	SDL_GpuEndCopyPass(copyPass);
	SDL_GpuSubmit(uploadCmdBuf);
	SDL_GpuReleaseTransferBuffer(context->Device, bufferTransferBuffer);
	SDL_GpuReleaseTransferBuffer(context->Device, imageDataTransferBuffer);

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
	SDL_GpuCommandBuffer* cmdbuf = SDL_GpuAcquireCommandBuffer(context->Device);
	if (cmdbuf == NULL) {
		SDL_Log("GpuAcquireCommandBuffer failed");
		return -1;
	}

	Uint32 w, h;
	SDL_GpuTexture* swapchainTexture = SDL_GpuAcquireSwapchainTexture(cmdbuf, context->Window, &w, &h);
	if (swapchainTexture == NULL) {
		SDL_GpuSubmit(cmdbuf);
		return 0;
	}

	/* Down sample the original texture for each layer */
	SDL_GpuTexture* sourceTexture = InputTexture;
	for (int i = 0; i < SDL_arraysize(IntermediateTextures); i++) {

		SDL_GpuColorAttachmentInfo colorAttachmentInfo = { 0 };
		colorAttachmentInfo.textureSlice.texture = IntermediateTextures[i];
		colorAttachmentInfo.clearColor = (SDL_GpuColor){ 0.0f, 0.0f, 0.0f, 1.0f };
		colorAttachmentInfo.loadOp = SDL_GPU_LOADOP_CLEAR;
		colorAttachmentInfo.storeOp = SDL_GPU_STOREOP_STORE;  

		SDL_GpuRenderPass* renderPass = SDL_GpuBeginRenderPass(cmdbuf, &colorAttachmentInfo, 1, NULL);

		SDL_GpuBindGraphicsPipeline(renderPass, DownsamplePipeline);
		SDL_GpuBindVertexBuffers(renderPass, 0, &(SDL_GpuBufferBinding){.buffer = VertexBuffer, .offset = 0 }, 1);
		SDL_GpuBindIndexBuffer(renderPass, &(SDL_GpuBufferBinding){.buffer = IndexBuffer, .offset = 0 }, SDL_GPU_INDEXELEMENTSIZE_16BIT);
		SDL_GpuBindFragmentSamplers(renderPass, 0, &(SDL_GpuTextureSamplerBinding){.texture = sourceTexture, .sampler = Sampler }, 1);
		SDL_GpuDrawIndexedPrimitives(renderPass, 0, 0, 2, 1);

		SDL_GpuEndRenderPass(renderPass);

		sourceTexture = IntermediateTextures[i];
	}

	/* Up-sample in reverse, blending with the previous texture */
	for (int i = SDL_arraysize(IntermediateTextures) - 1; i > 0; i--) {
		SDL_GpuColorAttachmentInfo colorAttachmentInfo = { 0 };
		colorAttachmentInfo.textureSlice.texture = IntermediateTextures[i - 1];
		colorAttachmentInfo.clearColor = (SDL_GpuColor){ 0.0f, 0.0f, 0.0f, 1.0f };
		colorAttachmentInfo.loadOp = SDL_GPU_LOADOP_LOAD;
		colorAttachmentInfo.storeOp = SDL_GPU_STOREOP_STORE;

		SDL_GpuRenderPass* renderPass = SDL_GpuBeginRenderPass(cmdbuf, &colorAttachmentInfo, 1, NULL);

		SDL_GpuBindGraphicsPipeline(renderPass, UpsamplePipeline);
		SDL_GpuBindVertexBuffers(renderPass, 0, &(SDL_GpuBufferBinding){.buffer = VertexBuffer, .offset = 0 }, 1);
		SDL_GpuBindIndexBuffer(renderPass, &(SDL_GpuBufferBinding){.buffer = IndexBuffer, .offset = 0 }, SDL_GPU_INDEXELEMENTSIZE_16BIT);
		SDL_GpuBindFragmentSamplers(renderPass, 0, &(SDL_GpuTextureSamplerBinding){.texture = IntermediateTextures[i], .sampler = Sampler }, 1);
		SDL_GpuPushFragmentUniformData(cmdbuf, 0, &FilterRadius, sizeof(FilterRadius));
		SDL_GpuDrawIndexedPrimitives(renderPass, 0, 0, 2, 1);

		SDL_GpuEndRenderPass(renderPass);
	}

	/* Blend the final texture into the original texture */
	{
		SDL_GpuColorAttachmentInfo colorAttachmentInfo = { 0 };
		colorAttachmentInfo.textureSlice.texture = OutputTexture;
		colorAttachmentInfo.clearColor = (SDL_GpuColor){ 0.0f, 0.0f, 0.0f, 1.0f };
		colorAttachmentInfo.loadOp = SDL_GPU_LOADOP_CLEAR;
		colorAttachmentInfo.storeOp = SDL_GPU_STOREOP_STORE;

		SDL_GpuRenderPass* renderPass = SDL_GpuBeginRenderPass(cmdbuf, &colorAttachmentInfo, 1, NULL);

		SDL_GpuBindGraphicsPipeline(renderPass, BlendPipeline);
		SDL_GpuBindVertexBuffers(renderPass, 0, &(SDL_GpuBufferBinding){.buffer = VertexBuffer, .offset = 0 }, 1);
		SDL_GpuBindIndexBuffer(renderPass, &(SDL_GpuBufferBinding){.buffer = IndexBuffer, .offset = 0 }, SDL_GPU_INDEXELEMENTSIZE_16BIT);
		SDL_GpuBindFragmentSamplers(renderPass, 0,
			&(SDL_GpuTextureSamplerBinding[]){
				{ .texture = InputTexture, .sampler = Sampler },
				{ .texture = IntermediateTextures[0], .sampler = Sampler }
		}, 2);
		SDL_GpuPushFragmentUniformData(cmdbuf, 0, &Weight, sizeof(Weight));
		SDL_GpuDrawIndexedPrimitives(renderPass, 0, 0, 2, 1);

		SDL_GpuEndRenderPass(renderPass);
	}

	/* Finally, blit the output directly to the swapchain texture. In a real render pipeline, it would be used as the input
	 * to a tonemapping pass. */
	SDL_GpuBlit(
		cmdbuf,
		&(SDL_GpuTextureRegion){
		.textureSlice.texture = OutputTexture,
			.w = w,
			.h = h,
			.d = 1
	},
		& (SDL_GpuTextureRegion) {
		.textureSlice.texture = swapchainTexture,
			.w = w,
			.h = h,
			.d = 1
	},
		SDL_GPU_FILTER_LINEAR,
		SDL_FALSE
	);

	SDL_GpuSubmit(cmdbuf);

	return 0;
}

static void Quit(Context* context) {
	SDL_GpuReleaseBuffer(context->Device, VertexBuffer);
	SDL_GpuReleaseBuffer(context->Device, IndexBuffer);
	SDL_GpuReleaseSampler(context->Device, Sampler);

	SDL_GpuReleaseGraphicsPipeline(context->Device, DownsamplePipeline);
	SDL_GpuReleaseGraphicsPipeline(context->Device, UpsamplePipeline);
	SDL_GpuReleaseGraphicsPipeline(context->Device, BlendPipeline);

	SDL_GpuReleaseTexture(context->Device, InputTexture);
	for (int i = 0; i < SDL_arraysize(IntermediateTextures); i++) {
		SDL_GpuReleaseTexture(context->Device, IntermediateTextures[i]);
	}
	SDL_GpuReleaseTexture(context->Device, OutputTexture);

	CommonQuit(context);
}

Example Bloom_Example = { "Bloom", Init, Update, Draw, Quit };
