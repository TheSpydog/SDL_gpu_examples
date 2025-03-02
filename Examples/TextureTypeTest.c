#include "Common.h"

// NOTE: This program is intended purely as a test suite for SDL GPU backends.
// It's not reflective of any best practices.

// If all combinations of src and dst texture formats look identical, the test passes.

static SDL_GPUTexture* SrcTextures[5];
static SDL_GPUTexture* DstTextures[5];
static Uint32 SrcTextureIndex;
static Uint32 DstTextureIndex;

static const char* TextureTypeNames[] =
{
	"2D",
	"2DArray",
	"3D",
	"Cubemap",
	"CubemapArray"
};

static Uint32 BaseMipSlices[] = { 0, 1, 1, 1, 7 };
static Uint32 SecondMipSlices[] = { 0, 1, 0, 1, 7 };

static int Init(Context* context)
{
	int result = CommonInit(context, 0);
	if (result < 0)
	{
		return result;
	}

	// Load the images

	SDL_Surface* baseMip = LoadImage("cube0.bmp", 4);
	SDL_Surface* secondMip = LoadImage("cube0mip1.bmp", 4);

	if (!baseMip || !secondMip)
	{
		SDL_Log("Failed to load images!");
		return -1;
	}

	int baseMipDataSize = baseMip->w * baseMip->h * 4;
	int secondMipDataSize = secondMip->w * secondMip->h * 4;

	// Create the textures

	SDL_GPUTextureCreateInfo createInfo =
	{
		.type = SDL_GPU_TEXTURETYPE_2D,
		.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
		.width = 32,
		.height = 32,
		.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
		.num_levels = 2,
		.layer_count_or_depth = 1,
	};

	// 2D
	SrcTextures[0] = SDL_CreateGPUTexture(context->Device, &createInfo);
	DstTextures[0] = SDL_CreateGPUTexture(context->Device, &createInfo);
	if (!SrcTextures[0] || !DstTextures[0])
	{
		SDL_Log("Failed to create 2D texture");
		return -1;
	}

	// 2D Array
	createInfo.type = SDL_GPU_TEXTURETYPE_2D_ARRAY;
	createInfo.layer_count_or_depth = 2;
	SrcTextures[1] = SDL_CreateGPUTexture(context->Device, &createInfo);
	DstTextures[1] = SDL_CreateGPUTexture(context->Device, &createInfo);
	if (!SrcTextures[1] || !DstTextures[1])
	{
		SDL_Log("Failed to create 2D array texture");
		return -1;
	}

	// 3D
	createInfo.type = SDL_GPU_TEXTURETYPE_3D;
	createInfo.layer_count_or_depth = 2;
	SrcTextures[2] = SDL_CreateGPUTexture(context->Device, &createInfo);
	DstTextures[2] = SDL_CreateGPUTexture(context->Device, &createInfo);
	if (!SrcTextures[2] || !DstTextures[2])
	{
		SDL_Log("Failed to create 3D texture");
		return -1;
	}

	// Cubemap
	createInfo.type = SDL_GPU_TEXTURETYPE_CUBE;
	createInfo.layer_count_or_depth = 6;
	SrcTextures[3] = SDL_CreateGPUTexture(context->Device, &createInfo);
	DstTextures[3] = SDL_CreateGPUTexture(context->Device, &createInfo);
	if (!SrcTextures[3] || !DstTextures[3])
	{
		SDL_Log("Failed to create cubemap texture");
		return -1;
	}

	// Cubemap Array
	createInfo.type = SDL_GPU_TEXTURETYPE_CUBE_ARRAY;
	createInfo.layer_count_or_depth = 12;
	SrcTextures[4] = SDL_CreateGPUTexture(context->Device, &createInfo);
	DstTextures[4] = SDL_CreateGPUTexture(context->Device, &createInfo);
	if (!SrcTextures[4] || !DstTextures[4])
	{
		SDL_Log("Failed to create cubemaparray  texture");
		return -1;
	}

	// Create and populate the transfer buffer

	SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(
		context->Device,
		&(SDL_GPUTransferBufferCreateInfo) {
			.size = baseMipDataSize + secondMipDataSize
		}
	);

	Uint8* transferPtr = SDL_MapGPUTransferBuffer(context->Device, transferBuffer, false);
	SDL_memcpy(transferPtr, baseMip->pixels, baseMipDataSize);
	SDL_memcpy(transferPtr + baseMipDataSize, secondMip->pixels, secondMipDataSize);
	SDL_UnmapGPUTransferBuffer(context->Device, transferBuffer);

	// Upload the texture data

	SDL_GPUCommandBuffer* uploadCommandBuffer = SDL_AcquireGPUCommandBuffer(context->Device);
	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCommandBuffer);

	SDL_GPUTextureTransferInfo baseMipTransferInfo = { transferBuffer, 0 };
	SDL_GPUTextureTransferInfo secondMipTransferInfo = { transferBuffer, baseMipDataSize };

	// 2D
	SDL_UploadToGPUTexture(copyPass, &baseMipTransferInfo, &(SDL_GPUTextureRegion){ SrcTextures[0], 0, 0, 0, 0, 0, 32, 32, 1 }, false);
	SDL_UploadToGPUTexture(copyPass, &secondMipTransferInfo, &(SDL_GPUTextureRegion){ SrcTextures[0], 1, 0, 0, 0, 0, 16, 16, 1 }, false);

	// 2D Array
	SDL_UploadToGPUTexture(copyPass, &baseMipTransferInfo, &(SDL_GPUTextureRegion){ SrcTextures[1], 0, 1, 0, 0, 0, 32, 32, 1 }, false);
	SDL_UploadToGPUTexture(copyPass, &secondMipTransferInfo, &(SDL_GPUTextureRegion){ SrcTextures[1], 1, 1, 0, 0, 0, 16, 16, 1 }, false);

	// 3D
	SDL_UploadToGPUTexture(copyPass, &baseMipTransferInfo, &(SDL_GPUTextureRegion){ SrcTextures[2], 0, 0, 0, 0, 1, 32, 32, 1 }, false);
	SDL_UploadToGPUTexture(copyPass, &secondMipTransferInfo, &(SDL_GPUTextureRegion){ SrcTextures[2], 1, 0, 0, 0, 0, 16, 16, 1 }, false);

	// Cubemap
	SDL_UploadToGPUTexture(copyPass, &baseMipTransferInfo, &(SDL_GPUTextureRegion){ SrcTextures[3], 0, 1, 0, 0, 0, 32, 32, 1 }, false);
	SDL_UploadToGPUTexture(copyPass, &secondMipTransferInfo, &(SDL_GPUTextureRegion){ SrcTextures[3], 1, 1, 0, 0, 0, 16, 16, 1 }, false);

	// Cubemap Array
	SDL_UploadToGPUTexture(copyPass, &baseMipTransferInfo, &(SDL_GPUTextureRegion){ SrcTextures[4], 0, 7, 0, 0, 0, 32, 32, 1 }, false);
	SDL_UploadToGPUTexture(copyPass, &secondMipTransferInfo, &(SDL_GPUTextureRegion){ SrcTextures[4], 1, 7, 0, 0, 0, 16, 16, 1 }, false);

	SDL_EndGPUCopyPass(copyPass);
	SDL_SubmitGPUCommandBuffer(uploadCommandBuffer);

	// Clean up

	SDL_DestroySurface(baseMip);
	SDL_DestroySurface(secondMip);
	SDL_ReleaseGPUTransferBuffer(context->Device, transferBuffer);

	// Set up the program

	SrcTextureIndex = 0;
	DstTextureIndex = 0;

	SDL_Log("Press Left to cycle through source texture types.");
	SDL_Log("Press Right to cycle through destination texture types.");
	SDL_Log("(2D / 2D)");

	return 0;
}

static int Update(Context* context)
{
	bool changed = false;

	if (context->LeftPressed)
	{
		SrcTextureIndex = (SrcTextureIndex + 1) % 5;
		changed = true;
	}

	if (context->RightPressed)
	{
		DstTextureIndex = (DstTextureIndex + 1) % 5;
		changed = true;
	}

	if (changed)
	{
		SDL_Log("(%s / %s)", TextureTypeNames[SrcTextureIndex], TextureTypeNames[DstTextureIndex]);
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
	if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdbuf, context->Window, &swapchainTexture, NULL, NULL))
	{
		SDL_Log("WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
		return -1;
	}

	if (swapchainTexture != NULL)
	{
		// Clear the screen
		SDL_GPURenderPass *renderPass = SDL_BeginGPURenderPass(
			cmdbuf,
			&(SDL_GPUColorTargetInfo){
				.texture = swapchainTexture,
				.load_op = SDL_GPU_LOADOP_CLEAR,
				.clear_color = (SDL_FColor){ 0.0f, 0.0f, 0.0f, 1.0f },
			},
			1,
			NULL
		);
		SDL_EndGPURenderPass(renderPass);

		// Copy the source to the destination

		SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdbuf);

		SDL_CopyGPUTextureToTexture(
			copyPass,
			&(SDL_GPUTextureLocation){
				.texture = SrcTextures[SrcTextureIndex],
				.layer = (SrcTextureIndex == 2) ? 0 : BaseMipSlices[SrcTextureIndex],
				.z = (SrcTextureIndex == 2) ? BaseMipSlices[SrcTextureIndex] : 0,
			},
			&(SDL_GPUTextureLocation){
				.texture = DstTextures[DstTextureIndex],
				.layer = (DstTextureIndex == 2) ? 0 : BaseMipSlices[DstTextureIndex],
				.z = (DstTextureIndex == 2) ? BaseMipSlices[DstTextureIndex] : 0,
			},
			32,
			32,
			1,
			false
		);

		SDL_CopyGPUTextureToTexture(
			copyPass,
			&(SDL_GPUTextureLocation){
				.texture = SrcTextures[SrcTextureIndex],
				.mip_level = 1,
				.layer = (SrcTextureIndex == 2) ? 0 : SecondMipSlices[SrcTextureIndex],
				.z = (SrcTextureIndex == 2) ? SecondMipSlices[SrcTextureIndex] : 0,
			},
			&(SDL_GPUTextureLocation){
				.texture = DstTextures[DstTextureIndex],
				.mip_level = 1,
				.layer = (DstTextureIndex == 2) ? 0 : SecondMipSlices[DstTextureIndex],
				.z = (DstTextureIndex == 2) ? SecondMipSlices[DstTextureIndex] : 0,
			},
			16,
			16,
			1,
			false
		);

		SDL_EndGPUCopyPass(copyPass);

		// Blit the source texture and its mip
		SDL_BlitGPUTexture(
			cmdbuf,
			&(SDL_GPUBlitInfo){
				.source.texture = SrcTextures[SrcTextureIndex],
				.source.w = 32,
				.source.h = 32,
				.source.layer_or_depth_plane = BaseMipSlices[SrcTextureIndex],
				.destination.texture = swapchainTexture,
				.destination.w = 128,
				.destination.h = 128
			}
		);
		SDL_BlitGPUTexture(
			cmdbuf,
			&(SDL_GPUBlitInfo){
				.source.texture = SrcTextures[SrcTextureIndex],
				.source.w = 16,
				.source.h = 16,
				.source.mip_level = 1,
				.source.layer_or_depth_plane = SecondMipSlices[SrcTextureIndex],
				.destination.texture = swapchainTexture,
				.destination.x = 128,
				.destination.w = 64,
				.destination.h = 64
			}
		);

		// Blit the destination texture and its mip
		SDL_BlitGPUTexture(
			cmdbuf,
			&(SDL_GPUBlitInfo){
				.source.texture = DstTextures[DstTextureIndex],
				.source.w = 32,
				.source.h = 32,
				.source.layer_or_depth_plane = BaseMipSlices[DstTextureIndex],
				.destination.texture = swapchainTexture,
				.destination.x = 256,
				.destination.w = 128,
				.destination.h = 128
			}
		);
		SDL_BlitGPUTexture(
			cmdbuf,
			&(SDL_GPUBlitInfo){
				.source.texture = DstTextures[DstTextureIndex],
				.source.w = 16,
				.source.h = 16,
				.source.mip_level = 1,
				.source.layer_or_depth_plane = SecondMipSlices[DstTextureIndex],
				.destination.texture = swapchainTexture,
				.destination.x = 384,
				.destination.w = 64,
				.destination.h = 64
			}
		);

		SDL_SubmitGPUCommandBuffer(cmdbuf);
	}
	else
	{
		SDL_CancelGPUCommandBuffer(cmdbuf);
	}

	return 0;
}

static void Quit(Context* context)
{
	for (int i = 0; i < 5; i += 1)
	{
		SDL_ReleaseGPUTexture(context->Device, SrcTextures[i]);
		SDL_ReleaseGPUTexture(context->Device, DstTextures[i]);
	}

	CommonQuit(context);
}

Example TextureTypeTest_Example = { "TextureTypeTest", Init, Update, Draw, Quit };