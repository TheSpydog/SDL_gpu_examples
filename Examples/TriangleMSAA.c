#include "Common.h"

static SDL_GpuGraphicsPipeline* Pipelines[4];
static SDL_GpuTexture* MSAARenderTextures[4];

const SDL_GpuTextureFormat RTFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8;

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
		.rasterizerState = {
			.fillMode = SDL_GPU_FILLMODE_FILL,
			.cullMode = SDL_GPU_CULLMODE_NONE,
			.frontFace = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
			.depthBiasEnable = SDL_FALSE
		},
		.multisampleState.sampleCount = SDL_GPU_SAMPLECOUNT_1,
		.multisampleState.sampleMask = 0xFFFF,
		.primitiveType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
		.vertexShader = vertexShader,
		.fragmentShader = fragmentShader,
	};

	for (int i = 0; i < SDL_arraysize(Pipelines); i += 1)
	{
		SDL_GpuSampleCount sampleCount = SDL_GPU_SAMPLECOUNT_1 + (SDL_GpuSampleCount)i;
		pipelineCreateInfo.multisampleState.sampleCount = SDL_GpuGetBestSampleCount(
			context->Device,
			RTFormat,
			sampleCount
		);
		Pipelines[i] = SDL_GpuCreateGraphicsPipeline(context->Device, &pipelineCreateInfo);
		if (Pipelines[i] == NULL)
		{
			SDL_Log("Failed to create pipeline!");
			return -1;
		}
	}

	// Clean up shader resources
	SDL_GpuReleaseShader(context->Device, vertexShader);
	SDL_GpuReleaseShader(context->Device, fragmentShader);

	// Create the render target textures
	SDL_GpuTextureCreateInfo textureCreateInfo = {
		.width = 640,
		.height = 480,
		.depth = 1,
		.layerCount = 1,
		.levelCount = 1,
		.format = RTFormat,
		.usageFlags = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT | SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT,
		.sampleCount = SDL_GPU_SAMPLECOUNT_1
	};

	for (int i = 0; i < SDL_arraysize(MSAARenderTextures); i += 1)
	{
		textureCreateInfo.sampleCount = SDL_GPU_SAMPLECOUNT_1 + (SDL_GpuSampleCount)i;
		MSAARenderTextures[i] = SDL_GpuCreateTexture(context->Device, &textureCreateInfo);
		if (MSAARenderTextures[i] == NULL) {
			SDL_Log("Failed to create MSAA render target texture!");
		}
	}

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
			CurrentSampleCount = SDL_arraysize(Pipelines) - 1;
		}
		changed = 1;
	}

	if (context->RightPressed)
	{
		CurrentSampleCount = (CurrentSampleCount + 1) % SDL_arraysize(Pipelines);
		changed = 1;
	}

	if (changed)
	{
		SDL_GpuSampleCount best = SDL_GpuGetBestSampleCount(
			context->Device,
			RTFormat,
			SDL_GPU_SAMPLECOUNT_1 + (SDL_GpuSampleCount)CurrentSampleCount
		);

		if (best != (SDL_GPU_SAMPLECOUNT_1 + (SDL_GpuSampleCount)CurrentSampleCount))
		{
			SDL_Log("Sample count %d not supported! Falling back to %d", (1 << CurrentSampleCount), (1 << best));
		}
		else
		{
			SDL_Log("Current sample count: %d", (1 << CurrentSampleCount));
		}
	}

	return 0;
}

static int Draw(Context* context)
{
	SDL_GpuCommandBuffer* cmdbuf = SDL_GpuAcquireCommandBuffer(context->Device);
	if (cmdbuf == NULL)
	{
		SDL_Log("GpuAcquireCommandBuffer failed");
		return -1;
	}

	Uint32 w, h;
	SDL_GpuTexture* swapchainTexture = SDL_GpuAcquireSwapchainTexture(cmdbuf, context->Window, &w, &h);
	if (swapchainTexture != NULL)
	{
		SDL_GpuRenderPass* renderPass;
		SDL_GpuColorAttachmentInfo colorAttachmentInfo = {
			.textureSlice.texture = MSAARenderTextures[CurrentSampleCount],
			.clearColor = (SDL_FColor){ 1.0f, 1.0f, 1.0f, 1.0f },
			.loadOp = SDL_GPU_LOADOP_CLEAR,
			.storeOp = SDL_GPU_STOREOP_STORE
		};

		renderPass = SDL_GpuBeginRenderPass(cmdbuf, &colorAttachmentInfo, 1, NULL);
		SDL_GpuBindGraphicsPipeline(renderPass, Pipelines[CurrentSampleCount]);
		SDL_GpuDrawPrimitives(renderPass, 0, 3);
		SDL_GpuEndRenderPass(renderPass);

		SDL_GpuBlit(
			cmdbuf,
			&(SDL_GpuTextureRegion){
			.textureSlice.texture = MSAARenderTextures[CurrentSampleCount],
				.x = 160,
				.w = 320,
				.h = 240,
				.d = 1
		},
			& (SDL_GpuTextureRegion) {
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
	for (int i = 0; i < SDL_arraysize(Pipelines); i += 1)
	{
		SDL_GpuReleaseGraphicsPipeline(context->Device, Pipelines[i]);
		SDL_GpuReleaseTexture(context->Device, MSAARenderTextures[i]);
	}

	CurrentSampleCount = 0;

	CommonQuit(context);
}

Example TriangleMSAA_Example = { "TriangleMSAA", Init, Update, Draw, Quit };
