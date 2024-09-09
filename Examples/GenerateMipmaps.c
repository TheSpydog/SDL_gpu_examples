#include "Common.h"

static SDL_GPUTexture *MipmapTexture;

static int Init(Context* context)
{
    int result = CommonInit(context, 0);
    if (result < 0)
    {
        return result;
    }

    MipmapTexture = SDL_CreateGPUTexture(
        context->Device,
        &(SDL_GPUTextureCreateInfo){
            .type = SDL_GPU_TEXTURETYPE_2D,
            .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
            .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
            .width = 32,
            .height = 32,
            .layer_count_or_depth = 1,
            .num_levels = 3
        }
    );

    Uint32 byteCount = 32 * 32 * 4;
    SDL_GPUTransferBuffer *textureTransferBuffer = SDL_CreateGPUTransferBuffer(
        context->Device,
        &(SDL_GPUTransferBufferCreateInfo) {
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .size = byteCount
        }
    );
    Uint8* textureTransferData = SDL_MapGPUTransferBuffer(
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

    SDL_UnmapGPUTransferBuffer(context->Device, textureTransferBuffer);

    SDL_GPUCommandBuffer *cmdbuf = SDL_AcquireGPUCommandBuffer(context->Device);
    SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmdbuf);
    SDL_UploadToGPUTexture(
        copyPass,
        &(SDL_GPUTextureTransferInfo){
            .transfer_buffer = textureTransferBuffer
        },
        &(SDL_GPUTextureRegion) {
            .texture = MipmapTexture,
            .w = 32,
            .h = 32,
            .d = 1
        },
        SDL_FALSE
    );
    SDL_EndGPUCopyPass(copyPass);
    SDL_GenerateMipmapsForGPUTexture(cmdbuf, MipmapTexture);

    SDL_SubmitGPUCommandBuffer(cmdbuf);

    SDL_ReleaseGPUTransferBuffer(context->Device, textureTransferBuffer);

    return 0;
}

static int Update(Context* context)
{
    return 0;
}

static int Draw(Context* context)
{
    SDL_GPUCommandBuffer *cmdbuf = SDL_AcquireGPUCommandBuffer(context->Device);
    Uint32 w, h;

    SDL_GPUTexture *swapchainTexture = SDL_AcquireGPUSwapchainTexture(cmdbuf, context->Window, &w, &h);
    if (swapchainTexture != NULL)
    {
        /* Blit the smallest mip level */
        SDL_BlitGPUTexture(
            cmdbuf,
            &(SDL_GPUBlitInfo){
                .source.texture = MipmapTexture,
                .source.w = 8,
                .source.h = 8,
                .source.mip_level = 2,
                .destination.texture = swapchainTexture,
                .destination.w = w,
                .destination.h = h,
                .load_op = SDL_GPU_LOADOP_DONT_CARE
            }
        );
    }

    SDL_SubmitGPUCommandBuffer(cmdbuf);

    return 0;
}

static void Quit(Context* context)
{
    SDL_ReleaseGPUTexture(context->Device, MipmapTexture);
    CommonQuit(context);
}

Example GenerateMipmaps_Example = { "GenerateMipmaps", Init, Update, Draw, Quit };
