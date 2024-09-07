#include "Common.h"

static SDL_GPUGraphicsPipeline* FillPipeline;
static SDL_GPUGraphicsPipeline* LinePipeline;
static SDL_GPUViewport SmallViewport = { 160, 120, 320, 240, 0.1f, 1.0f };
static SDL_Rect ScissorRect = { 320, 240, 320, 240 };

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
	SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {
		.target_info = {
			.num_color_targets = 1,
			.color_target_descriptions = (SDL_GPUColorTargetDescription[]){{
				.format = SDL_GetGPUSwapchainTextureFormat(context->Device, context->Window),
				.blend_state = {
					.enable_blend = SDL_TRUE,
					.alpha_blend_op = SDL_GPU_BLENDOP_ADD,
					.color_blend_op = SDL_GPU_BLENDOP_ADD,
					.color_write_mask = 0xF,
					.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
					.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
					.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ZERO,
					.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO
				}
			}},
		},
		.multisample_state.sample_mask = 0xFFFF,
		.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
		.vertex_shader = vertexShader,
		.fragment_shader = fragmentShader,
	};

	pipelineCreateInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
	FillPipeline = SDL_CreateGPUGraphicsPipeline(context->Device, &pipelineCreateInfo);
	if (FillPipeline == NULL)
	{
		SDL_Log("Failed to create fill pipeline!");
		return -1;
	}

	pipelineCreateInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_LINE;
	LinePipeline = SDL_CreateGPUGraphicsPipeline(context->Device, &pipelineCreateInfo);
	if (LinePipeline == NULL)
	{
		SDL_Log("Failed to create line pipeline!");
		return -1;
	}

	// Clean up shader resources
	SDL_ReleaseGPUShader(context->Device, vertexShader);
	SDL_ReleaseGPUShader(context->Device, fragmentShader);

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
	SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(context->Device);
	if (cmdbuf == NULL)
	{
		SDL_Log("GPUAcquireCommandBuffer failed");
		return -1;
	}

	Uint32 w, h;
	SDL_GPUTexture* swapchainTexture = SDL_AcquireGPUSwapchainTexture(cmdbuf, context->Window, &w, &h);
	if (swapchainTexture != NULL)
	{
		SDL_GPUColorTargetInfo colorTargetInfo = { 0 };
		colorTargetInfo.texture = swapchainTexture;
		colorTargetInfo.clear_color = (SDL_FColor){ 0.0f, 0.0f, 0.0f, 1.0f };
		colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
		colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

		SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTargetInfo, 1, NULL);
		SDL_BindGPUGraphicsPipeline(renderPass, UseWireframeMode ? LinePipeline : FillPipeline);
		if (UseSmallViewport)
		{
			SDL_SetGPUViewport(renderPass, &SmallViewport);
		}
		if (UseScissorRect)
		{
			SDL_SetGPUScissor(renderPass, &ScissorRect);
		}
		SDL_DrawGPUPrimitives(renderPass, 3, 1, 0, 0);
		SDL_EndGPURenderPass(renderPass);
	}

	SDL_SubmitGPUCommandBuffer(cmdbuf);

	return 0;
}

static void Quit(Context* context)
{
	SDL_ReleaseGPUGraphicsPipeline(context->Device, FillPipeline);
	SDL_ReleaseGPUGraphicsPipeline(context->Device, LinePipeline);

	UseWireframeMode = SDL_FALSE;
	UseSmallViewport = SDL_FALSE;
	UseScissorRect = SDL_FALSE;

	CommonQuit(context);
}

Example BasicTriangle_Example = { "BasicTriangle", Init, Update, Draw, Quit };
