#include "Common.h"
#include <stdlib.h> // for srand

static SDL_GpuComputePipeline* ComputePipeline;
static SDL_GpuGraphicsPipeline* RenderPipeline;
static SDL_GpuSampler* Sampler;
static SDL_GpuTexture* Texture;
static SDL_GpuTransferBuffer* SpriteComputeTransferBuffer;
static SDL_GpuBuffer* SpriteComputeBuffer;
static SDL_GpuBuffer* SpriteVertexBuffer;
static SDL_GpuBuffer* SpriteIndexBuffer;

typedef struct PositionTextureColorVertex
{
	float x, y, z, w;
	float u, v, padding_a, padding_b;
	float r, g, b, a;
} PositionTextureColorVertex;

typedef struct ComputeSpriteInstance
{
	float x, y, z;
	float rotation;
	float w, h, padding_a, padding_b;
	float r, g, b, a;
} ComputeSpriteInstance;

const Uint32 SPRITE_COUNT = 8192;

static int Init(Context* context)
{
	int result = CommonInit(context, 0);
	if (result < 0)
	{
		return result;
	}

	SDL_GpuPresentMode presentMode = SDL_GPU_PRESENTMODE_VSYNC;
	if (SDL_GpuSupportsPresentMode(
		context->Device,
		context->Window,
		SDL_GPU_PRESENTMODE_IMMEDIATE
	)) {
		presentMode = SDL_GPU_PRESENTMODE_IMMEDIATE;
	}
	else if (SDL_GpuSupportsPresentMode(
		context->Device,
		context->Window,
		SDL_GPU_PRESENTMODE_MAILBOX
	)) {
		presentMode = SDL_GPU_PRESENTMODE_MAILBOX;
	}

	SDL_GpuSetSwapchainParameters(
		context->Device,
		context->Window,
		SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
		presentMode
	);

	srand(0);

	// Create the shaders
	SDL_GpuShader* vertShader = LoadShader(
		context->Device,
		"TexturedQuadColorWithMatrix.vert",
		0,
		1,
		0,
		0
	);

	SDL_GpuShader* fragShader = LoadShader(
		context->Device,
		"TexturedQuadColor.frag",
		1,
		0,
		0,
		0
	);

	// Create the sprite render pipeline
	RenderPipeline = SDL_GpuCreateGraphicsPipeline(
		context->Device,
		&(SDL_GpuGraphicsPipelineCreateInfo){
			.attachmentInfo = (SDL_GpuGraphicsPipelineAttachmentInfo){
				.colorAttachmentCount = 1,
				.colorAttachmentDescriptions = (SDL_GpuColorAttachmentDescription[]){{
					.format = SDL_GpuGetSwapchainTextureFormat(context->Device, context->Window),
					.blendState = (SDL_GpuColorAttachmentBlendState){
						.blendEnable = SDL_TRUE,
						.alphaBlendOp = SDL_GPU_BLENDOP_ADD,
						.colorBlendOp = SDL_GPU_BLENDOP_ADD,
						.colorWriteMask = 0xF,
						.srcColorBlendFactor = SDL_GPU_BLENDFACTOR_ONE,
						.srcAlphaBlendFactor = SDL_GPU_BLENDFACTOR_ONE,
						.dstColorBlendFactor = SDL_GPU_BLENDFACTOR_ZERO,
						.dstAlphaBlendFactor = SDL_GPU_BLENDFACTOR_ZERO
					}
				}}
			},
			.vertexInputState = (SDL_GpuVertexInputState){
				.vertexBindingCount = 1,
				.vertexBindings = (SDL_GpuVertexBinding[]){{
					.binding = 0,
					.inputRate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
					.instanceStepRate = 0,
					.stride = sizeof(PositionTextureColorVertex)
				}},
				.vertexAttributeCount = 3,
				.vertexAttributes = (SDL_GpuVertexAttribute[]){{
					.binding = 0,
					.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
					.location = 0,
					.offset = 0
				}, {
					.binding = 0,
					.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
					.location = 1,
					.offset = 16
				}, {
					.binding = 0,
					.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
					.location = 2,
					.offset = 32
				}}
			},
			.multisampleState.sampleMask = 0xFFFF,
			.primitiveType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
			.vertexShader = vertShader,
			.fragmentShader = fragShader
		}
	);

	SDL_GpuReleaseShader(context->Device, vertShader);
	SDL_GpuReleaseShader(context->Device, fragShader);

	// Create the sprite batch compute pipeline
	ComputePipeline = CreateComputePipelineFromShader(
		context->Device,
		"SpriteBatch.comp",
		&(SDL_GpuComputePipelineCreateInfo){
			.readOnlyStorageBufferCount = 1,
			.writeOnlyStorageBufferCount = 1,
			.threadCountX = 64,
			.threadCountY = 1,
			.threadCountZ = 1
		}
	);

	// Load the image data
	SDL_Surface *imageData = LoadImage("ravioli.bmp", 4);
	if (imageData == NULL)
	{
		SDL_Log("Could not load image data!");
		return -1;
	}

	SDL_GpuTransferBuffer* textureTransferBuffer = SDL_GpuCreateTransferBuffer(
		context->Device,
		&(SDL_GpuTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.sizeInBytes = imageData->w * imageData->h * 4
		}
	);

	Uint8 *textureTransferPtr = SDL_GpuMapTransferBuffer(
		context->Device,
		textureTransferBuffer,
		SDL_FALSE
	);
	SDL_memcpy(textureTransferPtr, imageData->pixels, imageData->w * imageData->h * 4);
	SDL_GpuUnmapTransferBuffer(context->Device, textureTransferBuffer);

	// Create the GPU resources
	Texture = SDL_GpuCreateTexture(
		context->Device,
		&(SDL_GpuTextureCreateInfo){
			.type = SDL_GPU_TEXTURETYPE_2D,
			.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
			.width = imageData->w,
			.height = imageData->h,
			.layerCountOrDepth = 1,
			.levelCount = 1,
			.usageFlags = SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT
		}
	);

	Sampler = SDL_GpuCreateSampler(
		context->Device,
		&(SDL_GpuSamplerCreateInfo){
			.minFilter = SDL_GPU_FILTER_NEAREST,
			.magFilter = SDL_GPU_FILTER_NEAREST,
			.mipmapMode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
			.addressModeU = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
			.addressModeV = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
			.addressModeW = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE
		}
	);

	SpriteComputeTransferBuffer = SDL_GpuCreateTransferBuffer(
		context->Device,
		&(SDL_GpuTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.sizeInBytes = SPRITE_COUNT * sizeof(ComputeSpriteInstance)
		}
	);

	SpriteComputeBuffer = SDL_GpuCreateBuffer(
		context->Device,
		&(SDL_GpuBufferCreateInfo) {
			.usageFlags = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ_BIT,
			.sizeInBytes = SPRITE_COUNT * sizeof(ComputeSpriteInstance)
		}
	);

	SpriteVertexBuffer = SDL_GpuCreateBuffer(
		context->Device,
		&(SDL_GpuBufferCreateInfo) {
			.usageFlags = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE_BIT | SDL_GPU_BUFFERUSAGE_VERTEX_BIT,
			.sizeInBytes = SPRITE_COUNT * 4 * sizeof(PositionTextureColorVertex)
		}
	);

	SpriteIndexBuffer = SDL_GpuCreateBuffer(
		context->Device,
		&(SDL_GpuBufferCreateInfo) {
			.usageFlags = SDL_GPU_BUFFERUSAGE_INDEX_BIT,
			.sizeInBytes = SPRITE_COUNT * 6 * sizeof(Uint32)
		}
	);

	// Transfer the up-front data
	SDL_GpuTransferBuffer* indexBufferTransferBuffer = SDL_GpuCreateTransferBuffer(
		context->Device,
		&(SDL_GpuTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.sizeInBytes = SPRITE_COUNT * 6 * sizeof(Uint32)
		}
	);

	Uint32* indexTransferPtr = SDL_GpuMapTransferBuffer(
		context->Device,
		indexBufferTransferBuffer,
		SDL_FALSE
	);

	for (Uint32 i = 0, j = 0; i < SPRITE_COUNT * 6; i += 6, j += 4)
	{
		indexTransferPtr[i]     =  j;
		indexTransferPtr[i + 1] =  j + 1;
		indexTransferPtr[i + 2] =  j + 2;
		indexTransferPtr[i + 3] =  j + 3;
		indexTransferPtr[i + 4] =  j + 2;
		indexTransferPtr[i + 5] =  j + 1;
	}

	SDL_GpuUnmapTransferBuffer(
		context->Device,
		indexBufferTransferBuffer
	);

	SDL_GpuCommandBuffer* uploadCmdBuf = SDL_GpuAcquireCommandBuffer(context->Device);
	SDL_GpuCopyPass* copyPass = SDL_GpuBeginCopyPass(uploadCmdBuf);

	SDL_GpuUploadToTexture(
		copyPass,
		&(SDL_GpuTextureTransferInfo) {
			.transferBuffer = textureTransferBuffer,
			.offset = 0, /* Zeroes out the rest */
		},
		&(SDL_GpuTextureRegion){
			.texture = Texture,
			.w = imageData->w,
			.h = imageData->h,
			.d = 1
		},
		SDL_FALSE
	);

	SDL_GpuUploadToBuffer(
		copyPass,
		&(SDL_GpuTransferBufferLocation) {
			.transferBuffer = indexBufferTransferBuffer,
			.offset = 0
		},
		&(SDL_GpuBufferRegion) {
			.buffer = SpriteIndexBuffer,
			.offset = 0,
			.size = SPRITE_COUNT * 6 * sizeof(Uint32)
		},
		SDL_FALSE
	);

	SDL_DestroySurface(imageData);
	SDL_GpuEndCopyPass(copyPass);
	SDL_GpuSubmit(uploadCmdBuf);
	SDL_GpuReleaseTransferBuffer(context->Device, textureTransferBuffer);
	SDL_GpuReleaseTransferBuffer(context->Device, indexBufferTransferBuffer);

	return 0;
}

static int Update(Context* context)
{
	return 0;
}

static int Draw(Context* context)
{
	Matrix4x4 cameraMatrix = Matrix4x4_CreateOrthographicOffCenter(
		0,
		640,
		480,
		0,
		0,
		-1
	);

	Uint32 w, h;

	SDL_GpuCommandBuffer* cmdBuf = SDL_GpuAcquireCommandBuffer(context->Device);
	SDL_GpuTexture* swapchainTexture = SDL_GpuAcquireSwapchainTexture(
		cmdBuf,
		context->Window,
		&w,
		&h
	);

	if (swapchainTexture != NULL)
	{
		// Build sprite instance transfer
		ComputeSpriteInstance* dataPtr = SDL_GpuMapTransferBuffer(
			context->Device,
			SpriteComputeTransferBuffer,
			SDL_TRUE
		);

		for (Uint32 i = 0; i < SPRITE_COUNT; i += 1)
		{
			dataPtr[i].x = (float)(rand() % 640);
			dataPtr[i].y = (float)(rand() % 480);
			dataPtr[i].z = 0;
			dataPtr[i].w = 1;
			dataPtr[i].rotation = ((float)rand())/(RAND_MAX/(SDL_PI_F * 2));
			dataPtr[i].w = 32;
			dataPtr[i].h = 32;
			dataPtr[i].r = 1.0f;
			dataPtr[i].g = 1.0f;
			dataPtr[i].b = 1.0f;
			dataPtr[i].a = 1.0f;
		}

		SDL_GpuUnmapTransferBuffer(context->Device, SpriteComputeTransferBuffer);

		// Upload instance data
		SDL_GpuCopyPass* copyPass = SDL_GpuBeginCopyPass(cmdBuf);
		SDL_GpuUploadToBuffer(
			copyPass,
			&(SDL_GpuTransferBufferLocation) {
				.transferBuffer = SpriteComputeTransferBuffer,
				.offset = 0
			},
			&(SDL_GpuBufferRegion) {
				.buffer = SpriteComputeBuffer,
				.offset = 0,
				.size = SPRITE_COUNT * sizeof(ComputeSpriteInstance)
			},
			SDL_TRUE
		);
		SDL_GpuEndCopyPass(copyPass);

		// Set up compute pass to build vertex buffer
		SDL_GpuComputePass* computePass = SDL_GpuBeginComputePass(
			cmdBuf,
			NULL,
			0,
			&(SDL_GpuStorageBufferWriteOnlyBinding){
				.buffer = SpriteVertexBuffer,
				.cycle = SDL_TRUE
			},
			1
		);

		SDL_GpuBindComputePipeline(computePass, ComputePipeline);
		SDL_GpuBindComputeStorageBuffers(
			computePass,
			0,
			&(SDL_GpuBuffer*){
				SpriteComputeBuffer,
			},
			1
		);
		SDL_GpuDispatchCompute(computePass, SPRITE_COUNT / 64, 1, 1);

		SDL_GpuEndComputePass(computePass);

		// Render sprites
		SDL_GpuRenderPass* renderPass = SDL_GpuBeginRenderPass(
			cmdBuf,
			&(SDL_GpuColorAttachmentInfo){
				.texture = swapchainTexture,
				.cycle = SDL_FALSE,
				.loadOp = SDL_GPU_LOADOP_CLEAR,
				.storeOp = SDL_GPU_STOREOP_STORE,
				.clearColor = { 0, 0, 0, 1 }
			},
			1,
			NULL
		);

		SDL_GpuBindGraphicsPipeline(renderPass, RenderPipeline);
		SDL_GpuBindVertexBuffers(
			renderPass,
			0,
			&(SDL_GpuBufferBinding){
				.buffer = SpriteVertexBuffer
			},
			1
		);
		SDL_GpuBindIndexBuffer(
			renderPass,
			&(SDL_GpuBufferBinding){
				.buffer = SpriteIndexBuffer
			},
			SDL_GPU_INDEXELEMENTSIZE_32BIT
		);
		SDL_GpuBindFragmentSamplers(
			renderPass,
			0,
			&(SDL_GpuTextureSamplerBinding){
				.texture = Texture,
				.sampler = Sampler
			},
			1
		);
		SDL_GpuPushVertexUniformData(
			cmdBuf,
			0,
			&cameraMatrix,
			sizeof(Matrix4x4)
		);
		SDL_GpuDrawIndexedPrimitives(
			renderPass,
			SPRITE_COUNT * 6,
			1,
			0,
			0,
			0
		);

		SDL_GpuEndRenderPass(renderPass);
	}

	SDL_GpuSubmit(cmdBuf);

	return 0;
}

static void Quit(Context* context)
{
	SDL_GpuReleaseComputePipeline(context->Device, ComputePipeline);
	SDL_GpuReleaseGraphicsPipeline(context->Device, RenderPipeline);
	SDL_GpuReleaseSampler(context->Device, Sampler);
	SDL_GpuReleaseTexture(context->Device, Texture);
	SDL_GpuReleaseTransferBuffer(context->Device, SpriteComputeTransferBuffer);
	SDL_GpuReleaseBuffer(context->Device, SpriteComputeBuffer);
	SDL_GpuReleaseBuffer(context->Device, SpriteVertexBuffer);
	SDL_GpuReleaseBuffer(context->Device, SpriteIndexBuffer);

	CommonQuit(context);
}

Example ComputeSpriteBatch_Example = { "ComputeSpriteBatch", Init, Update, Draw, Quit };
