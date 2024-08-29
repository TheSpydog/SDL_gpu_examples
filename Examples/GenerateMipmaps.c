#include "Common.h"

static SDL_GpuTexture *MipmapTexture;

static int Init(Context* context)
{
    int result = CommonInit(context, 0);
    if (result < 0)
    {
        return result;
    }

    MipmapTexture = SDL_CreateGpuTexture(
        context->Device,
        &(SDL_GpuTextureCreateInfo){
            .type = SDL_GPU_TEXTURETYPE_2D,
            .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
            .usageFlags = SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT,
            .width = 32,
            .height = 32,
            .layerCountOrDepth = 1,
            .levelCount = 3
        }
    );

    Uint32 byteCount = 32 * 32 * 4;
    SDL_GpuTransferBuffer *textureTransferBuffer = SDL_CreateGpuTransferBuffer(
        context->Device,
        &(SDL_GpuTransferBufferCreateInfo) {
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .sizeInBytes = byteCount
        }
    );
    Uint8* textureTransferData = SDL_MapGpuTransferBuffer(
        context->Device,
        textureTransferBuffer,
        SDL_FALSE
    );

    SDL_Surface* imageData = LoadImage("cube0.bmp", 4);
    if (imageData == NULL)
    {
        SDL_Log("Could not load image data!");
        return -1;
    }
    SDL_memcpy(textureTransferData, imageData->pixels, byteCount);
    SDL_DestroySurface(imageData);

    SDL_UnmapGpuTransferBuffer(context->Device, textureTransferBuffer);

    SDL_GpuCommandBuffer *cmdbuf = SDL_AcquireGpuCommandBuffer(context->Device);
    SDL_GpuCopyPass *copyPass = SDL_BeginGpuCopyPass(cmdbuf);
    SDL_UploadToGpuTexture(
        copyPass,
        &(SDL_GpuTextureTransferInfo){
            .transferBuffer = textureTransferBuffer
        },
        &(SDL_GpuTextureRegion) {
            .texture = MipmapTexture,
            .w = 32,
            .h = 32,
            .d = 1
        },
        SDL_FALSE
    );
    SDL_EndGpuCopyPass(copyPass);
    SDL_GenerateGpuMipmaps(cmdbuf, MipmapTexture);

    SDL_SubmitGpu(cmdbuf);

    SDL_ReleaseGpuTransferBuffer(context->Device, textureTransferBuffer);

    return 0;
}

static int Update(Context* context)
{
    return 0;
}

static int Draw(Context* context)
{
    SDL_GpuCommandBuffer *cmdbuf = SDL_AcquireGpuCommandBuffer(context->Device);
    Uint32 w, h;

    SDL_GpuTexture *swapchainTexture = SDL_AcquireGpuSwapchainTexture(cmdbuf, context->Window, &w, &h);
    if (swapchainTexture != NULL)
    {
        /* Blit the smallest mip level */
        SDL_BlitGpu(
            cmdbuf,
            &(SDL_GpuBlitRegion){
                .texture = MipmapTexture,
                .w = 8,
                .h = 8,
                .mipLevel = 2
            },
            &(SDL_GpuBlitRegion){
                .texture = swapchainTexture,
                .w = w,
                .h = h,
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
    SDL_ReleaseGpuTexture(context->Device, MipmapTexture);
    CommonQuit(context);
}

Example GenerateMipmaps_Example = { "GenerateMipmaps", Init, Update, Draw, Quit };
