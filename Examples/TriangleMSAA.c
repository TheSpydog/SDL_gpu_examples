#include "Common.h"

static SDL_GpuGraphicsPipeline* Pipelines[4];
static SDL_GpuTexture* MSAARenderTextures[4];
static int SampleCounts;

static SDL_GpuTextureFormat RTFormat;

static int CurrentSampleCount = 0;

static int Init(Context* context)
{
	int result = CommonInit(context, 0);
	if (result < 0)
	{
		return result;
	}

	// Create the shaders
	SDL_GpuShader* vertexShader = LoadShader(context->Device, "RawTriangle.vert", 0, 0, 0, 0);
	if (vertexShader == NULL)
	{
		SDL_Log("Failed to create vertex shader!");
		return -1;
	}

	SDL_GpuShader* fragmentShader = LoadShader(context->Device, "SolidColor.frag", 0, 0, 0, 0);
	if (fragmentShader == NULL)
	{
		SDL_Log("Failed to create fragment shader!");
		return -1;
	}

	// Create the pipelines
	RTFormat = SDL_GetGpuSwapchainTextureFormat(context->Device, context->Window);
	SDL_GpuGraphicsPipelineCreateInfo pipelineCreateInfo = {
		.attachmentInfo = {
			.colorAttachmentCount = 1,
			.colorAttachmentDescriptions = (SDL_GpuColorAttachmentDescription[]){{
				.format = RTFormat,
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
		.vertexShader = vertexShader,
		.fragmentShader = fragmentShader,
	};

	SampleCounts = 0;
	for (int i = 0; i < SDL_arraysize(Pipelines); i += 1)
	{
		SDL_GpuSampleCount sampleCount = (SDL_GpuSampleCount) i;
		if (!SDL_SupportsGpuSampleCount(
			context->Device,
			RTFormat,
			sampleCount)) {
			SDL_Log("Sample count %d not supported", (1 << CurrentSampleCount));
			continue;
		}
		pipelineCreateInfo.multisampleState.sampleCount = sampleCount;
		Pipelines[SampleCounts] = SDL_CreateGpuGraphicsPipeline(context->Device, &pipelineCreateInfo);
		if (Pipelines[SampleCounts] == NULL)
		{
			SDL_Log("Failed to create pipeline!");
			return -1;
		}
		// Create the render target textures
		SDL_GpuTextureCreateInfo textureCreateInfo = {
			.type = SDL_GPU_TEXTURETYPE_2D,
			.width = 640,
			.height = 480,
			.layerCountOrDepth = 1,
			.levelCount = 1,
			.format = RTFormat,
			.usageFlags = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT | SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT,
			.sampleCount = sampleCount
		};
		MSAARenderTextures[SampleCounts] = SDL_CreateGpuTexture(context->Device, &textureCreateInfo);
		if (MSAARenderTextures[SampleCounts] == NULL) {
			SDL_Log("Failed to create MSAA render target texture!");
			SDL_ReleaseGpuGraphicsPipeline(context->Device, Pipelines[SampleCounts]);
		}
		SampleCounts += 1;
	}

	// Clean up shader resources
	SDL_ReleaseGpuShader(context->Device, vertexShader);
	SDL_ReleaseGpuShader(context->Device, fragmentShader);

	// Print the instructions
	SDL_Log("Press Left/Right to cycle between sample counts");
	SDL_Log("Current sample count: %d", (1 << CurrentSampleCount));

	return 0;
}

static int Update(Context* context)
{
	int changed = 0;

	if (context->LeftPressed)
	{
		CurrentSampleCount -= 1;
		if (CurrentSampleCount < 0)
		{
			CurrentSampleCount = SampleCounts - 1;
		}
		changed = 1;
	}

	if (context->RightPressed)
	{
		CurrentSampleCount = (CurrentSampleCount + 1) % SampleCounts;
		changed = 1;
	}

	if (changed)
	{
		SDL_Log("Current sample count: %d", (1 << CurrentSampleCount));
	}

	return 0;
}

static int Draw(Context* context)
{
	SDL_GpuCommandBuffer* cmdbuf = SDL_AcquireGpuCommandBuffer(context->Device);
	if (cmdbuf == NULL)
	{
		SDL_Log("GpuAcquireCommandBuffer failed");
		return -1;
	}

	Uint32 w, h;
	SDL_GpuTexture* swapchainTexture = SDL_AcquireGpuSwapchainTexture(cmdbuf, context->Window, &w, &h);
	if (swapchainTexture != NULL)
	{
		SDL_GpuRenderPass* renderPass;
		SDL_GpuColorAttachmentInfo colorAttachmentInfo = {
			.texture = MSAARenderTextures[CurrentSampleCount],
			.clearColor = (SDL_FColor){ 1.0f, 1.0f, 1.0f, 1.0f },
			.loadOp = SDL_GPU_LOADOP_CLEAR,
			.storeOp = SDL_GPU_STOREOP_STORE
		};

		renderPass = SDL_BeginGpuRenderPass(cmdbuf, &colorAttachmentInfo, 1, NULL);
		SDL_BindGpuGraphicsPipeline(renderPass, Pipelines[CurrentSampleCount]);
		SDL_DrawGpuPrimitives(renderPass, 3, 1, 0, 0);
		SDL_EndGpuRenderPass(renderPass);

		SDL_BlitGpu(
			cmdbuf,
			&(SDL_BlitGpuRegion){
				.texture = MSAARenderTextures[CurrentSampleCount],
				.x = 160,
				.w = 320,
				.h = 240,
			},
			&(SDL_BlitGpuRegion){
				.texture = swapchainTexture,
				.w = w,
				.h = h,
			},
			SDL_FLIP_NONE,
			SDL_GPU_FILTER_LINEAR,
			SDL_FALSE
		);
	}

	SDL_SubmitGpu(cmdbuf);

	return 0;
}

static void Quit(Context* context)
{
	for (int i = 0; i < SampleCounts; i += 1)
	{
		SDL_ReleaseGpuGraphicsPipeline(context->Device, Pipelines[i]);
		SDL_ReleaseGpuTexture(context->Device, MSAARenderTextures[i]);
	}

	CurrentSampleCount = 0;

	CommonQuit(context);
}

Example TriangleMSAA_Example = { "TriangleMSAA", Init, Update, Draw, Quit };
