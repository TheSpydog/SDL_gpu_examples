#include <SDL_gpu_examples.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC SDL_malloc
#define STBI_REALLOC SDL_realloc
#define STBI_FREE SDL_free
#include "stb_image.h"

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

	if (!SDL_GpuClaimWindow(context->Device, context->Window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC))
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
void InitializeAssetLoader()
{
	BasePath = SDL_GetBasePath();
}

void* LoadShader(const char* shaderFilename, size_t* pSizeInBytes)
{
	char fullPath[256];
	SDL_snprintf(fullPath, sizeof(fullPath), "%sContent/Shaders/Compiled/%s", BasePath, shaderFilename);
	return SDL_LoadFile(fullPath, pSizeInBytes);
}

void* LoadImage(const char* imageFilename, int* pWidth, int* pHeight, int* pChannels, int desiredChannels)
{
	char fullPath[256];
	SDL_snprintf(fullPath, sizeof(fullPath), "%sContent/Images/%s", BasePath, imageFilename);
	return stbi_loadf(fullPath, pWidth, pHeight, pChannels, desiredChannels);
}
