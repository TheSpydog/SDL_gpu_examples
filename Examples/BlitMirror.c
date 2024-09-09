#include "Common.h"

static SDL_GPUTexture* Texture;

static Uint32 TextureWidth, TextureHeight;

static int Init(Context* context)
{
	int result = CommonInit(context, 0);
	if (result < 0)
	{
		return result;
	}

	// Load the image
	SDL_Surface *imageData = LoadImage("ravioli.bmp", 4);
	if (imageData == NULL)
	{
		SDL_Log("Could not load image data!");
		return -1;
	}

	TextureWidth = imageData->w;
	TextureHeight = imageData->h;

	// Create texture resource
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

	SDL_GPUTransferBuffer* uploadTransferBuffer = SDL_CreateGPUTransferBuffer(
		context->Device,
		&(SDL_GPUTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = imageData->w * imageData->h * 4
		}
	);

	Uint8* uploadTransferPtr = SDL_MapGPUTransferBuffer(
		context->Device,
		uploadTransferBuffer,
		SDL_FALSE
	);
	SDL_memcpy(uploadTransferPtr, imageData->pixels, imageData->w * imageData->h * 4);
	SDL_UnmapGPUTransferBuffer(context->Device, uploadTransferBuffer);

	SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(context->Device);
	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdbuf);
	SDL_UploadToGPUTexture(
		copyPass,
		&(SDL_GPUTextureTransferInfo) {
			.transfer_buffer = uploadTransferBuffer,
		},
		&(SDL_GPUTextureRegion){
			.texture = Texture,
			.w = TextureWidth,
			.h = TextureHeight,
			.d = 1
		},
		SDL_FALSE
	);
	SDL_EndGPUCopyPass(copyPass);
	SDL_SubmitGPUCommandBuffer(cmdbuf);

	SDL_ReleaseGPUTransferBuffer(context->Device, uploadTransferBuffer);
	SDL_DestroySurface(imageData);

	return 0;
}

static int Update(Context* context)
{
	return 0;
}

static int Draw(Context* context)
{
	Uint32 w, h;
	SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(context->Device);
	SDL_GPUTexture* swapchainTexture = SDL_AcquireGPUSwapchainTexture(cmdbuf, context->Window, &w, &h);
	if (swapchainTexture != NULL)
	{
		SDL_GPURenderPass* clearPass = SDL_BeginGPURenderPass(
			cmdbuf,
			(SDL_GPUColorTargetInfo[]){{
				.texture = swapchainTexture,
				.load_op = SDL_GPU_LOADOP_CLEAR,
				.store_op = SDL_GPU_STOREOP_STORE,
				.clear_color = { 0, 0, 0, 1 },
				.cycle = SDL_FALSE
			}},
			1,
			NULL
		);
		SDL_EndGPURenderPass(clearPass);

		// Normal
		SDL_BlitGPUTexture(
			cmdbuf,
			&(SDL_GPUBlitInfo){
				.source.texture = Texture,
				.source.w = TextureWidth,
				.source.h = TextureHeight,
				.destination.texture = swapchainTexture,
				.destination.w = w / 2,
				.destination.h = h / 2,
				.load_op = SDL_GPU_LOADOP_DONT_CARE,
				.filter = SDL_GPU_FILTER_NEAREST
			}
		);

		// Flipped Horizontally
		SDL_BlitGPUTexture(
			cmdbuf,
			&(SDL_GPUBlitInfo){
				.source.texture = Texture,
				.source.w = TextureWidth,
				.source.h = TextureHeight,
				.destination.texture = swapchainTexture,
				.destination.x = w / 2,
				.destination.w = w / 2,
				.destination.h = h / 2,
				.load_op = SDL_GPU_LOADOP_LOAD,
				.flip_mode = SDL_FLIP_HORIZONTAL,
				.filter = SDL_GPU_FILTER_NEAREST
			}
		);

		// Flipped Vertically
		SDL_BlitGPUTexture(
			cmdbuf,
			&(SDL_GPUBlitInfo){
				.source.texture = Texture,
				.source.w = TextureWidth,
				.source.h = TextureHeight,
				.destination.texture = swapchainTexture,
				.destination.w = w / 2,
				.destination.y = h / 2,
				.destination.h = h / 2,
				.load_op = SDL_GPU_LOADOP_LOAD,
				.flip_mode = SDL_FLIP_VERTICAL,
				.filter = SDL_GPU_FILTER_NEAREST
			}
		);

		// Flipped Horizontally and Vertically
		SDL_BlitGPUTexture(
			cmdbuf,
			&(SDL_GPUBlitInfo){
				.source.texture = Texture,
				.source.w = TextureWidth,
				.source.h = TextureHeight,
				.destination.texture = swapchainTexture,
				.destination.x = w / 2,
				.destination.w = w / 2,
				.destination.y = h / 2,
				.destination.h = h / 2,
				.load_op = SDL_GPU_LOADOP_LOAD,
				.flip_mode = SDL_FLIP_HORIZONTAL | SDL_FLIP_VERTICAL,
				.filter = SDL_GPU_FILTER_NEAREST
			}
		);
	}

	SDL_SubmitGPUCommandBuffer(cmdbuf);

	return 0;
}

static void Quit(Context* context)
{
	SDL_ReleaseGPUTexture(context->Device, Texture);

	CommonQuit(context);
}

Example BlitMirror_Example = { "BlitMirror", Init, Update, Draw, Quit };
