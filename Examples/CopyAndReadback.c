#include "Common.h"

static SDL_GpuTexture* OriginalTexture;
static SDL_GpuTexture* TextureCopy;
static SDL_GpuTexture* TextureSmall;

static SDL_GpuBuffer* OriginalBuffer;
static SDL_GpuBuffer* BufferCopy;

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
	OriginalTexture = SDL_CreateGpuTexture(
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

	TextureCopy = SDL_CreateGpuTexture(
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

	TextureSmall = SDL_CreateGpuTexture(
		context->Device,
		&(SDL_GpuTextureCreateInfo){
			.type = SDL_GPU_TEXTURETYPE_2D,
			.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
			.width = imageData->w / 2,
			.height = imageData->h / 2,
			.layerCountOrDepth = 1,
			.levelCount = 1,
			.usageFlags = SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT
		}
	);

	Uint32 bufferData[8] = { 2, 4, 8, 16, 32, 64, 128 };

	OriginalBuffer = SDL_CreateGpuBuffer(
		context->Device,
		&(SDL_GpuBufferCreateInfo) {
			.usageFlags = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ_BIT, /* arbitrary */
			.sizeInBytes = sizeof(bufferData)
		}
	);

	BufferCopy = SDL_CreateGpuBuffer(
		context->Device,
		&(SDL_GpuBufferCreateInfo) {
			.usageFlags = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ_BIT, /* arbitrary */
			.sizeInBytes = sizeof(bufferData)
		}
	);

	SDL_GpuTransferBuffer* downloadTransferBuffer = SDL_CreateGpuTransferBuffer(
		context->Device,
		&(SDL_GpuTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
			.sizeInBytes = imageData->w * imageData->h * 4 + sizeof(bufferData)
		}
	);

	SDL_GpuTransferBuffer* uploadTransferBuffer = SDL_CreateGpuTransferBuffer(
		context->Device,
		&(SDL_GpuTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.sizeInBytes = imageData->w * imageData->h * 4 + sizeof(bufferData)
		}
	);

	Uint8* uploadTransferPtr = SDL_MapGpuTransferBuffer(
		context->Device,
		uploadTransferBuffer,
		SDL_FALSE
	);
	SDL_memcpy(uploadTransferPtr, imageData->pixels, imageData->w * imageData->h * 4);
	SDL_memcpy(uploadTransferPtr + (imageData->w * imageData->h * 4), bufferData, sizeof(bufferData));
	SDL_UnmapGpuTransferBuffer(context->Device, uploadTransferBuffer);

	SDL_GpuCommandBuffer* cmdbuf = SDL_AcquireGpuCommandBuffer(context->Device);
	SDL_GpuCopyPass* copyPass = SDL_BeginGpuCopyPass(cmdbuf);

	// Upload original texture
	SDL_UploadToGpuTexture(
		copyPass,
		&(SDL_GpuTextureTransferInfo) {
			.transferBuffer = uploadTransferBuffer,
			.offset = 0, /* Zeroes out the rest */
		},
		&(SDL_GpuTextureRegion){
			.texture = OriginalTexture,
			.w = imageData->w,
			.h = imageData->h,
			.d = 1
		},
		SDL_FALSE
	);

	// Copy original to copy
	SDL_CopyGpuTextureToTexture(
		copyPass,
		&(SDL_GpuTextureLocation){
			.texture = OriginalTexture,
			.x = 0,
			.y = 0,
			.z = 0
		},
		&(SDL_GpuTextureLocation){
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
	SDL_UploadToGpuBuffer(
		copyPass,
		&(SDL_GpuTransferBufferLocation) {
			.transferBuffer = uploadTransferBuffer,
			.offset = imageData->w * imageData->h * 4,
		},
		&(SDL_GpuBufferRegion) {
			.buffer = OriginalBuffer,
			.offset = 0,
			.size = sizeof(bufferData)
		},
		SDL_FALSE
	);

	// Copy original to copy
	SDL_CopyGpuBufferToBuffer(
		copyPass,
		&(SDL_GpuBufferLocation) {
			.buffer = OriginalBuffer,
			.offset = 0
		},
		&(SDL_GpuBufferLocation) {
			.buffer = BufferCopy,
			.offset = 0
		},
		sizeof(bufferData),
		SDL_FALSE
	);

	SDL_EndGpuCopyPass(copyPass);

	// Render the half-size version
	SDL_BlitGpu(
		cmdbuf,
		&(SDL_GpuBlitRegion){
			.texture = OriginalTexture,
			.w = imageData->w,
			.h = imageData->h,
		},
		&(SDL_GpuBlitRegion){
			.texture = TextureSmall,
			.w = imageData->w / 2,
			.h = imageData->h / 2,
		},
		SDL_FLIP_NONE,
		SDL_GPU_FILTER_LINEAR,
		SDL_FALSE
	);

	// Download the original bytes from the copy
	copyPass = SDL_BeginGpuCopyPass(cmdbuf);

	SDL_DownloadFromGpuTexture(
		copyPass,
		&(SDL_GpuTextureRegion){
			.texture = TextureCopy,
			.w = imageData->w,
			.h = imageData->h,
			.d = 1
		},
		&(SDL_GpuTextureTransferInfo) {
			.transferBuffer = downloadTransferBuffer,
			.offset = 0, /* Zeroes out the rest */
		}
	);

	SDL_DownloadFromGpuBuffer(
		copyPass,
		&(SDL_GpuBufferRegion) {
			.buffer = BufferCopy,
			.offset = 0,
			.size = sizeof(bufferData)
		},
		&(SDL_GpuTransferBufferLocation) {
			.transferBuffer = downloadTransferBuffer,
			.offset = imageData->w * imageData->h * 4
		}
	);

	SDL_EndGpuCopyPass(copyPass);

	SDL_GpuFence* fence = SDL_SubmitGpuAndAcquireFence(cmdbuf);
	SDL_WaitGpuForFences(context->Device, SDL_TRUE, &fence, 1);
	SDL_ReleaseGpuFence(context->Device, fence);

	// Compare the original bytes to the copied bytes
	Uint8 *downloadedData = SDL_MapGpuTransferBuffer(
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

	SDL_UnmapGpuTransferBuffer(context->Device, downloadTransferBuffer);

	// Cleanup
	SDL_ReleaseGpuTransferBuffer(context->Device, downloadTransferBuffer);
	SDL_ReleaseGpuTransferBuffer(context->Device, uploadTransferBuffer);

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
	SDL_GpuCommandBuffer* cmdbuf = SDL_AcquireGpuCommandBuffer(context->Device);
	SDL_GpuTexture* swapchainTexture = SDL_AcquireGpuSwapchainTexture(cmdbuf, context->Window, &w, &h);
	if (swapchainTexture != NULL)
	{
		SDL_GpuRenderPass* clearPass = SDL_BeginGpuRenderPass(
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
		SDL_EndGpuRenderPass(clearPass);

		SDL_BlitGpu(
			cmdbuf,
			&(SDL_GpuBlitRegion){
				.texture = OriginalTexture,
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

		SDL_BlitGpu(
			cmdbuf,
			&(SDL_GpuBlitRegion){
				.texture = TextureCopy,
				.w = TextureWidth,
				.h = TextureHeight,
			},
			&(SDL_GpuBlitRegion){
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

		SDL_BlitGpu(
			cmdbuf,
			&(SDL_GpuBlitRegion){
				.texture = TextureSmall,
				.w = TextureWidth / 2,
				.h = TextureHeight / 2,
			},
			&(SDL_GpuBlitRegion){
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

	SDL_SubmitGpu(cmdbuf);

	return 0;
}

static void Quit(Context* context)
{
	SDL_ReleaseGpuTexture(context->Device, OriginalTexture);
	SDL_ReleaseGpuTexture(context->Device, TextureCopy);
	SDL_ReleaseGpuTexture(context->Device, TextureSmall);

	SDL_ReleaseGpuBuffer(context->Device, OriginalBuffer);
	SDL_ReleaseGpuBuffer(context->Device, BufferCopy);

	CommonQuit(context);
}

Example CopyAndReadback_Example = { "CopyAndReadback", Init, Update, Draw, Quit };
