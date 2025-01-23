#include "Common.h"

static SDL_GPUGraphicsPipeline* RenderPipeline;
static SDL_GPUSampler* Sampler;
static SDL_GPUTexture* Texture;
static SDL_GPUTransferBuffer* SpriteDataTransferBuffer;
static SDL_GPUBuffer* SpriteDataBuffer;

typedef struct SpriteInstance
{
	float x, y, z;
	float rotation;
	float w, h, padding_a, padding_b;
	float tex_u, tex_v, tex_w, tex_h;
	float r, g, b, a;
} SpriteInstance;

static const Uint32 SPRITE_COUNT = 8192;

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

	SDL_srand(0);

	// Create the shaders
	SDL_GPUShader* vertShader = LoadShader(
		context->Device,
		"PullSpriteBatch.vert",
		0,
		1,
		1,
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
			.target_info = (SDL_GPUGraphicsPipelineTargetInfo){
				.num_color_targets = 1,
				.color_target_descriptions = (SDL_GPUColorTargetDescription[]){{
					.format = SDL_GetGPUSwapchainTextureFormat(context->Device, context->Window),
					.blend_state = {
						.enable_blend = true,
						.color_blend_op = SDL_GPU_BLENDOP_ADD,
						.alpha_blend_op = SDL_GPU_BLENDOP_ADD,
						.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
						.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
						.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
						.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
					}
				}}
			},
			.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
			.vertex_shader = vertShader,
			.fragment_shader = fragShader
		}
	);

	SDL_ReleaseGPUShader(context->Device, vertShader);
	SDL_ReleaseGPUShader(context->Device, fragShader);

	// Load the image data
	SDL_Surface *imageData = LoadImage("ravioli_atlas.bmp", 4);
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
		false
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

	SpriteDataTransferBuffer = SDL_CreateGPUTransferBuffer(
		context->Device,
		&(SDL_GPUTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = SPRITE_COUNT * sizeof(SpriteInstance)
		}
	);

	SpriteDataBuffer = SDL_CreateGPUBuffer(
		context->Device,
		&(SDL_GPUBufferCreateInfo) {
			.usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
			.size = SPRITE_COUNT * sizeof(SpriteInstance)
		}
	);

	// Transfer the up-front data
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
		false
	);

	SDL_EndGPUCopyPass(copyPass);
	SDL_SubmitGPUCommandBuffer(uploadCmdBuf);

    SDL_DestroySurface(imageData);
	SDL_ReleaseGPUTransferBuffer(context->Device, textureTransferBuffer);

	return 0;
}

static int Update(Context* context)
{
	return 0;
}

static float uCoords[4] = { 0.0f, 0.5f, 0.0f, 0.5f };
static float vCoords[4] = { 0.0f, 0.0f, 0.5f, 0.5f };

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

    SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(context->Device);
    if (cmdBuf == NULL)
    {
        SDL_Log("AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return -1;
    }

    SDL_GPUTexture* swapchainTexture;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdBuf, context->Window, &swapchainTexture, NULL, NULL)) {
        SDL_Log("WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
        return -1;
    }

	if (swapchainTexture != NULL)
	{
		// Build sprite instance transfer
		SpriteInstance* dataPtr = SDL_MapGPUTransferBuffer(
			context->Device,
			SpriteDataTransferBuffer,
			true
		);

		for (Uint32 i = 0; i < SPRITE_COUNT; i += 1)
		{
			int ravioli = SDL_rand(4);
			dataPtr[i].x = (float)(SDL_rand(640));
			dataPtr[i].y = (float)(SDL_rand(480));
			dataPtr[i].z = 0;
			dataPtr[i].rotation = SDL_randf() * SDL_PI_F * 2;
			dataPtr[i].w = 32;
			dataPtr[i].h = 32;
			dataPtr[i].tex_u = uCoords[ravioli];
			dataPtr[i].tex_v = vCoords[ravioli];
			dataPtr[i].tex_w = 0.5f;
			dataPtr[i].tex_h = 0.5f;
			dataPtr[i].r = 1.0f;
			dataPtr[i].g = 1.0f;
			dataPtr[i].b = 1.0f;
			dataPtr[i].a = 1.0f;
		}

		SDL_UnmapGPUTransferBuffer(context->Device, SpriteDataTransferBuffer);

		// Upload instance data
		SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);
		SDL_UploadToGPUBuffer(
			copyPass,
			&(SDL_GPUTransferBufferLocation) {
				.transfer_buffer = SpriteDataTransferBuffer,
				.offset = 0
			},
			&(SDL_GPUBufferRegion) {
				.buffer = SpriteDataBuffer,
				.offset = 0,
				.size = SPRITE_COUNT * sizeof(SpriteInstance)
			},
			true
		);
		SDL_EndGPUCopyPass(copyPass);

		// Render sprites
		SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(
			cmdBuf,
			&(SDL_GPUColorTargetInfo){
				.texture = swapchainTexture,
				.cycle = false,
				.load_op = SDL_GPU_LOADOP_CLEAR,
				.store_op = SDL_GPU_STOREOP_STORE,
				.clear_color = { 0, 0, 0, 1 }
			},
			1,
			NULL
		);

		SDL_BindGPUGraphicsPipeline(renderPass, RenderPipeline);
		SDL_BindGPUVertexStorageBuffers(
			renderPass,
			0,
            &SpriteDataBuffer,
			1
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
		SDL_DrawGPUPrimitives(
			renderPass,
			SPRITE_COUNT * 6,
			1,
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
	SDL_ReleaseGPUGraphicsPipeline(context->Device, RenderPipeline);
	SDL_ReleaseGPUSampler(context->Device, Sampler);
	SDL_ReleaseGPUTexture(context->Device, Texture);
	SDL_ReleaseGPUTransferBuffer(context->Device, SpriteDataTransferBuffer);
	SDL_ReleaseGPUBuffer(context->Device, SpriteDataBuffer);

	CommonQuit(context);
}

Example PullSpriteBatch_Example = { "PullSpriteBatch", Init, Update, Draw, Quit };
