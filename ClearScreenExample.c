#include "SDL_gpu_examples.h"

static int Init(Context* context)
{
	context->Device = SDL_GpuCreateDevice(SDL_GPU_BACKEND_ALL, SDL_TRUE);
	if (context->Device == NULL)
	{
		SDL_Log("GpuCreateDevice failed");
		return -1;
	}

	context->Window = SDL_CreateWindow("ClearScreen", 640, 480, SDL_WINDOW_RESIZABLE);
	if (context->Window == NULL)
	{
		SDL_Log("CreateWindow failed: %s", SDL_GetError());
		return -1;
	}

	if (!SDL_GpuClaimWindow(context->Device, context->Window, SDL_GPU_COLORSPACE_NONLINEAR_SRGB, SDL_TRUE))
	{
		SDL_Log("GpuClaimWindow failed");
		return -1;
	}

	return 0;
}

static int Update(Context* context)
{
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
		colorAttachmentInfo.clearColor = (SDL_GpuColor){ 0.3f, 0.4f, 0.5f, 1.0f };
		colorAttachmentInfo.loadOp = SDL_GPU_LOADOP_CLEAR;
		colorAttachmentInfo.storeOp = SDL_GPU_STOREOP_STORE;

		SDL_GpuRenderPass* renderPass = SDL_GpuBeginRenderPass(cmdbuf, &colorAttachmentInfo, 1, NULL);
		SDL_GpuEndRenderPass(renderPass);
	}

	SDL_GpuSubmit(cmdbuf);

	return 0;
}

static int Quit(Context* context)
{
	SDL_GpuUnclaimWindow(context->Device, context->Window);
	SDL_DestroyWindow(context->Window);
	SDL_GpuDestroyDevice(context->Device);
	return 0;
}

Example ClearScreen_Example = { "ClearScreen", Init, Update, Draw, Quit };