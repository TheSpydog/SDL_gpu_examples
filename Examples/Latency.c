#include "Common.h"

static SDL_GPUTexture *LagTexture;
static int LagX = 1;
static bool CaptureCursor = false;
static int allowedFramesInFlight;
static int fullscreen;

static int Init(Context* context)
{
    int result = CommonInit(context, 0);
    if (result < 0)
    {
        return result;
    }

    LagTexture = SDL_CreateGPUTexture(
        context->Device,
        &(SDL_GPUTextureCreateInfo){
            .type = SDL_GPU_TEXTURETYPE_2D,
            .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
            .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
            .width = 8,
            .height = 32,
            .layer_count_or_depth = 1,
            .num_levels = 1
        }
    );

    Uint32 byteCount = 8 * 32 * 4;
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
        false
    );

    SDL_Surface* imageData = LoadImage("latency.bmp", 4);
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
            .texture = LagTexture,
            .w = 8,
            .h = 32,
            .d = 1
        },
        false
    );
    SDL_EndGPUCopyPass(copyPass);

    SDL_SubmitGPUCommandBuffer(cmdbuf);

    SDL_ReleaseGPUTransferBuffer(context->Device, textureTransferBuffer);

    SDL_Log("Press Left/Right to toggle capturing the mouse cursor.");
    SDL_Log("Press Down to change the number of allowed frames in flight.");
    SDL_Log("Press Up to toggle fullscreen mode.");
    SDL_Log("When the mouse cursor is captured the color directly above the cursor's point in the "
            "result of the test.");
    SDL_Log("Negative lag can occur when the cursor is below the tear line when tearing is enabled "
            "as the cursor is only moved during V-blank so it lags the framebuffer update.");
    SDL_Log("  Gray:  -1 frames lag");
    SDL_Log("  White:  0 frames lag");
    SDL_Log("  Green:  1 frames lag");
    SDL_Log("  Yellow: 2 frames lag");
    SDL_Log("  Red:    3 frames lag");
    SDL_Log("  Cyan:   4 frames lag");
    SDL_Log("  Purple: 5 frames lag");
    SDL_Log("  Blue:   6 frames lag");

    allowedFramesInFlight = 2;
    fullscreen = false;

    SDL_SetGPUAllowedFramesInFlight(context->Device, allowedFramesInFlight);
    return 0;
}

static int Update(Context* context)
{
    if (context->LeftPressed || context->RightPressed)
    {
        CaptureCursor = !CaptureCursor;
    }

    if (context->DownPressed)
    {
        allowedFramesInFlight = SDL_clamp((allowedFramesInFlight + 1) % 4, 1, 3);
        SDL_SetGPUAllowedFramesInFlight(context->Device, allowedFramesInFlight);
        SDL_Log("Allowed frames in flight: %i", allowedFramesInFlight);
    }

    if (context->UpPressed)
    {
        fullscreen = !fullscreen;
        SDL_SetWindowFullscreen(context->Window, fullscreen);
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
    Uint32 w, h;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdbuf, context->Window, &swapchainTexture, &w, &h)) {
        SDL_Log("WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
        return -1;
    }

    if (swapchainTexture != NULL)
    {
        // Get the current mouse cursor position. We use SDL_GetGlobalMouseState() as it actively
        // queries the latest position of the mouse unlike SDL_GetMouseState() which uses a cached
        // value.
        float cursorX, cursorY;
        SDL_GetGlobalMouseState(&cursorX, &cursorY);
        int winX, winY;
        SDL_GetWindowPosition(context->Window, &winX, &winY);
        cursorX -= winX;
        cursorY -= winY;

        if (CaptureCursor)
        {
            // Move the cursor to a known position.
            cursorX = LagX;
            SDL_WarpMouseInWindow(context->Window, cursorX, cursorY);
            if (LagX >= w - 8)
            {
                LagX = 1;
            }
            else
            {
                ++LagX;
            }
        }

        // Draw a sprite directly under the cursor if permitted by the blitting engine.
        if (cursorX >= 1 && cursorX <= w - 8 && cursorY >= 5 && cursorY <= h - 27)
        {
            SDL_BlitGPUTexture(
                cmdbuf,
                &(SDL_GPUBlitInfo){
                    .source.texture = LagTexture,
                    .source.w = 8,
                    .source.h = 32,
                    .source.mip_level = 0,
                    .destination.texture = swapchainTexture,
                    .destination.x = cursorX - 1,
                    .destination.y = cursorY - 5,
                    .destination.w = 8,
                    .destination.h = 32,
                    .load_op = SDL_GPU_LOADOP_CLEAR,
                    .clear_color = (SDL_FColor){ 0.0f, 0.0f, 0.0f, 1.0f }
                }
            );
        } else {
            SDL_GPUColorTargetInfo colorTargetInfo = { 0 };
            colorTargetInfo.texture = swapchainTexture;
            colorTargetInfo.clear_color = (SDL_FColor){ 0.0f, 0.0f, 0.0f, 1.0f };
            colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
            colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;
            SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTargetInfo, 1, NULL);
            SDL_EndGPURenderPass(renderPass);
        }
    }

    SDL_SubmitGPUCommandBuffer(cmdbuf);

    return 0;
}

static void Quit(Context* context)
{
    SDL_ReleaseGPUTexture(context->Device, LagTexture);
    CommonQuit(context);
}

Example Latency_Example = { "Latency", Init, Update, Draw, Quit };
