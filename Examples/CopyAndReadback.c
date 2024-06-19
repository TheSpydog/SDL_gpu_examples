#include <SDL_gpu_examples.h>

static SDL_GpuTexture* OriginalTexture;
static SDL_GpuTexture* TextureCopy;
static SDL_GpuTexture* TextureSmall;

static SDL_GpuBuffer* OriginalBuffer;
static SDL_GpuBuffer* BufferCopy;

Uint32 TextureWidth, TextureHeight;

static int Init(Context* context)
{
	int result = CommonInit(context, 0);
	if (result < 0)
	{
		return result;
	}

	// Load the image
	int img_x, img_y, n;
	Uint8 *imageData = LoadImage("ravioli.png", &img_x, &img_y, &n, 4, SDL_FALSE);
	if (imageData == NULL)
	{
		SDL_Log("Could not load image data!");
		return -1;
	}

	TextureWidth = img_x;
	TextureHeight = img_y;

	// Create texture resources
	OriginalTexture = SDL_GpuCreateTexture(
		context->Device,
		&(SDL_GpuTextureCreateInfo){
			.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8,
			.width = img_x,
			.height = img_y,
			.depth = 1,
			.layerCount = 1,
			.levelCount = 1,
			.usageFlags = SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT
		}
	);

	TextureCopy = SDL_GpuCreateTexture(
		context->Device,
		&(SDL_GpuTextureCreateInfo){
			.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8,
			.width = img_x,
			.height = img_y,
			.depth = 1,
			.layerCount = 1,
			.levelCount = 1,
			.usageFlags = SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT
		}
	);

	TextureSmall = SDL_GpuCreateTexture(
		context->Device,
		&(SDL_GpuTextureCreateInfo){
			.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8,
			.width = img_x / 2,
			.height = img_y / 2,
			.depth = 1,
			.layerCount = 1,
			.levelCount = 1,
			.usageFlags = SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT
		}
	);

	Uint32 bufferData[8] = { 2, 4, 8, 16, 32, 64, 128 };

	OriginalBuffer = SDL_GpuCreateBuffer(
		context->Device,
		SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ_BIT, /* arbitrary */
		sizeof(bufferData)
	);

	BufferCopy = SDL_GpuCreateBuffer(
		context->Device,
		SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ_BIT, /* arbitrary */
		sizeof(bufferData)
	);

	SDL_GpuTransferBuffer* downloadTransferBuffer = SDL_GpuCreateTransferBuffer(
		context->Device,
		SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
		img_x * img_y * 4 + sizeof(bufferData)
	);

	SDL_GpuTransferBuffer* uploadTransferBuffer = SDL_GpuCreateTransferBuffer(
		context->Device,
		SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
		img_x * img_y * 4 + sizeof(bufferData)
	);

	SDL_GpuSetTransferData(
		context->Device,
		imageData,
		uploadTransferBuffer,
		&(SDL_GpuBufferCopy){
			.size = img_x * img_y * 4
		},
		SDL_FALSE
	);

	SDL_GpuSetTransferData(
		context->Device,
		bufferData,
		uploadTransferBuffer,
		&(SDL_GpuBufferCopy){
			.srcOffset = 0,
			.dstOffset = img_x * img_y * 4,
			.size = sizeof(bufferData)
		},
		SDL_FALSE
	);

	SDL_GpuCommandBuffer* cmdbuf = SDL_GpuAcquireCommandBuffer(context->Device);
	SDL_GpuCopyPass* copyPass = SDL_GpuBeginCopyPass(cmdbuf);

	// Upload original texture
	SDL_GpuUploadToTexture(
		copyPass,
		uploadTransferBuffer,
		&(SDL_GpuTextureRegion){
			.textureSlice.texture = OriginalTexture,
			.w = img_x,
			.h = img_y,
			.d = 1
		},
		&(SDL_GpuBufferImageCopy){
			.bufferOffset = 0
		},
		SDL_FALSE
	);

	// Copy original to copy
	SDL_GpuCopyTextureToTexture(
		copyPass,
		&(SDL_GpuTextureRegion){
			.textureSlice.texture = OriginalTexture,
			.w = img_x,
			.h = img_y,
			.d = 1
		},
		&(SDL_GpuTextureRegion){
			.textureSlice.texture = TextureCopy,
			.w = img_x,
			.h = img_y,
			.d = 1
		},
		SDL_FALSE
	);

	// Upload original buffer
	SDL_GpuUploadToBuffer(
		copyPass,
		uploadTransferBuffer,
		OriginalBuffer,
		&(SDL_GpuBufferCopy){
			.srcOffset = img_x * img_y * 4,
			.dstOffset = 0,
			.size = sizeof(bufferData)
		},
		SDL_FALSE
	);

	// Copy original to copy
	SDL_GpuCopyBufferToBuffer(
		copyPass,
		OriginalBuffer,
		BufferCopy,
		&(SDL_GpuBufferCopy){
			.size = sizeof(bufferData)
		},
		SDL_FALSE
	);

	SDL_GpuEndCopyPass(copyPass);

	// Render the half-size version
	SDL_GpuBlit(
		cmdbuf,
		&(SDL_GpuTextureRegion){
			.textureSlice.texture = OriginalTexture,
			.w = img_x,
			.h = img_y,
			.d = 1
		},
		&(SDL_GpuTextureRegion){
			.textureSlice.texture = TextureSmall,
			.w = img_x / 2,
			.h = img_y / 2,
			.d = 1
		},
		SDL_GPU_FILTER_LINEAR,
		SDL_FALSE
	);

	// Download the original bytes from the copy
	copyPass = SDL_GpuBeginCopyPass(cmdbuf);

	SDL_GpuDownloadFromTexture(
		copyPass,
		&(SDL_GpuTextureRegion){
			.textureSlice.texture = TextureCopy,
			.w = img_x,
			.h = img_y,
			.d = 1
		},
		downloadTransferBuffer,
		&(SDL_GpuBufferImageCopy){
			.bufferOffset = 0
		}
	);

	SDL_GpuDownloadFromBuffer(
		copyPass,
		BufferCopy,
		downloadTransferBuffer,
		&(SDL_GpuBufferCopy){
			.dstOffset = img_x * img_y * 4,
			.size = sizeof(bufferData)
		}
	);

	SDL_GpuEndCopyPass(copyPass);

	SDL_GpuFence* fence = SDL_GpuSubmitAndAcquireFence(cmdbuf);
	SDL_GpuWaitForFences(context->Device, SDL_TRUE, &fence, 1);
	SDL_GpuReleaseFence(context->Device, fence);

	// Compare the original bytes to the copied bytes
	Uint8 *downloadedData;

	SDL_GpuMapTransferBuffer(
		context->Device,
		downloadTransferBuffer,
		SDL_FALSE,
		&downloadedData
	);

	if (SDL_memcmp(downloadedData, imageData, img_x * img_y * 4) == 0)
	{
		SDL_Log("SUCCESS! Original texture bytes and the downloaded bytes match!");
	}
	else
	{
		SDL_Log("FAILURE! Original texture bytes do not match downloaded bytes!");
	}

	if (SDL_memcmp(downloadedData + (img_x * img_y * 4), bufferData, sizeof(bufferData)) == 0)
	{
		SDL_Log("SUCCESS! Original buffer bytes and the downloaded bytes match!");
	}
	else
	{
		SDL_Log("FAILURE! Original buffer bytes do not match downloaded bytes!");
	}

	SDL_GpuUnmapTransferBuffer(context->Device, downloadTransferBuffer);

	// Cleanup
	SDL_GpuReleaseTransferBuffer(context->Device, downloadTransferBuffer);
	SDL_GpuReleaseTransferBuffer(context->Device, uploadTransferBuffer);

	SDL_free(imageData);

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
				.textureSlice.texture = swapchainTexture,
				.loadOp = SDL_GPU_LOADOP_CLEAR,
				.storeOp = SDL_GPU_STOREOP_STORE,
				.clearColor = { 0, 0, 0, 1 },
				.cycle = SDL_FALSE
			}},
			1,
			NULL
		);
		SDL_GpuEndRenderPass(clearPass);

		SDL_GpuBlit(
			cmdbuf,
			&(SDL_GpuTextureRegion){
				.textureSlice.texture = OriginalTexture,
				.w = TextureWidth,
				.h = TextureHeight,
				.d = 1
			},
			&(SDL_GpuTextureRegion){
				.textureSlice.texture = swapchainTexture,
				.w = w / 2,
				.h = h / 2,
				.d = 1
			},
			SDL_GPU_FILTER_NEAREST,
			SDL_FALSE
		);

		SDL_GpuBlit(
			cmdbuf,
			&(SDL_GpuTextureRegion){
				.textureSlice.texture = TextureCopy,
				.w = TextureWidth,
				.h = TextureHeight,
				.d = 1
			},
			&(SDL_GpuTextureRegion){
				.textureSlice.texture = swapchainTexture,
				.x = w / 2,
				.y = 0,
				.w = w / 2,
				.h = h / 2,
				.d = 1
			},
			SDL_GPU_FILTER_NEAREST,
			SDL_FALSE
		);

		SDL_GpuBlit(
			cmdbuf,
			&(SDL_GpuTextureRegion){
				.textureSlice.texture = TextureSmall,
				.w = TextureWidth / 2,
				.h = TextureHeight / 2,
				.d = 1
			},
			&(SDL_GpuTextureRegion){
				.textureSlice.texture = swapchainTexture,
				.x = w / 4,
				.y = h / 2,
				.w = w / 2,
				.h = h / 2,
				.d = 1
			},
			SDL_GPU_FILTER_NEAREST,
			SDL_FALSE
		);
	}

	SDL_GpuSubmit(cmdbuf);

	return 0;
}

static void Quit(Context* context)
{
	SDL_GpuReleaseTexture(context->Device, OriginalTexture);
	SDL_GpuReleaseTexture(context->Device, TextureCopy);
	SDL_GpuReleaseTexture(context->Device, TextureSmall);

	SDL_GpuReleaseBuffer(context->Device, OriginalBuffer);
	SDL_GpuReleaseBuffer(context->Device, BufferCopy);

	CommonQuit(context);
}

Example CopyAndReadback_Example = { "CopyAndReadback", Init, Update, Draw, Quit };
