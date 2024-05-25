#include <SDL_gpu_examples.h>

int CommonInit(Context* context, SDL_WindowFlags windowFlags)
{
	context->Device = SDL_GpuCreateDevice(SDL_GPU_BACKEND_ALL, SDL_TRUE);
	if (context->Device == NULL)
	{
		SDL_Log("GpuCreateDevice failed");
		return -1;
	}

	context->Window = SDL_CreateWindow(context->ExampleName, 640, 480, windowFlags);
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

void CommonQuit(Context* context)
{
	SDL_GpuUnclaimWindow(context->Device, context->Window);
	SDL_DestroyWindow(context->Window);
	SDL_GpuDestroyDevice(context->Device);
}

static char* BasePath = NULL;
void* LoadAsset(const char* relativePath, size_t* pFileSize)
{
	if (!BasePath)
	{
		BasePath = SDL_GetBasePath();
	}

	char fullPath[256];
	SDL_snprintf(fullPath, sizeof(fullPath), "%s%s", BasePath, relativePath);

	void* result = SDL_LoadFile(fullPath, pFileSize);
	return result;
}