#include "Common.h"

static SDL_GPUGraphicsPipeline* Pipelines[4];
static SDL_GPUTexture* MSAARenderTextures[4];
static SDL_GPUTexture* ResolveTexture;
static int SampleCounts;

static SDL_GPUTextureFormat RTFormat;

static int CurrentSampleCount = 0;

static int Init(Context* context)
{
	int result = CommonInit(context, 0);
	if (result < 0)
	{
		return result;
	}

	// Create the shaders
	SDL_GPUShader* vertexShader = LoadShader(context->Device, "RawTriangle.vert", 0, 0, 0, 0);
	if (vertexShader == NULL)
	{
		SDL_Log("Failed to create vertex shader!");
		return -1;
	}

	SDL_GPUShader* fragmentShader = LoadShader(context->Device, "SolidColor.frag", 0, 0, 0, 0);
	if (fragmentShader == NULL)
	{
		SDL_Log("Failed to create fragment shader!");
		return -1;
	}

	// Create the pipelines
	RTFormat = SDL_GetGPUSwapchainTextureFormat(context->Device, context->Window);
	SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {
		.target_info = {
			.num_color_targets = 1,
			.color_target_descriptions = (SDL_GPUColorTargetDescription[]){{
				.format = RTFormat
			}},
		},
		.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
		.vertex_shader = vertexShader,
		.fragment_shader = fragmentShader,
	};

	SampleCounts = 0;
	for (int i = 0; i < SDL_arraysize(Pipelines); i += 1)
	{
		SDL_GPUSampleCount sample_count = (SDL_GPUSampleCount) i;
		if (!SDL_GPUTextureSupportsSampleCount(
			context->Device,
			RTFormat,
			sample_count)) {
			SDL_Log("Sample count %d not supported", (1 << CurrentSampleCount));
			continue;
		}
		pipelineCreateInfo.multisample_state.sample_count = sample_count;
		Pipelines[SampleCounts] = SDL_CreateGPUGraphicsPipeline(context->Device, &pipelineCreateInfo);
		if (Pipelines[SampleCounts] == NULL)
		{
			SDL_Log("Failed to create pipeline!");
			return -1;
		}
		// Create the render target textures
		SDL_GPUTextureCreateInfo textureCreateInfo = {
			.type = SDL_GPU_TEXTURETYPE_2D,
			.width = 640,
			.height = 480,
			.layer_count_or_depth = 1,
			.num_levels = 1,
			.format = RTFormat,
			.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
			.sample_count = sample_count
		};
		if (sample_count == SDL_GPU_SAMPLECOUNT_1)
		{
			textureCreateInfo.usage |= SDL_GPU_TEXTUREUSAGE_SAMPLER;
		}
		MSAARenderTextures[SampleCounts] = SDL_CreateGPUTexture(context->Device, &textureCreateInfo);
		if (MSAARenderTextures[SampleCounts] == NULL) {
			SDL_Log("Failed to create MSAA render target texture!");
			SDL_ReleaseGPUGraphicsPipeline(context->Device, Pipelines[SampleCounts]);
		}
		SampleCounts += 1;
	}

	// Create resolve texture
	ResolveTexture = SDL_CreateGPUTexture(
		context->Device,
		&(SDL_GPUTextureCreateInfo) {
			.type = SDL_GPU_TEXTURETYPE_2D,
			.width = 640,
			.height = 480,
			.layer_count_or_depth = 1,
			.num_levels = 1,
			.format = RTFormat,
			.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER
		}
	);

	// Clean up shader resources
	SDL_ReleaseGPUShader(context->Device, vertexShader);
	SDL_ReleaseGPUShader(context->Device, fragmentShader);

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
    SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(context->Device);
    if (cmdbuf == NULL)
    {
        SDL_Log("AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return -1;
    }

    SDL_GPUTexture* swapchainTexture;
	Uint32 w, h;
    if (!SDL_AcquireGPUSwapchainTexture(cmdbuf, context->Window, &swapchainTexture, &w, &h)) {
        SDL_Log("AcquireGPUSwapchainTexture failed: %s", SDL_GetError());
        return -1;
    }

	if (swapchainTexture != NULL)
	{
		SDL_GPURenderPass* renderPass;
		SDL_GPUColorTargetInfo colorTargetInfo = {
			.texture = MSAARenderTextures[CurrentSampleCount],
			.clear_color = (SDL_FColor){ 1.0f, 1.0f, 1.0f, 1.0f },
			.load_op = SDL_GPU_LOADOP_CLEAR,
		};

		if (CurrentSampleCount == SDL_GPU_SAMPLECOUNT_1)
		{
			colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;
		}
		else
		{
			colorTargetInfo.store_op = SDL_GPU_STOREOP_RESOLVE;
			colorTargetInfo.resolve_texture = ResolveTexture;
		}

		renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTargetInfo, 1, NULL);
		SDL_BindGPUGraphicsPipeline(renderPass, Pipelines[CurrentSampleCount]);
		SDL_DrawGPUPrimitives(renderPass, 3, 1, 0, 0);
		SDL_EndGPURenderPass(renderPass);

		SDL_GPUTexture* blitSourceTexture = (colorTargetInfo.resolve_texture != NULL) ? colorTargetInfo.resolve_texture : colorTargetInfo.texture;
		SDL_BlitGPUTexture(
			cmdbuf,
			&(SDL_GPUBlitInfo){
				.source.texture = blitSourceTexture,
				.source.x = 160,
				.source.w = 320,
				.source.h = 240,
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
	for (int i = 0; i < SampleCounts; i += 1)
	{
		SDL_ReleaseGPUGraphicsPipeline(context->Device, Pipelines[i]);
		SDL_ReleaseGPUTexture(context->Device, MSAARenderTextures[i]);
	}
	SDL_ReleaseGPUTexture(context->Device, ResolveTexture);

	CurrentSampleCount = 0;

	CommonQuit(context);
}

Example TriangleMSAA_Example = { "TriangleMSAA", Init, Update, Draw, Quit };
