#include "Common.h"

static SDL_GpuTexture *Texture3D;

static int Init(Context* context)
{
    int result = CommonInit(context, SDL_WINDOW_RESIZABLE);
    if (result < 0)
    {
        return result;
    }

    SDL_GpuTextureFormat swapchainFormat = SDL_GetGpuSwapchainTextureFormat(context->Device, context->Window);

    Texture3D = SDL_CreateGpuTexture(
        context->Device,
        &(SDL_GpuTextureCreateInfo){
            .type = SDL_GPU_TEXTURETYPE_3D,
            .format = swapchainFormat,
            .width = 64,
            .height = 64,
            .layerCountOrDepth = 4,
            .levelCount = 1,
            .usageFlags = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT | SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT
        }
    );

    return 0;
}

static int Update(Context* context)
{
    return 0;
}

static int Draw(Context* context)
{
    Uint32 w, h;

    SDL_GpuCommandBuffer *cmdbuf = SDL_AcquireGpuCommandBuffer(context->Device);
    SDL_GpuTexture *swapchainTexture = SDL_AcquireGpuSwapchainTexture(
        cmdbuf,
        context->Window,
        &w,
        &h
    );

    if (swapchainTexture != NULL)
    {
        SDL_GpuRenderPass *renderPass = SDL_BeginGpuRenderPass(
            cmdbuf,
            &(SDL_GpuColorAttachmentInfo){
                .texture = Texture3D,
                .cycle = SDL_TRUE,
                .loadOp = SDL_GPU_LOADOP_CLEAR,
                .storeOp = SDL_GPU_STOREOP_STORE,
                .clearColor = { 1.0f, 0.0f, 0.0f, 1.0f },
                .layerOrDepthPlane = 0
            },
            1,
            NULL
        );
        SDL_EndGpuRenderPass(renderPass);

        renderPass = SDL_BeginGpuRenderPass(
            cmdbuf,
            &(SDL_GpuColorAttachmentInfo){
                .texture = Texture3D,
                .cycle = SDL_FALSE,
                .loadOp = SDL_GPU_LOADOP_CLEAR,
                .storeOp = SDL_GPU_STOREOP_STORE,
                .clearColor = { 0.0f, 1.0f, 0.0f, 1.0f },
                .layerOrDepthPlane = 1
            },
            1,
            NULL
        );
        SDL_EndGpuRenderPass(renderPass);

        renderPass = SDL_BeginGpuRenderPass(
            cmdbuf,
            &(SDL_GpuColorAttachmentInfo){
                .texture = Texture3D,
                .cycle = SDL_FALSE,
                .loadOp = SDL_GPU_LOADOP_CLEAR,
                .storeOp = SDL_GPU_STOREOP_STORE,
                .clearColor = { 0.0f, 0.0f, 1.0f, 1.0f },
                .layerOrDepthPlane = 2
            },
            1,
            NULL
        );
        SDL_EndGpuRenderPass(renderPass);

        renderPass = SDL_BeginGpuRenderPass(
            cmdbuf,
            &(SDL_GpuColorAttachmentInfo){
                .texture = Texture3D,
                .cycle = SDL_FALSE,
                .loadOp = SDL_GPU_LOADOP_CLEAR,
                .storeOp = SDL_GPU_STOREOP_STORE,
                .clearColor = { 1.0f, 0.0f, 1.0f, 1.0f },
                .layerOrDepthPlane = 3
            },
            1,
            NULL
        );
        SDL_EndGpuRenderPass(renderPass);

        for (int i = 0; i < 4; i += 1) {
            Uint32 destX = (i % 2) * (w / 2);
            Uint32 destY = (i > 1) ? (h / 2) : 0;
            SDL_BlitGpu(
                cmdbuf,
                &(SDL_BlitGpuRegion){
                    .texture = Texture3D,
                    .layerOrDepthPlane = i,
                    .w = 64,
                    .h = 64,
                },
                &(SDL_BlitGpuRegion){
                    .texture = swapchainTexture,
                    .x = destX,
                    .y = destY,
                    .w = (w / 2),
                    .h = (h / 2),
                },
                SDL_FLIP_NONE,
                SDL_GPU_FILTER_NEAREST,
                SDL_FALSE
            );
        }
    }

    SDL_SubmitGpu(cmdbuf);

    return 0;
}

static void Quit(Context* context)
{
    SDL_ReleaseGpuTexture(context->Device, Texture3D);
    CommonQuit(context);
}

Example Clear3DSlice_Example = { "Clear3DSlice", Init, Update, Draw, Quit };
