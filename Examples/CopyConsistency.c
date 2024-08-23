#include "Common.h"

static SDL_GpuGraphicsPipeline* Pipeline;
static SDL_GpuBuffer* VertexBuffer;
static SDL_GpuBuffer* LeftVertexBuffer;
static SDL_GpuBuffer* RightVertexBuffer;
static SDL_GpuBuffer* IndexBuffer;
static SDL_GpuTexture* Texture;
static SDL_GpuTexture* LeftTexture;
static SDL_GpuTexture* RightTexture;
static SDL_GpuSampler* Sampler;

static int Init(Context* context)
{
	int result = CommonInit(context, 0);
	if (result < 0)
	{
		return result;
	}

	// Create the shaders
	SDL_GpuShader* vertexShader = LoadShader(context->Device, "TexturedQuad.vert", 0, 0, 0, 0);
	if (vertexShader == NULL)
	{
		SDL_Log("Failed to create vertex shader!");
		return -1;
	}

	SDL_GpuShader* fragmentShader = LoadShader(context->Device, "TexturedQuad.frag", 1, 0, 0, 0);
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
					.dstColorBlendFactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
					.dstAlphaBlendFactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA
				}
			}},
		},
		.vertexInputState = (SDL_GpuVertexInputState){
			.vertexBindingCount = 1,
			.vertexBindings = (SDL_GpuVertexBinding[]){{
				.binding = 0,
				.inputRate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
				.instanceStepRate = 0,
				.stride = sizeof(PositionTextureVertex)
			}},
			.vertexAttributeCount = 2,
			.vertexAttributes = (SDL_GpuVertexAttribute[]){{
				.binding = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
				.location = 0,
				.offset = 0
			}, {
				.binding = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
				.location = 1,
				.offset = sizeof(float) * 3
			}}
		},
		.multisampleState.sampleMask = 0xFFFF,
		.primitiveType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
		.vertexShader = vertexShader,
		.fragmentShader = fragmentShader,
	};

	Pipeline = SDL_GpuCreateGraphicsPipeline(context->Device, &pipelineCreateInfo);
	if (Pipeline == NULL)
	{
		SDL_Log("Failed to create pipeline!");
		return -1;
	}

	SDL_GpuReleaseShader(context->Device, vertexShader);
	SDL_GpuReleaseShader(context->Device, fragmentShader);

	// Create the textures
	SDL_GpuTextureCreateInfo textureCreateInfo = {
		.type = SDL_GPU_TEXTURETYPE_2D,
		.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
		.width = 16,
		.height = 16,
		.layerCountOrDepth = 1,
		.levelCount = 1,
		.usageFlags = SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT
	};
	LeftTexture = SDL_GpuCreateTexture(context->Device, &textureCreateInfo);
	RightTexture = SDL_GpuCreateTexture(context->Device, &textureCreateInfo);
	Texture = SDL_GpuCreateTexture(context->Device, &textureCreateInfo);

	// Load the texture data
	SDL_Surface *leftImageData = LoadImage("ravioli.bmp", 4);
	if (leftImageData == NULL)
	{
		SDL_Log("Could not load image data!");
		return -1;
	}

	SDL_Surface *rightImageData = LoadImage("ravioli_inverted.bmp", 4);
	if (rightImageData == NULL)
	{
		SDL_Log("Could not load image data!");
		return -1;
	}

	SDL_assert(leftImageData->w == rightImageData->w);
	SDL_assert(leftImageData->h == rightImageData->h);

	// Create the sampler
	Sampler = SDL_GpuCreateSampler(context->Device, &(SDL_GpuSamplerCreateInfo){
		.minFilter = SDL_GPU_FILTER_NEAREST,
		.magFilter = SDL_GPU_FILTER_NEAREST,
		.mipmapMode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
		.addressModeU = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		.addressModeV = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		.addressModeW = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
	});

	// Create the buffers
	VertexBuffer = SDL_GpuCreateBuffer(
		context->Device,
		&(SDL_GpuBufferCreateInfo) {
			.usageFlags = SDL_GPU_BUFFERUSAGE_VERTEX_BIT,
			.sizeInBytes = sizeof(PositionTextureVertex) * 4
		}
	);
	LeftVertexBuffer = SDL_GpuCreateBuffer(
		context->Device,
		&(SDL_GpuBufferCreateInfo) {
			.usageFlags = SDL_GPU_BUFFERUSAGE_VERTEX_BIT,
			.sizeInBytes = sizeof(PositionTextureVertex) * 4
		}
	);
	RightVertexBuffer = SDL_GpuCreateBuffer(
		context->Device,
		&(SDL_GpuBufferCreateInfo) {
			.usageFlags = SDL_GPU_BUFFERUSAGE_VERTEX_BIT,
			.sizeInBytes = sizeof(PositionTextureVertex) * 4
		}
	);
	IndexBuffer = SDL_GpuCreateBuffer(
		context->Device,
		&(SDL_GpuBufferCreateInfo) {
			.usageFlags = SDL_GPU_BUFFERUSAGE_INDEX_BIT,
			.sizeInBytes = sizeof(Uint16) * 6
		}
	);

	// Set up buffer data
	SDL_GpuTransferBuffer* bufferTransferBuffer = SDL_GpuCreateTransferBuffer(
		context->Device,
		&(SDL_GpuTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.sizeInBytes = sizeof(PositionTextureVertex) * 8 + sizeof(Uint16) * 6
		}
	);

	PositionTextureVertex* transferData = SDL_GpuMapTransferBuffer(
		context->Device,
		bufferTransferBuffer,
		SDL_FALSE
	);

	transferData[0] = (PositionTextureVertex){ -1.0f,  1.0f, 0, 0, 0 };
	transferData[1] = (PositionTextureVertex){  0.0f,  1.0f, 0, 1, 0 };
	transferData[2] = (PositionTextureVertex){  0.0f, -1.0f, 0, 1, 1 };
	transferData[3] = (PositionTextureVertex){ -1.0f, -1.0f, 0, 0, 1 };

	transferData[4] = (PositionTextureVertex){  0.0f,  1.0f, 0, 0, 0 };
	transferData[5] = (PositionTextureVertex){  1.0f,  1.0f, 0, 1, 0 };
	transferData[6] = (PositionTextureVertex){  1.0f, -1.0f, 0, 1, 1 };
	transferData[7] = (PositionTextureVertex){  0.0f, -1.0f, 0, 0, 1 };

	Uint16* indexData = (Uint16*) &transferData[8];
	indexData[0] = 0;
	indexData[1] = 1;
	indexData[2] = 2;
	indexData[3] = 0;
	indexData[4] = 2;
	indexData[5] = 3;

	SDL_GpuUnmapTransferBuffer(context->Device, bufferTransferBuffer);

	// Set up texture data
	SDL_GpuTransferBuffer* textureTransferBuffer = SDL_GpuCreateTransferBuffer(
		context->Device,
		&(SDL_GpuTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.sizeInBytes = leftImageData->w * leftImageData->h * 8
		}
	);

	Uint8* textureTransferPtr = SDL_GpuMapTransferBuffer(
		context->Device,
		textureTransferBuffer,
		SDL_FALSE
	);
	SDL_memcpy(textureTransferPtr, leftImageData->pixels, leftImageData->w * leftImageData->h * 4);
	SDL_memcpy(textureTransferPtr + (leftImageData->w * leftImageData->h * 4), rightImageData->pixels, rightImageData->w * rightImageData->h * 4);
	SDL_GpuUnmapTransferBuffer(context->Device, textureTransferBuffer);

	// Upload the transfer data to the GPU resources
	SDL_GpuCommandBuffer* uploadCmdBuf = SDL_GpuAcquireCommandBuffer(context->Device);
	SDL_GpuCopyPass* copyPass = SDL_GpuBeginCopyPass(uploadCmdBuf);

	SDL_GpuUploadToBuffer(
		copyPass,
		&(SDL_GpuTransferBufferLocation) {
			.transferBuffer = bufferTransferBuffer,
			.offset = 0
		},
		&(SDL_GpuBufferRegion) {
			.buffer = LeftVertexBuffer,
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
		&(SDL_GpuBufferRegion) {
			.buffer = RightVertexBuffer,
			.offset = 0,
			.size = sizeof(PositionTextureVertex) * 4
		},
		SDL_FALSE
	);

	SDL_GpuUploadToBuffer(
		copyPass,
		&(SDL_GpuTransferBufferLocation) {
			.transferBuffer = bufferTransferBuffer,
			.offset = sizeof(PositionTextureVertex) * 8
		},
		&(SDL_GpuBufferRegion) {
			.buffer = IndexBuffer,
			.offset = 0,
			.size = sizeof(Uint16) * 6
		},
		SDL_FALSE
	);

	SDL_GpuUploadToTexture(
		copyPass,
		&(SDL_GpuTextureTransferInfo) {
			.transferBuffer = textureTransferBuffer,
			.offset = 0,
		},
		&(SDL_GpuTextureRegion){
			.texture = LeftTexture,
			.w = leftImageData->w,
			.h = leftImageData->h,
			.d = 1
		},
		SDL_FALSE
	);

	SDL_GpuUploadToTexture(
		copyPass,
		&(SDL_GpuTextureTransferInfo) {
			.transferBuffer = textureTransferBuffer,
			.offset = leftImageData->w * leftImageData->w * 4,
		},
		&(SDL_GpuTextureRegion){
			.texture = RightTexture,
			.w = rightImageData->w,
			.h = rightImageData->h,
			.d = 1
		},
		SDL_FALSE
	);

	SDL_DestroySurface(leftImageData);
	SDL_DestroySurface(rightImageData);
	SDL_GpuEndCopyPass(copyPass);
	SDL_GpuSubmit(uploadCmdBuf);
	SDL_GpuReleaseTransferBuffer(context->Device, bufferTransferBuffer);
	SDL_GpuReleaseTransferBuffer(context->Device, textureTransferBuffer);

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
		SDL_GpuCopyPass* copyPass;
		SDL_GpuRenderPass* renderPass;
		SDL_GpuColorAttachmentInfo colorAttachmentInfo = {
			.texture = swapchainTexture,
			.clearColor = (SDL_FColor){ 0.0f, 0.0f, 0.0f, 1.0f },
			.loadOp = SDL_GPU_LOADOP_CLEAR,
			.storeOp = SDL_GPU_STOREOP_STORE
		};

		// Copy left-side resources
		copyPass = SDL_GpuBeginCopyPass(cmdbuf);
		SDL_GpuCopyBufferToBuffer(
			copyPass,
			&(SDL_GpuBufferLocation){
				.buffer = LeftVertexBuffer,
			},
			&(SDL_GpuBufferLocation){
				.buffer = VertexBuffer,
			},
			sizeof(PositionTextureVertex) * 4,
			SDL_FALSE
		);
		SDL_GpuCopyTextureToTexture(
			copyPass,
			&(SDL_GpuTextureLocation){
				.texture = LeftTexture
			},
			&(SDL_GpuTextureLocation){
				.texture = Texture
			},
			16,
			16,
			1,
			SDL_FALSE
		);
		SDL_GpuEndCopyPass(copyPass);

		// Draw the left side
		renderPass = SDL_GpuBeginRenderPass(cmdbuf, &colorAttachmentInfo, 1, NULL);
		SDL_GpuBindGraphicsPipeline(renderPass, Pipeline);
		SDL_GpuBindVertexBuffers(renderPass, 0, &(SDL_GpuBufferBinding){ .buffer = VertexBuffer, .offset = 0 }, 1);
		SDL_GpuBindIndexBuffer(renderPass, &(SDL_GpuBufferBinding){ .buffer = IndexBuffer, .offset = 0 }, SDL_GPU_INDEXELEMENTSIZE_16BIT);
		SDL_GpuBindFragmentSamplers(renderPass, 0, &(SDL_GpuTextureSamplerBinding){ .texture = Texture, .sampler = Sampler }, 1);
		SDL_GpuDrawIndexedPrimitives(renderPass, 0, 0, 6, 1, 0);
		SDL_GpuEndRenderPass(renderPass);

		// Copy right-side resources
		copyPass = SDL_GpuBeginCopyPass(cmdbuf);
		SDL_GpuCopyBufferToBuffer(
			copyPass,
			&(SDL_GpuBufferLocation){
				.buffer = RightVertexBuffer,
			},
			&(SDL_GpuBufferLocation){
				.buffer = VertexBuffer,
			},
			sizeof(PositionTextureVertex) * 4,
			SDL_FALSE
		);
		SDL_GpuCopyTextureToTexture(
			copyPass,
			&(SDL_GpuTextureLocation){
				.texture = RightTexture
			},
			&(SDL_GpuTextureLocation){
				.texture = Texture
			},
			16,
			16,
			1,
			SDL_FALSE
		);
		SDL_GpuEndCopyPass(copyPass);

		// Draw the right side
		colorAttachmentInfo.loadOp = SDL_GPU_LOADOP_LOAD;
		renderPass = SDL_GpuBeginRenderPass(cmdbuf, &colorAttachmentInfo, 1, NULL);
		SDL_GpuBindGraphicsPipeline(renderPass, Pipeline);
		SDL_GpuBindVertexBuffers(renderPass, 0, &(SDL_GpuBufferBinding){ .buffer = VertexBuffer, .offset = 0 }, 1);
		SDL_GpuBindIndexBuffer(renderPass, &(SDL_GpuBufferBinding){ .buffer = IndexBuffer, .offset = 0 }, SDL_GPU_INDEXELEMENTSIZE_16BIT);
		SDL_GpuBindFragmentSamplers(renderPass, 0, &(SDL_GpuTextureSamplerBinding){ .texture = Texture, .sampler = Sampler }, 1);
		SDL_GpuDrawIndexedPrimitives(renderPass, 0, 0, 6, 1, 0);
		SDL_GpuEndRenderPass(renderPass);
	}

	SDL_GpuSubmit(cmdbuf);

	return 0;
}

static void Quit(Context* context)
{
	SDL_GpuReleaseGraphicsPipeline(context->Device, Pipeline);
	SDL_GpuReleaseBuffer(context->Device, VertexBuffer);
	SDL_GpuReleaseBuffer(context->Device, LeftVertexBuffer);
	SDL_GpuReleaseBuffer(context->Device, RightVertexBuffer);
	SDL_GpuReleaseBuffer(context->Device, IndexBuffer);
	SDL_GpuReleaseTexture(context->Device, Texture);
	SDL_GpuReleaseTexture(context->Device, LeftTexture);
	SDL_GpuReleaseTexture(context->Device, RightTexture);
	SDL_GpuReleaseSampler(context->Device, Sampler);

	CommonQuit(context);
}

Example CopyConsistency_Example = { "CopyConsistency", Init, Update, Draw, Quit };
