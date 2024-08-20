#include "Common.h"

static SDL_GpuTexture* Texture;

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
	Texture = SDL_GpuCreateTexture(
		context->Device,
		&(SDL_GpuTextureCreateInfo){
			.type = SDL_GPU_TEXTURETYPE_2D,
			.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8,
			.width = imageData->w,
			.height = imageData->h,
			.layerCountOrDepth = 1,
			.levelCount = 1,
			.usageFlags = SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT
		}
	);

	SDL_GpuTransferBuffer* uploadTransferBuffer = SDL_GpuCreateTransferBuffer(
		context->Device,
		&(SDL_GpuTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.sizeInBytes = imageData->w * imageData->h * 4
		}
	);

	Uint8* uploadTransferPtr = SDL_GpuMapTransferBuffer(
		context->Device,
		uploadTransferBuffer,
		SDL_FALSE
	);
	SDL_memcpy(uploadTransferPtr, imageData->pixels, imageData->w * imageData->h * 4);
	SDL_GpuUnmapTransferBuffer(context->Device, uploadTransferBuffer);

	SDL_GpuCommandBuffer* cmdbuf = SDL_GpuAcquireCommandBuffer(context->Device);
	SDL_GpuCopyPass* copyPass = SDL_GpuBeginCopyPass(cmdbuf);
	SDL_GpuUploadToTexture(
		copyPass,
		&(SDL_GpuTextureTransferInfo) {
			.transferBuffer = uploadTransferBuffer,
		},
		&(SDL_GpuTextureRegion){
			.texture = Texture,
			.w = TextureWidth,
			.h = TextureHeight,
			.d = 1
		},
		SDL_FALSE
	);
	SDL_GpuEndCopyPass(copyPass);
	SDL_GpuSubmit(cmdbuf);

	SDL_GpuReleaseTransferBuffer(context->Device, uploadTransferBuffer);
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
	SDL_GpuCommandBuffer* cmdbuf = SDL_GpuAcquireCommandBuffer(context->Device);
	SDL_GpuTexture* swapchainTexture = SDL_GpuAcquireSwapchainTexture(cmdbuf, context->Window, &w, &h);
	if (swapchainTexture != NULL)
	{
		SDL_GpuRenderPass* clearPass = SDL_GpuBeginRenderPass(
			cmdbuf,
			(SDL_GpuColorAttachmentInfo[]){{
				.texture = swapchainTexture,
				.loadOp = SDL_GPU_LOADOP_CLEAR,
				.storeOp = SDL_GPU_STOREOP_STORE,
				.clearColor = { 0, 0, 0, 1 },
				.cycle = SDL_FALSE
			}},
			1,
			NULL
		);
		SDL_GpuEndRenderPass(clearPass);

		// Normal
		SDL_GpuBlit(
			cmdbuf,
			&(SDL_GpuBlitRegion){
				.texture = Texture,
				.w = TextureWidth,
				.h = TextureHeight,
			},
			&(SDL_GpuBlitRegion){
				.texture = swapchainTexture,
				.w = w / 2,
				.h = h / 2,
			},
			SDL_FLIP_NONE,
			SDL_GPU_FILTER_NEAREST,
			SDL_FALSE
		);

		// Flipped Horizontally
		SDL_GpuBlit(
			cmdbuf,
			&(SDL_GpuBlitRegion){
				.texture = Texture,
				.w = TextureWidth,
				.h = TextureHeight,
			},
			&(SDL_GpuBlitRegion) {
			.texture = swapchainTexture,
				.x = w / 2,
				.w = w / 2,
				.h = h / 2,
			},
			SDL_FLIP_HORIZONTAL,
			SDL_GPU_FILTER_NEAREST,
			SDL_FALSE
		);

		// Flipped Vertically
		SDL_GpuBlit(
			cmdbuf,
			&(SDL_GpuBlitRegion){
				.texture = Texture,
				.w = TextureWidth,
				.h = TextureHeight,
			},
			&(SDL_GpuBlitRegion) {
			.texture = swapchainTexture,
				.w = w / 2,
				.y = h / 2,
				.h = h / 2,
			},
			SDL_FLIP_VERTICAL,
			SDL_GPU_FILTER_NEAREST,
			SDL_FALSE
		);

		// Flipped Horizontally and Vertically
		SDL_GpuBlit(
			cmdbuf,
			&(SDL_GpuBlitRegion){
				.texture = Texture,
				.w = TextureWidth,
				.h = TextureHeight,
			},
			&(SDL_GpuBlitRegion) {
			.texture = swapchainTexture,
				.x = w / 2,
				.w = w / 2,
				.y = h / 2,
				.h = h / 2,
			},
			SDL_FLIP_HORIZONTAL | SDL_FLIP_VERTICAL,
			SDL_GPU_FILTER_NEAREST,
			SDL_FALSE
		);
	}

	SDL_GpuSubmit(cmdbuf);

	return 0;
}

static void Quit(Context* context)
{
	SDL_GpuReleaseTexture(context->Device, Texture);

	CommonQuit(context);
}

Example BlitMirror_Example = { "BlitMirror", Init, Update, Draw, Quit };
