#include "Common.h"

static SDL_GpuTexture *MipmapTexture;

static int Init(Context* context)
{
    int result = CommonInit(context, 0);
    if (result < 0)
    {
        return result;
    }

    MipmapTexture = SDL_GpuCreateTexture(
        context->Device,
        &(SDL_GpuTextureCreateInfo){
            .type = SDL_GPU_TEXTURETYPE_2D,
            .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8,
            .usageFlags = SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT,
            .width = 32,
            .height = 32,
            .layerCountOrDepth = 1,
            .levelCount = 3
        }
    );

    Uint32 byteCount = 32 * 32 * 4;
    SDL_GpuTransferBuffer *textureTransferBuffer = SDL_GpuCreateTransferBuffer(
        context->Device,
        &(SDL_GpuTransferBufferCreateInfo) {
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .sizeInBytes = byteCount
        }
    );
    Uint8* textureTransferData = SDL_GpuMapTransferBuffer(
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

    SDL_GpuUnmapTransferBuffer(context->Device, textureTransferBuffer);

    SDL_GpuCommandBuffer *cmdbuf = SDL_GpuAcquireCommandBuffer(context->Device);
    SDL_GpuCopyPass *copyPass = SDL_GpuBeginCopyPass(cmdbuf);
    SDL_GpuUploadToTexture(
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
    SDL_GpuEndCopyPass(copyPass);
    SDL_GpuGenerateMipmaps(cmdbuf, MipmapTexture);

    SDL_GpuSubmit(cmdbuf);

    SDL_GpuReleaseTransferBuffer(context->Device, textureTransferBuffer);

    return 0;
}

static int Update(Context* context)
{
    return 0;
}

static int Draw(Context* context)
{
    SDL_GpuCommandBuffer *cmdbuf = SDL_GpuAcquireCommandBuffer(context->Device);
    Uint32 w, h;

    SDL_GpuTexture *swapchainTexture = SDL_GpuAcquireSwapchainTexture(cmdbuf, context->Window, &w, &h);
    if (swapchainTexture != NULL)
    {
        /* Blit the smallest mip level */
        SDL_GpuBlit(
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

    SDL_GpuSubmit(cmdbuf);

    return 0;
}

static void Quit(Context* context)
{
    SDL_GpuReleaseTexture(context->Device, MipmapTexture);
    CommonQuit(context);
}

Example GenerateMipmaps_Example = { "GenerateMipmaps", Init, Update, Draw, Quit };
