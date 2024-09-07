#include "Common.h"
#include <stdlib.h> // for srand

static SDL_GPUComputePipeline* ComputePipeline;
static SDL_GPUGraphicsPipeline* RenderPipeline;
static SDL_GPUSampler* Sampler;
static SDL_GPUTexture* Texture;
static SDL_GPUTransferBuffer* SpriteComputeTransferBuffer;
static SDL_GPUBuffer* SpriteComputeBuffer;
static SDL_GPUBuffer* SpriteVertexBuffer;
static SDL_GPUBuffer* SpriteIndexBuffer;

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

	SDL_GPUPresentMode presentMode = SDL_GPU_PRESENTMODE_VSYNC;
	if (SDL_WindowSupportsGPUPresentMode(
		context->Device,
		context->Window,
		SDL_GPU_PRESENTMODE_IMMEDIATE
	)) {
		presentMode = SDL_GPU_PRESENTMODE_IMMEDIATE;
	}
	else if (SDL_WindowSupportsGPUPresentMode(
		context->Device,
		context->Window,
		SDL_GPU_PRESENTMODE_MAILBOX
	)) {
		presentMode = SDL_GPU_PRESENTMODE_MAILBOX;
	}

	SDL_SetGPUSwapchainParameters(
		context->Device,
		context->Window,
		SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
		presentMode
	);

	srand(0);

	// Create the shaders
	SDL_GPUShader* vertShader = LoadShader(
		context->Device,
		"TexturedQuadColorWithMatrix.vert",
		0,
		1,
		0,
		0
	);

	SDL_GPUShader* fragShader = LoadShader(
		context->Device,
		"TexturedQuadColor.frag",
		1,
		0,
		0,
		0
	);

	// Create the sprite render pipeline
	RenderPipeline = SDL_CreateGPUGraphicsPipeline(
		context->Device,
		&(SDL_GPUGraphicsPipelineCreateInfo){
			.target_info = (SDL_GpuGraphicsPipelineTargetInfo){
				.num_color_targets = 1,
				.color_target_descriptions = (SDL_GPUColorTargetDescription[]){{
					.format = SDL_GetGPUSwapchainTextureFormat(context->Device, context->Window),
					.blend_state = (SDL_GPUColorTargetBlendState){
						.enable_blend = SDL_TRUE,
						.alpha_blend_op = SDL_GPU_BLENDOP_ADD,
						.color_blend_op = SDL_GPU_BLENDOP_ADD,
						.color_write_mask = 0xF,
						.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
						.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
						.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ZERO,
						.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO
					}
				}}
			},
			.vertex_input_state = (SDL_GPUVertexInputState){
				.num_vertex_bindings = 1,
				.vertex_bindings = (SDL_GPUVertexBinding[]){{
					.index = 0,
					.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
					.instance_step_rate = 0,
					.pitch = sizeof(PositionTextureColorVertex)
				}},
				.num_vertex_attributes = 3,
				.vertex_attributes = (SDL_GPUVertexAttribute[]){{
					.binding_index = 0,
					.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
					.location = 0,
					.offset = 0
				}, {
					.binding_index = 0,
					.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
					.location = 1,
					.offset = 16
				}, {
					.binding_index = 0,
					.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
					.location = 2,
					.offset = 32
				}}
			},
			.multisample_state.sample_mask = 0xFFFF,
			.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
			.vertex_shader = vertShader,
			.fragment_shader = fragShader
		}
	);

	SDL_ReleaseGPUShader(context->Device, vertShader);
	SDL_ReleaseGPUShader(context->Device, fragShader);

	// Create the sprite batch compute pipeline
	ComputePipeline = CreateComputePipelineFromShader(
		context->Device,
		"SpriteBatch.comp",
		&(SDL_GPUComputePipelineCreateInfo){
			.num_readonly_storage_buffers = 1,
			.num_writeonly_storage_buffers = 1,
			.threadcount_x = 64,
			.threadcount_y = 1,
			.threadcount_z = 1
		}
	);

	// Load the image data
	SDL_Surface *imageData = LoadImage("ravioli.bmp", 4);
	if (imageData == NULL)
	{
		SDL_Log("Could not load image data!");
		return -1;
	}

	SDL_GPUTransferBuffer* textureTransferBuffer = SDL_CreateGPUTransferBuffer(
		context->Device,
		&(SDL_GPUTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = imageData->w * imageData->h * 4
		}
	);

	Uint8 *textureTransferPtr = SDL_MapGPUTransferBuffer(
		context->Device,
		textureTransferBuffer,
		SDL_FALSE
	);
	SDL_memcpy(textureTransferPtr, imageData->pixels, imageData->w * imageData->h * 4);
	SDL_UnmapGPUTransferBuffer(context->Device, textureTransferBuffer);

	// Create the GPU resources
	Texture = SDL_CreateGPUTexture(
		context->Device,
		&(SDL_GPUTextureCreateInfo){
			.type = SDL_GPU_TEXTURETYPE_2D,
			.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
			.width = imageData->w,
			.height = imageData->h,
			.layer_count_or_depth = 1,
			.num_levels = 1,
			.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER
		}
	);

	Sampler = SDL_CreateGPUSampler(
		context->Device,
		&(SDL_GPUSamplerCreateInfo){
			.min_filter = SDL_GPU_FILTER_NEAREST,
			.mag_filter = SDL_GPU_FILTER_NEAREST,
			.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
			.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
			.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
			.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE
		}
	);

	SpriteComputeTransferBuffer = SDL_CreateGPUTransferBuffer(
		context->Device,
		&(SDL_GPUTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = SPRITE_COUNT * sizeof(ComputeSpriteInstance)
		}
	);

	SpriteComputeBuffer = SDL_CreateGPUBuffer(
		context->Device,
		&(SDL_GPUBufferCreateInfo) {
			.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ,
			.size = SPRITE_COUNT * sizeof(ComputeSpriteInstance)
		}
	);

	SpriteVertexBuffer = SDL_CreateGPUBuffer(
		context->Device,
		&(SDL_GPUBufferCreateInfo) {
			.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_BUFFERUSAGE_VERTEX,
			.size = SPRITE_COUNT * 4 * sizeof(PositionTextureColorVertex)
		}
	);

	SpriteIndexBuffer = SDL_CreateGPUBuffer(
		context->Device,
		&(SDL_GPUBufferCreateInfo) {
			.usage = SDL_GPU_BUFFERUSAGE_INDEX,
			.size = SPRITE_COUNT * 6 * sizeof(Uint32)
		}
	);

	// Transfer the up-front data
	SDL_GPUTransferBuffer* indexBufferTransferBuffer = SDL_CreateGPUTransferBuffer(
		context->Device,
		&(SDL_GPUTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = SPRITE_COUNT * 6 * sizeof(Uint32)
		}
	);

	Uint32* indexTransferPtr = SDL_MapGPUTransferBuffer(
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

	SDL_UnmapGPUTransferBuffer(
		context->Device,
		indexBufferTransferBuffer
	);

	SDL_GPUCommandBuffer* uploadCmdBuf = SDL_AcquireGPUCommandBuffer(context->Device);
	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);

	SDL_UploadToGPUTexture(
		copyPass,
		&(SDL_GPUTextureTransferInfo) {
			.transfer_buffer = textureTransferBuffer,
			.offset = 0, /* Zeroes out the rest */
		},
		&(SDL_GPUTextureRegion){
			.texture = Texture,
			.w = imageData->w,
			.h = imageData->h,
			.d = 1
		},
		SDL_FALSE
	);

	SDL_UploadToGPUBuffer(
		copyPass,
		&(SDL_GPUTransferBufferLocation) {
			.transfer_buffer = indexBufferTransferBuffer,
			.offset = 0
		},
		&(SDL_GPUBufferRegion) {
			.buffer = SpriteIndexBuffer,
			.offset = 0,
			.size = SPRITE_COUNT * 6 * sizeof(Uint32)
		},
		SDL_FALSE
	);

	SDL_DestroySurface(imageData);
	SDL_EndGPUCopyPass(copyPass);
	SDL_SubmitGPUCommandBuffer(uploadCmdBuf);
	SDL_ReleaseGPUTransferBuffer(context->Device, textureTransferBuffer);
	SDL_ReleaseGPUTransferBuffer(context->Device, indexBufferTransferBuffer);

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

	SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(context->Device);
	SDL_GPUTexture* swapchainTexture = SDL_AcquireGPUSwapchainTexture(
		cmdBuf,
		context->Window,
		&w,
		&h
	);

	if (swapchainTexture != NULL)
	{
		// Build sprite instance transfer
		ComputeSpriteInstance* dataPtr = SDL_MapGPUTransferBuffer(
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

		SDL_UnmapGPUTransferBuffer(context->Device, SpriteComputeTransferBuffer);

		// Upload instance data
		SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);
		SDL_UploadToGPUBuffer(
			copyPass,
			&(SDL_GPUTransferBufferLocation) {
				.transfer_buffer = SpriteComputeTransferBuffer,
				.offset = 0
			},
			&(SDL_GPUBufferRegion) {
				.buffer = SpriteComputeBuffer,
				.offset = 0,
				.size = SPRITE_COUNT * sizeof(ComputeSpriteInstance)
			},
			SDL_TRUE
		);
		SDL_EndGPUCopyPass(copyPass);

		// Set up compute pass to build vertex buffer
		SDL_GPUComputePass* computePass = SDL_BeginGPUComputePass(
			cmdBuf,
			NULL,
			0,
			&(SDL_GPUStorageBufferWriteOnlyBinding){
				.buffer = SpriteVertexBuffer,
				.cycle = SDL_TRUE
			},
			1
		);

		SDL_BindGPUComputePipeline(computePass, ComputePipeline);
		SDL_BindGPUComputeStorageBuffers(
			computePass,
			0,
			&(SDL_GPUBuffer*){
				SpriteComputeBuffer,
			},
			1
		);
		SDL_DispatchGPUCompute(computePass, SPRITE_COUNT / 64, 1, 1);

		SDL_EndGPUComputePass(computePass);

		// Render sprites
		SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(
			cmdBuf,
			&(SDL_GPUColorTargetInfo){
				.texture = swapchainTexture,
				.cycle = SDL_FALSE,
				.load_op = SDL_GPU_LOADOP_CLEAR,
				.store_op = SDL_GPU_STOREOP_STORE,
				.clear_color = { 0, 0, 0, 1 }
			},
			1,
			NULL
		);

		SDL_BindGPUGraphicsPipeline(renderPass, RenderPipeline);
		SDL_BindGPUVertexBuffers(
			renderPass,
			0,
			&(SDL_GPUBufferBinding){
				.buffer = SpriteVertexBuffer
			},
			1
		);
		SDL_BindGPUIndexBuffer(
			renderPass,
			&(SDL_GPUBufferBinding){
				.buffer = SpriteIndexBuffer
			},
			SDL_GPU_INDEXELEMENTSIZE_32BIT
		);
		SDL_BindGPUFragmentSamplers(
			renderPass,
			0,
			&(SDL_GPUTextureSamplerBinding){
				.texture = Texture,
				.sampler = Sampler
			},
			1
		);
		SDL_PushGPUVertexUniformData(
			cmdBuf,
			0,
			&cameraMatrix,
			sizeof(Matrix4x4)
		);
		SDL_DrawGPUIndexedPrimitives(
			renderPass,
			SPRITE_COUNT * 6,
			1,
			0,
			0,
			0
		);

		SDL_EndGPURenderPass(renderPass);
	}

	SDL_SubmitGPUCommandBuffer(cmdBuf);

	return 0;
}

static void Quit(Context* context)
{
	SDL_ReleaseGPUComputePipeline(context->Device, ComputePipeline);
	SDL_ReleaseGPUGraphicsPipeline(context->Device, RenderPipeline);
	SDL_ReleaseGPUSampler(context->Device, Sampler);
	SDL_ReleaseGPUTexture(context->Device, Texture);
	SDL_ReleaseGPUTransferBuffer(context->Device, SpriteComputeTransferBuffer);
	SDL_ReleaseGPUBuffer(context->Device, SpriteComputeBuffer);
	SDL_ReleaseGPUBuffer(context->Device, SpriteVertexBuffer);
	SDL_ReleaseGPUBuffer(context->Device, SpriteIndexBuffer);

	CommonQuit(context);
}

Example ComputeSpriteBatch_Example = { "ComputeSpriteBatch", Init, Update, Draw, Quit };
