#include "Common.h"

static SDL_GPUComputePipeline* GradientPipeline;
static SDL_GPUTexture* GradientRenderTexture;

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
        &(SDL_GPUComputePipelineCreateInfo) {
            .num_writeonly_storage_textures = 1,
            .num_uniform_buffers = 1,
            .threadcount_x = 8,
            .threadcount_y = 8,
            .threadcount_z = 1,
        }
    );

    int w, h;
    SDL_GetWindowSizeInPixels(context->Window, &w, &h);

    GradientRenderTexture = SDL_CreateGPUTexture(context->Device, &(SDL_GPUTextureCreateInfo){
        .format = SDL_GetGPUSwapchainTextureFormat(context->Device, context->Window),
        .type = SDL_GPU_TEXTURETYPE_2D,
        .width = w,
        .height = h,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE
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
    SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(context->Device);
    if (cmdbuf == NULL)
    {
        SDL_Log("GPUAcquireCommandBuffer failed!");
        return -1;
    }

    Uint32 w, h;
    SDL_GPUTexture* swapchainTexture = SDL_AcquireGPUSwapchainTexture(cmdbuf, context->Window, &w, &h);
    if (swapchainTexture != NULL)
    {
        SDL_GPUComputePass* computePass = SDL_BeginGPUComputePass(
            cmdbuf,
            (SDL_GPUStorageTextureWriteOnlyBinding[]){{
                .texture = GradientRenderTexture,
                .cycle = SDL_TRUE
            }},
            1,
            NULL,
            0
        );

        SDL_BindGPUComputePipeline(computePass, GradientPipeline);
        SDL_PushGPUComputeUniformData(cmdbuf, 0, &GradientUniformValues, sizeof(GradientUniforms));
        SDL_DispatchGPUCompute(computePass, w / 8 , h / 8 , 1);

        SDL_EndGPUComputePass(computePass);

        SDL_BlitGPUTexture(
            cmdbuf,
            &(SDL_GPUBlitInfo){
                .source.texture = GradientRenderTexture,
                .source.w = w,
                .source.h = h,
                .destination.texture = swapchainTexture,
                .destination.w = w,
                .destination.h = h,
                .load_op = SDL_GPU_LOADOP_DONT_CARE,
                .filter = SDL_GPU_FILTER_LINEAR
            }
        );
    }

    SDL_SubmitGPUCommandBuffer(cmdbuf);

    return 0;
}

static void Quit(Context* context)
{
    SDL_ReleaseGPUComputePipeline(context->Device, GradientPipeline);
    SDL_ReleaseGPUTexture(context->Device, GradientRenderTexture);

    CommonQuit(context);
}

Example ComputeUniforms_Example = { "ComputeUniforms", Init, Update, Draw, Quit };
