#include <SDL_gpu_examples.h>

static SDL_GpuGraphicsPipeline* FillPipeline;
static SDL_GpuGraphicsPipeline* LinePipeline;
static SDL_GpuViewport SmallViewport = { 160, 120, 320, 240, 0.1f, 1.0f };
static SDL_GpuRect ScissorRect = { 320, 240, 320, 240 };

static SDL_bool UseWireframeMode = SDL_FALSE;
static SDL_bool UseSmallViewport = SDL_FALSE;
static SDL_bool UseScissorRect = SDL_FALSE;

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
		.vertexShader = vertexShader,
		.fragmentShader = fragmentShader,
	};

	pipelineCreateInfo.rasterizerState.fillMode = SDL_GPU_FILLMODE_FILL;
	FillPipeline = SDL_GpuCreateGraphicsPipeline(context->Device, &pipelineCreateInfo);
	if (FillPipeline == NULL)
	{
		SDL_Log("Failed to create fill pipeline!");
		return -1;
	}

	pipelineCreateInfo.rasterizerState.fillMode = SDL_GPU_FILLMODE_LINE;
	LinePipeline = SDL_GpuCreateGraphicsPipeline(context->Device, &pipelineCreateInfo);
	if (LinePipeline == NULL)
	{
		SDL_Log("Failed to create line pipeline!");
		return -1;
	}

	// Clean up shader resources
	SDL_GpuReleaseShader(context->Device, vertexShader);
	SDL_GpuReleaseShader(context->Device, fragmentShader);

	// Finally, print instructions!
	SDL_Log("Press Left to toggle wireframe mode");
	SDL_Log("Press Down to toggle small viewport");
	SDL_Log("Press Right to toggle scissor rect");

	return 0;
}

static int Update(Context* context)
{
	if (context->LeftPressed)
	{
		UseWireframeMode = !UseWireframeMode;
	}

	if (context->DownPressed)
	{
		UseSmallViewport = !UseSmallViewport;
	}

	if (context->RightPressed)
	{
		UseScissorRect = !UseScissorRect;
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
		SDL_GpuColorAttachmentInfo colorAttachmentInfo = { 0 };
		colorAttachmentInfo.textureSlice.texture = swapchainTexture;
		colorAttachmentInfo.clearColor = (SDL_GpuColor){ 0.0f, 0.0f, 0.0f, 1.0f };
		colorAttachmentInfo.loadOp = SDL_GPU_LOADOP_CLEAR;
		colorAttachmentInfo.storeOp = SDL_GPU_STOREOP_STORE;

		SDL_GpuRenderPass* renderPass = SDL_GpuBeginRenderPass(cmdbuf, &colorAttachmentInfo, 1, NULL);
		SDL_GpuBindGraphicsPipeline(renderPass, UseWireframeMode ? LinePipeline : FillPipeline);
		if (UseSmallViewport)
		{
			SDL_GpuSetViewport(renderPass, &SmallViewport);
		}
		if (UseScissorRect)
		{
			SDL_GpuSetScissor(renderPass, &ScissorRect);
		}
		SDL_GpuDrawPrimitives(renderPass, 0, 1);
		SDL_GpuEndRenderPass(renderPass);
	}

	SDL_GpuSubmit(cmdbuf);

	return 0;
}

static void Quit(Context* context)
{
	SDL_GpuReleaseGraphicsPipeline(context->Device, FillPipeline);
	SDL_GpuReleaseGraphicsPipeline(context->Device, LinePipeline);

	UseWireframeMode = SDL_FALSE;
	UseSmallViewport = SDL_FALSE;
	UseScissorRect = SDL_FALSE;

	CommonQuit(context);
}

Example BasicTriangle_Example = { "BasicTriangle", Init, Update, Draw, Quit };
