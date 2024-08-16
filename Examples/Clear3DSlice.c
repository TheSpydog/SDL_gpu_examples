#include "Common.h"

static SDL_GpuTexture *Texture3D;
static SDL_GpuGraphicsPipeline *Sample3DPipeline;
static SDL_GpuSampler *Sampler;

static int Init(Context* context)
{
    int result = CommonInit(context, SDL_WINDOW_RESIZABLE);
    if (result < 0)
    {
        return result;
    }

    Texture3D = SDL_GpuCreateTexture(
        context->Device,
        &(SDL_GpuTextureCreateInfo){
            .type = SDL_GPU_TEXTURETYPE_3D,
            .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8,
            .width = 64,
            .height = 64,
            .depth = 4,
            .levelCount = 1,
            .layerCount = 1,
            .usageFlags = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT | SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT
        }
    );

    SDL_GpuShader *vertShader = LoadShader(
        context->Device,
        "Fullscreen.vert",
        0,
        0,
        0,
        0
    );

    SDL_GpuShader *fragShader = LoadShader(
        context->Device,
        "Sample3D.frag",
        1,
        0,
        0,
        0
    );

    Sample3DPipeline = SDL_GpuCreateGraphicsPipeline(
        context->Device,
        &(SDL_GpuGraphicsPipelineCreateInfo){
            .attachmentInfo = {
                .colorAttachmentCount = 1,
                .colorAttachmentDescriptions = (SDL_GpuColorAttachmentDescription[]){{
                    .format = SDL_GpuGetSwapchainTextureFormat(context->Device, context->Window),
                    .blendState = {
                        .blendEnable = SDL_TRUE,
                        .alphaBlendOp = SDL_GPU_BLENDOP_ADD,
                        .colorBlendOp = SDL_GPU_BLENDOP_ADD,
                        .colorWriteMask = 0xF,
                        .srcColorBlendFactor = SDL_GPU_BLENDFACTOR_ONE,
                        .srcAlphaBlendFactor = SDL_GPU_BLENDFACTOR_ONE,
                        .dstColorBlendFactor = SDL_GPU_BLENDFACTOR_ZERO,
                        .dstAlphaBlendFactor = SDL_GPU_BLENDFACTOR_ZERO
                    }
                }},
            },
            .multisampleState.sampleMask = 0xFFFF,
            .primitiveType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
            .vertexShader = vertShader,
            .fragmentShader = fragShader
        }
    );

	Sampler = SDL_GpuCreateSampler(
		context->Device,
		&(SDL_GpuSamplerCreateInfo){
			.minFilter = SDL_GPU_FILTER_NEAREST,
			.magFilter = SDL_GPU_FILTER_NEAREST,
			.mipmapMode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
			.addressModeU = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
			.addressModeV = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
			.addressModeW = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE
		}
	);

    SDL_GpuReleaseShader(context->Device, vertShader);
    SDL_GpuReleaseShader(context->Device, fragShader);

    return 0;
}

static int Update(Context* context)
{
    return 0;
}

static int Draw(Context* context)
{
    Uint32 w, h;

    SDL_GpuCommandBuffer *cmdbuf = SDL_GpuAcquireCommandBuffer(context->Device);
    SDL_GpuTexture *swapchainTexture = SDL_GpuAcquireSwapchainTexture(
        cmdbuf,
        context->Window,
        &w,
        &h
    );

    if (swapchainTexture != NULL)
    {
        SDL_GpuRenderPass *renderPass = SDL_GpuBeginRenderPass(
            cmdbuf,
            (SDL_GpuColorAttachmentInfo[]){
                {
                    .texture = Texture3D,
                    .cycle = SDL_TRUE,
                    .loadOp = SDL_GPU_LOADOP_CLEAR,
                    .storeOp = SDL_GPU_STOREOP_STORE,
                    .clearColor = { 1.0f, 0.0f, 0.0f, 1.0f },
                    .layerOrDepthPlane = 0
                },
                {
                    .texture = Texture3D,
                    .cycle = SDL_FALSE,
                    .loadOp = SDL_GPU_LOADOP_CLEAR,
                    .storeOp = SDL_GPU_STOREOP_STORE,
                    .clearColor = { 0.0f, 1.0f, 0.0f, 1.0f },
                    .layerOrDepthPlane = 1
                },
                {
                    .texture = Texture3D,
                    .cycle = SDL_FALSE,
                    .loadOp = SDL_GPU_LOADOP_CLEAR,
                    .storeOp = SDL_GPU_STOREOP_STORE,
                    .clearColor = { 0.0f, 0.0f, 1.0f, 1.0f },
                    .layerOrDepthPlane = 2
                },
                {
                    .texture = Texture3D,
                    .cycle = SDL_FALSE,
                    .loadOp = SDL_GPU_LOADOP_CLEAR,
                    .storeOp = SDL_GPU_STOREOP_STORE,
                    .clearColor = { 1.0f, 0.0f, 1.0f, 1.0f },
                    .layerOrDepthPlane = 3
                }
            },
            4,
            NULL
        );
        SDL_GpuEndRenderPass(renderPass);

        renderPass = SDL_GpuBeginRenderPass(
            cmdbuf,
            (SDL_GpuColorAttachmentInfo[]){{
                .texture = swapchainTexture,
                .loadOp = SDL_GPU_LOADOP_DONT_CARE,
                .storeOp = SDL_GPU_STOREOP_STORE,
            }},
            1,
            NULL
        );

        SDL_GpuBindGraphicsPipeline(renderPass, Sample3DPipeline);
        SDL_GpuBindFragmentSamplers(
            renderPass,
            0,
            &(SDL_GpuTextureSamplerBinding){
                .texture = Texture3D,
                .sampler = Sampler
            },
            1
        );
        SDL_GpuDrawPrimitives(renderPass, 0, 3);

        SDL_GpuEndRenderPass(renderPass);
    }

    SDL_GpuSubmit(cmdbuf);

    return 0;
}

static void Quit(Context* context)
{
    SDL_GpuReleaseSampler(context->Device, Sampler);
    SDL_GpuReleaseTexture(context->Device, Texture3D);
    SDL_GpuReleaseGraphicsPipeline(context->Device, Sample3DPipeline);
    CommonQuit(context);
}

Example Clear3DSlice_Example = { "Clear3DSlice", Init, Update, Draw, Quit };
