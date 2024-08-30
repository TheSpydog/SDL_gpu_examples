#include "Common.h"

static SDL_GPUTexture *Texture3D;

static int Init(Context* context)
{
    int result = CommonInit(context, SDL_WINDOW_RESIZABLE);
    if (result < 0)
    {
        return result;
    }

    SDL_GPUTextureFormat swapchainFormat = SDL_GetGPUSwapchainTextureFormat(context->Device, context->Window);

    Texture3D = SDL_CreateGPUTexture(
        context->Device,
        &(SDL_GPUTextureCreateInfo){
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

    SDL_GPUCommandBuffer *cmdbuf = SDL_AcquireGPUCommandBuffer(context->Device);
    SDL_GPUTexture *swapchainTexture = SDL_AcquireGPUSwapchainTexture(
        cmdbuf,
        context->Window,
        &w,
        &h
    );

    if (swapchainTexture != NULL)
    {
        SDL_GPURenderPass *renderPass = SDL_BeginGPURenderPass(
            cmdbuf,
            &(SDL_GPUColorAttachmentInfo){
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
        SDL_EndGPURenderPass(renderPass);

        renderPass = SDL_BeginGPURenderPass(
            cmdbuf,
            &(SDL_GPUColorAttachmentInfo){
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
        SDL_EndGPURenderPass(renderPass);

        renderPass = SDL_BeginGPURenderPass(
            cmdbuf,
            &(SDL_GPUColorAttachmentInfo){
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
        SDL_EndGPURenderPass(renderPass);

        renderPass = SDL_BeginGPURenderPass(
            cmdbuf,
            &(SDL_GPUColorAttachmentInfo){
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
        SDL_EndGPURenderPass(renderPass);

        for (int i = 0; i < 4; i += 1) {
            Uint32 destX = (i % 2) * (w / 2);
            Uint32 destY = (i > 1) ? (h / 2) : 0;
            SDL_BlitGPUTexture(
                cmdbuf,
                &(SDL_GPUBlitRegion){
                    .texture = Texture3D,
                    .layerOrDepthPlane = i,
                    .w = 64,
                    .h = 64,
                },
                &(SDL_GPUBlitRegion){
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

    SDL_SubmitGPUCommandBuffer(cmdbuf);

    return 0;
}

static void Quit(Context* context)
{
    SDL_ReleaseGPUTexture(context->Device, Texture3D);
    CommonQuit(context);
}

Example Clear3DSlice_Example = { "Clear3DSlice", Init, Update, Draw, Quit };
