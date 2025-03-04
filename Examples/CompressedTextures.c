#include "Common.h"

#define BC_IMAGE_COUNT 12
#define ASTC_IMAGE_COUNT 14

static SDL_GPUTexture* SrcTextures[BC_IMAGE_COUNT + ASTC_IMAGE_COUNT];
static SDL_GPUTexture* DstTextures[BC_IMAGE_COUNT + ASTC_IMAGE_COUNT];

static SDL_GPUTextureFormat TextureFormats[BC_IMAGE_COUNT + ASTC_IMAGE_COUNT] =
{
	// BCn formats
	SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM,
	SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM,
	SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM,
	SDL_GPU_TEXTUREFORMAT_BC4_R_UNORM,
	SDL_GPU_TEXTUREFORMAT_BC5_RG_UNORM,
	SDL_GPU_TEXTUREFORMAT_BC6H_RGB_FLOAT,
	SDL_GPU_TEXTUREFORMAT_BC6H_RGB_UFLOAT,
	SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM,
	SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM_SRGB,
	SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM_SRGB,
	SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM_SRGB,
	SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM_SRGB,

	// ASTC formats
	SDL_GPU_TEXTUREFORMAT_ASTC_4x4_UNORM,
	SDL_GPU_TEXTUREFORMAT_ASTC_5x4_UNORM,
	SDL_GPU_TEXTUREFORMAT_ASTC_5x5_UNORM,
	SDL_GPU_TEXTUREFORMAT_ASTC_6x5_UNORM,
	SDL_GPU_TEXTUREFORMAT_ASTC_6x6_UNORM,
	SDL_GPU_TEXTUREFORMAT_ASTC_8x5_UNORM,
	SDL_GPU_TEXTUREFORMAT_ASTC_8x6_UNORM,
	SDL_GPU_TEXTUREFORMAT_ASTC_8x8_UNORM,
	SDL_GPU_TEXTUREFORMAT_ASTC_10x5_UNORM,
	SDL_GPU_TEXTUREFORMAT_ASTC_10x6_UNORM,
	SDL_GPU_TEXTUREFORMAT_ASTC_10x8_UNORM,
	SDL_GPU_TEXTUREFORMAT_ASTC_10x10_UNORM,
	SDL_GPU_TEXTUREFORMAT_ASTC_12x10_UNORM,
	SDL_GPU_TEXTUREFORMAT_ASTC_12x12_UNORM,
	// FIXME: SRGB/FLOAT formats
};

static const char* TextureNames[BC_IMAGE_COUNT + ASTC_IMAGE_COUNT] =
{
	"bcn/BC1.dds",
	"bcn/BC2.dds",
	"bcn/BC3.dds",
	"bcn/BC4.dds",
	"bcn/BC5.dds",
	"bcn/BC6H_S.dds",
	"bcn/BC6H_U.dds",
	"bcn/BC7.dds",
	"bcn/BC1_SRGB.dds",
	"bcn/BC2_SRGB.dds",
	"bcn/BC3_SRGB.dds",
	"bcn/BC7_SRGB.dds",

	"astc/4x4.astc",
	"astc/5x4.astc",
	"astc/5x5.astc",
	"astc/6x5.astc",
	"astc/6x6.astc",
	"astc/8x5.astc",
	"astc/8x6.astc",
	"astc/8x8.astc",
	"astc/10x5.astc",
	"astc/10x6.astc",
	"astc/10x8.astc",
	"astc/10x10.astc",
	"astc/12x10.astc",
	"astc/12x12.astc",
};

static int CurrentTextureIndex;

static int Init(Context* context)
{
	int result = CommonInit(context, 0);
	if (result < 0)
	{
		return result;
	}

	SDL_GPUTransferBuffer* downloadTransferBuffer = NULL;
	Uint8* firstTextureData = NULL;
	size_t firstTextureDataLength = 0;

	SDL_GPUCommandBuffer* uploadCmdBuf = SDL_AcquireGPUCommandBuffer(context->Device);
	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);

	// Upload texture data
	for (int i = 0; i < SDL_arraysize(SrcTextures); i += 1)
	{
		if (!SDL_GPUTextureSupportsFormat(context->Device, TextureFormats[i], SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_SAMPLER))
		{
			SrcTextures[i] = NULL;
			DstTextures[i] = NULL;
			continue;
		}

		// Load the image
		int imageWidth, imageHeight, imageDataLength;
		void* imageData;
		if (i < BC_IMAGE_COUNT)
		{
			imageData = LoadDDSImage(TextureNames[i], TextureFormats[i], &imageWidth, &imageHeight, &imageDataLength);
		}
		else
		{
			imageData = LoadASTCImage(TextureNames[i], &imageWidth, &imageHeight, &imageDataLength);
		}
		if (imageData == NULL)
		{
			SDL_Log("Failed to load image data! %s", TextureNames[i]);
			return 1;
		}

		// Create the texture
		SDL_GPUTextureCreateInfo createInfo =
		{
			.format = TextureFormats[i],
			.width = imageWidth,
			.height = imageHeight,
			.layer_count_or_depth = 1,
			.type = SDL_GPU_TEXTURETYPE_2D,
			.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
			.num_levels = 1,
		};
		SrcTextures[i] = SDL_CreateGPUTexture(context->Device, &createInfo);
		if (!SrcTextures[i])
		{
			SDL_Log("Failed to create texture: %s", SDL_GetError());
			return -1;
		}
		DstTextures[i] = SDL_CreateGPUTexture(context->Device, &createInfo);
		if (!DstTextures[i])
		{
			SDL_Log("Failed to create texture: %s", SDL_GetError());
			return -1;
		}

		// Set up texture transfer data
		SDL_GPUTransferBuffer* textureTransferBuffer = SDL_CreateGPUTransferBuffer(
			context->Device,
			&(SDL_GPUTransferBufferCreateInfo) {
				.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
				.size = imageDataLength
			}
		);

		Uint8* textureTransferPtr = SDL_MapGPUTransferBuffer(
			context->Device,
			textureTransferBuffer,
			false
		);
		SDL_memcpy(textureTransferPtr, imageData, imageDataLength);
		SDL_UnmapGPUTransferBuffer(context->Device, textureTransferBuffer);

		// Upload the texture data
		SDL_UploadToGPUTexture(
			copyPass,
			&(SDL_GPUTextureTransferInfo) {
				.transfer_buffer = textureTransferBuffer,
				.offset = 0, /* Zeros out the rest */
			},
			&(SDL_GPUTextureRegion){
				.texture = SrcTextures[i],
				.w = imageWidth,
				.h = imageHeight,
				.d = 1
			},
			false
		);

		SDL_CopyGPUTextureToTexture(
			copyPass,
			&(SDL_GPUTextureLocation){
				.texture = SrcTextures[i]
			},
			&(SDL_GPUTextureLocation){
				.texture = DstTextures[i]
			},
			256,
			256,
			1,
			false
		);

		SDL_ReleaseGPUTransferBuffer(context->Device, textureTransferBuffer);

		// Testing if downloads work...
		if (i == 0)
		{
			downloadTransferBuffer = SDL_CreateGPUTransferBuffer(
				context->Device,
				&(SDL_GPUTransferBufferCreateInfo){
					.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
					.size = imageDataLength
				}
			);

			SDL_DownloadFromGPUTexture(
				copyPass,
				&(SDL_GPUTextureRegion){
					.texture = SrcTextures[i],
					.w = 256,
					.h = 256,
					.d = 1,
				},
				&(SDL_GPUTextureTransferInfo){
					.transfer_buffer = downloadTransferBuffer
				}
			);

			firstTextureData = SDL_malloc(imageDataLength);
			firstTextureDataLength = imageDataLength;
			SDL_memcpy(firstTextureData, imageData, imageDataLength);
		}

		SDL_free(imageData);
	}

	SDL_EndGPUCopyPass(copyPass);
	SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(uploadCmdBuf);

	// Read the downloaded data
	SDL_WaitForGPUFences(context->Device, true, &fence, 1);
	SDL_ReleaseGPUFence(context->Device, fence);
	Uint8* downloadPtr = SDL_MapGPUTransferBuffer(context->Device, downloadTransferBuffer, false);
	if (SDL_memcmp(downloadPtr, firstTextureData, firstTextureDataLength) == 0)
	{
		SDL_Log("Success: Downloaded bytes match original texture bytes!");
	}
	else
	{
		SDL_Log("Failure: Downloaded bytes match original texture bytes!");
	}
	SDL_UnmapGPUTransferBuffer(context->Device, downloadTransferBuffer);
	SDL_ReleaseGPUTransferBuffer(context->Device, downloadTransferBuffer);
	SDL_free(firstTextureData);

	// Finally, print instructions!
	SDL_Log("Press Left/Right to switch between textures");

	return 0;
}

static int Update(Context* context)
{
	bool changed = false;

	if (context->LeftPressed)
	{
		CurrentTextureIndex -= 1;
		if (CurrentTextureIndex < 0)
		{
			CurrentTextureIndex = SDL_arraysize(SrcTextures) - 1;
		}
		changed = true;
	}

	if (context->RightPressed)
	{
		CurrentTextureIndex = (CurrentTextureIndex + 1) % SDL_arraysize(SrcTextures);
		changed = true;
	}

	if (changed)
	{
		if (SrcTextures[CurrentTextureIndex] == NULL)
		{
			SDL_Log("Unsupported texture format: %s", TextureNames[CurrentTextureIndex]);
		}
		else
		{
			SDL_Log("Setting texture to: %s", TextureNames[CurrentTextureIndex]);
		}
	}

	return 0;
}

static int Draw(Context* context)
{
    SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(context->Device);
    if (cmdbuf == NULL)
    {
        SDL_Log("AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return -1;
    }

    SDL_GPUTexture* swapchainTexture;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdbuf, context->Window, &swapchainTexture, NULL, NULL)) {
        SDL_Log("WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
        return -1;
    }

	if (swapchainTexture != NULL)
	{
		if (SrcTextures[CurrentTextureIndex] != NULL)
		{
			SDL_BlitGPUTexture(
				cmdbuf,
				&(SDL_GPUBlitInfo){
					.clear_color = (SDL_FColor){ 1.0f, 1.0f, 1.0f, 1.0f },
					.load_op = SDL_GPU_LOADOP_CLEAR,
					.source.texture = SrcTextures[CurrentTextureIndex],
					.source.w = 256,
					.source.h = 256,
					.destination.texture = swapchainTexture,
					.destination.w = 256,
					.destination.h = 256,
				}
			);
			SDL_BlitGPUTexture(
				cmdbuf,
				&(SDL_GPUBlitInfo){
					.source.texture = DstTextures[CurrentTextureIndex],
					.source.w = 256,
					.source.h = 256,
					.destination.texture = swapchainTexture,
					.destination.x = 512,
					.destination.w = 256,
					.destination.h = 256,
				}
			);
		}
		else
		{
			SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(
				cmdbuf,
				&(SDL_GPUColorTargetInfo){
					.texture = swapchainTexture,
					.load_op = SDL_GPU_LOADOP_CLEAR,
					.clear_color = (SDL_FColor){ 1.0f, 1.0f, 1.0f, 1.0f }
				},
				1,
				NULL
			);
			SDL_EndGPURenderPass(renderPass);
		}
	}

	SDL_SubmitGPUCommandBuffer(cmdbuf);

	return 0;
}

static void Quit(Context* context)
{
	for (int i = 0; i < SDL_arraysize(SrcTextures); i += 1)
	{
		SDL_ReleaseGPUTexture(context->Device, SrcTextures[i]);
		SDL_ReleaseGPUTexture(context->Device, DstTextures[i]);
		SrcTextures[i] = NULL;
		DstTextures[i] = NULL;
	}

	CurrentTextureIndex = 0;

	CommonQuit(context);
}

Example CompressedTextures_Example = { "CompressedTextures", Init, Update, Draw, Quit };
