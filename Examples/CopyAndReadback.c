#include "Common.h"

static SDL_GPUTexture* OriginalTexture;
static SDL_GPUTexture* TextureCopy;
static SDL_GPUTexture* TextureSmall;

static SDL_GPUBuffer* OriginalBuffer;
static SDL_GPUBuffer* BufferCopy;

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

	// Create texture resources
	OriginalTexture = SDL_CreateGPUTexture(
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

	TextureCopy = SDL_CreateGPUTexture(
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

	TextureSmall = SDL_CreateGPUTexture(
		context->Device,
		&(SDL_GPUTextureCreateInfo){
			.type = SDL_GPU_TEXTURETYPE_2D,
			.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
			.width = imageData->w / 2,
			.height = imageData->h / 2,
			.layer_count_or_depth = 1,
			.num_levels = 1,
			.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET
		}
	);

	Uint32 bufferData[8] = { 2, 4, 8, 16, 32, 64, 128 };

	OriginalBuffer = SDL_CreateGPUBuffer(
		context->Device,
		&(SDL_GPUBufferCreateInfo) {
			.usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ, /* arbitrary */
			.size = sizeof(bufferData)
		}
	);

	BufferCopy = SDL_CreateGPUBuffer(
		context->Device,
		&(SDL_GPUBufferCreateInfo) {
			.usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ, /* arbitrary */
			.size = sizeof(bufferData)
		}
	);

	SDL_GPUTransferBuffer* downloadTransferBuffer = SDL_CreateGPUTransferBuffer(
		context->Device,
		&(SDL_GPUTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
			.size = imageData->w * imageData->h * 4 + sizeof(bufferData)
		}
	);

	SDL_GPUTransferBuffer* uploadTransferBuffer = SDL_CreateGPUTransferBuffer(
		context->Device,
		&(SDL_GPUTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = imageData->w * imageData->h * 4 + sizeof(bufferData)
		}
	);

	Uint8* uploadTransferPtr = SDL_MapGPUTransferBuffer(
		context->Device,
		uploadTransferBuffer,
		SDL_FALSE
	);
	SDL_memcpy(uploadTransferPtr, imageData->pixels, imageData->w * imageData->h * 4);
	SDL_memcpy(uploadTransferPtr + (imageData->w * imageData->h * 4), bufferData, sizeof(bufferData));
	SDL_UnmapGPUTransferBuffer(context->Device, uploadTransferBuffer);

	SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(context->Device);
	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdbuf);

	// Upload original texture
	SDL_UploadToGPUTexture(
		copyPass,
		&(SDL_GPUTextureTransferInfo) {
			.transfer_buffer = uploadTransferBuffer,
			.offset = 0, /* Zeroes out the rest */
		},
		&(SDL_GPUTextureRegion){
			.texture = OriginalTexture,
			.w = imageData->w,
			.h = imageData->h,
			.d = 1
		},
		SDL_FALSE
	);

	// Copy original to copy
	SDL_CopyGPUTextureToTexture(
		copyPass,
		&(SDL_GPUTextureLocation){
			.texture = OriginalTexture,
			.x = 0,
			.y = 0,
			.z = 0
		},
		&(SDL_GPUTextureLocation){
			.texture = TextureCopy,
			.x = 0,
			.y = 0,
			.z = 0
		},
		imageData->w,
		imageData->h,
		1,
		SDL_FALSE
	);

	// Upload original buffer
	SDL_UploadToGPUBuffer(
		copyPass,
		&(SDL_GPUTransferBufferLocation) {
			.transfer_buffer = uploadTransferBuffer,
			.offset = imageData->w * imageData->h * 4,
		},
		&(SDL_GPUBufferRegion) {
			.buffer = OriginalBuffer,
			.offset = 0,
			.size = sizeof(bufferData)
		},
		SDL_FALSE
	);

	// Copy original to copy
	SDL_CopyGPUBufferToBuffer(
		copyPass,
		&(SDL_GPUBufferLocation) {
			.buffer = OriginalBuffer,
			.offset = 0
		},
		&(SDL_GPUBufferLocation) {
			.buffer = BufferCopy,
			.offset = 0
		},
		sizeof(bufferData),
		SDL_FALSE
	);

	SDL_EndGPUCopyPass(copyPass);

	// Render the half-size version
	SDL_BlitGPUTexture(
		cmdbuf,
		&(SDL_GPUBlitRegion){
			.texture = OriginalTexture,
			.w = imageData->w,
			.h = imageData->h,
		},
		&(SDL_GPUBlitRegion){
			.texture = TextureSmall,
			.w = imageData->w / 2,
			.h = imageData->h / 2,
		},
		SDL_FLIP_NONE,
		SDL_GPU_FILTER_LINEAR,
		SDL_FALSE
	);

	// Download the original bytes from the copy
	copyPass = SDL_BeginGPUCopyPass(cmdbuf);

	SDL_DownloadFromGPUTexture(
		copyPass,
		&(SDL_GPUTextureRegion){
			.texture = TextureCopy,
			.w = imageData->w,
			.h = imageData->h,
			.d = 1
		},
		&(SDL_GPUTextureTransferInfo) {
			.transfer_buffer = downloadTransferBuffer,
			.offset = 0, /* Zeroes out the rest */
		}
	);

	SDL_DownloadFromGPUBuffer(
		copyPass,
		&(SDL_GPUBufferRegion) {
			.buffer = BufferCopy,
			.offset = 0,
			.size = sizeof(bufferData)
		},
		&(SDL_GPUTransferBufferLocation) {
			.transfer_buffer = downloadTransferBuffer,
			.offset = imageData->w * imageData->h * 4
		}
	);

	SDL_EndGPUCopyPass(copyPass);

	SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmdbuf);
	SDL_WaitForGPUFences(context->Device, SDL_TRUE, &fence, 1);
	SDL_ReleaseGPUFence(context->Device, fence);

	// Compare the original bytes to the copied bytes
	Uint8 *downloadedData = SDL_MapGPUTransferBuffer(
		context->Device,
		downloadTransferBuffer,
		SDL_FALSE
	);

	if (SDL_memcmp(downloadedData, imageData->pixels, imageData->w * imageData->h * 4) == 0)
	{
		SDL_Log("SUCCESS! Original texture bytes and the downloaded bytes match!");
	}
	else
	{
		SDL_Log("FAILURE! Original texture bytes do not match downloaded bytes!");
	}

	if (SDL_memcmp(downloadedData + (imageData->w * imageData->h * 4), bufferData, sizeof(bufferData)) == 0)
	{
		SDL_Log("SUCCESS! Original buffer bytes and the downloaded bytes match!");
	}
	else
	{
		SDL_Log("FAILURE! Original buffer bytes do not match downloaded bytes!");
	}

	SDL_UnmapGPUTransferBuffer(context->Device, downloadTransferBuffer);

	// Cleanup
	SDL_ReleaseGPUTransferBuffer(context->Device, downloadTransferBuffer);
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

		SDL_BlitGPUTexture(
			cmdbuf,
			&(SDL_GPUBlitRegion){
				.texture = OriginalTexture,
				.w = TextureWidth,
				.h = TextureHeight,
			},
			&(SDL_GPUBlitRegion){
				.texture = swapchainTexture,
				.w = w / 2,
				.h = h / 2,
			},
			SDL_FLIP_NONE,
			SDL_GPU_FILTER_NEAREST,
			SDL_FALSE
		);

		SDL_BlitGPUTexture(
			cmdbuf,
			&(SDL_GPUBlitRegion){
				.texture = TextureCopy,
				.w = TextureWidth,
				.h = TextureHeight,
			},
			&(SDL_GPUBlitRegion){
				.texture = swapchainTexture,
				.x = w / 2,
				.y = 0,
				.w = w / 2,
				.h = h / 2,
			},
			SDL_FLIP_NONE,
			SDL_GPU_FILTER_NEAREST,
			SDL_FALSE
		);

		SDL_BlitGPUTexture(
			cmdbuf,
			&(SDL_GPUBlitRegion){
				.texture = TextureSmall,
				.w = TextureWidth / 2,
				.h = TextureHeight / 2,
			},
			&(SDL_GPUBlitRegion){
				.texture = swapchainTexture,
				.x = w / 4,
				.y = h / 2,
				.w = w / 2,
				.h = h / 2,
			},
			SDL_FLIP_NONE,
			SDL_GPU_FILTER_NEAREST,
			SDL_FALSE
		);
	}

	SDL_SubmitGPUCommandBuffer(cmdbuf);

	return 0;
}

static void Quit(Context* context)
{
	SDL_ReleaseGPUTexture(context->Device, OriginalTexture);
	SDL_ReleaseGPUTexture(context->Device, TextureCopy);
	SDL_ReleaseGPUTexture(context->Device, TextureSmall);

	SDL_ReleaseGPUBuffer(context->Device, OriginalBuffer);
	SDL_ReleaseGPUBuffer(context->Device, BufferCopy);

	CommonQuit(context);
}

Example CopyAndReadback_Example = { "CopyAndReadback", Init, Update, Draw, Quit };
