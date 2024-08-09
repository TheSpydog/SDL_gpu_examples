#include "Common.h"

static SDL_GpuComputePipeline* GradientPipeline;
static SDL_GpuTexture* GradientRenderTexture;

typedef struct GradientUniforms
{
    float time;
} GradientUniforms;

static GradientUniforms GradientUniformValues;

static int Init(Context* context)
{
    int result = CommonInit(context, 0);
    if (result < 0)
    {
        return result;
    }

    GradientPipeline = CreateComputePipelineFromShader(
        context->Device,
        "GradientTexture.comp",
        &(SDL_GpuComputePipelineCreateInfo) {
            .readWriteStorageTextureCount = 1,
            .uniformBufferCount = 1,
            .threadCountX = 8,
            .threadCountY = 8,
            .threadCountZ = 1,
        }
    );

    int w, h;
    SDL_GetWindowSizeInPixels(context->Window, &w, &h);

    GradientRenderTexture = SDL_GpuCreateTexture(context->Device, &(SDL_GpuTextureCreateInfo){
        .width = w,
        .height = h,
        .depth = 1,
        .layerCount = 1,
        .levelCount = 1,
        .usageFlags = SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE_BIT,
        .sampleCount = SDL_GPU_SAMPLECOUNT_1,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8,
    });

    GradientUniformValues.time = 0;

    return 0;
}

static int Update(Context* context)
{
    GradientUniformValues.time += 0.01f;

    return 0;
}

static int Draw(Context* context)
{
    SDL_GpuCommandBuffer* cmdbuf = SDL_GpuAcquireCommandBuffer(context->Device);
    if (cmdbuf == NULL)
    {
        SDL_Log("GpuAcquireCommandBuffer failed!");
        return -1;
    }

    Uint32 w, h;
    SDL_GpuTexture* swapchainTexture = SDL_GpuAcquireSwapchainTexture(cmdbuf, context->Window, &w, &h);
    if (swapchainTexture != NULL)
    {
        SDL_GpuComputePass* computePass = SDL_GpuBeginComputePass(
            cmdbuf,
            (SDL_GpuStorageTextureReadWriteBinding[]){{
                .textureSlice.texture = GradientRenderTexture,
                .cycle = SDL_TRUE
            }},
            1,
            NULL,
            0
        );

        SDL_GpuBindComputePipeline(computePass, GradientPipeline);
        SDL_GpuPushComputeUniformData(cmdbuf, 0, &GradientUniformValues, sizeof(GradientUniforms));
        SDL_GpuDispatchCompute(computePass, w / 8 , h / 8 , 1);

        SDL_GpuEndComputePass(computePass);

        SDL_GpuBlit(
            cmdbuf,
            &(SDL_GpuTextureRegion){
                .textureSlice.texture = GradientRenderTexture,
                .w = w,
                .h = h,
                .d = 1
            },
            &(SDL_GpuTextureRegion){
                .textureSlice.texture = swapchainTexture,
                .w = w,
                .h = h,
                .d = 1
            },
            SDL_GPU_FILTER_LINEAR,
            SDL_FALSE
        );
    }

    SDL_GpuSubmit(cmdbuf);

    return 0;
}

static void Quit(Context* context)
{
    SDL_GpuReleaseComputePipeline(context->Device, GradientPipeline);
    SDL_GpuReleaseTexture(context->Device, GradientRenderTexture);

    CommonQuit(context);
}

Example ComputeUniforms_Example = { "ComputeUniforms", Init, Update, Draw, Quit };
