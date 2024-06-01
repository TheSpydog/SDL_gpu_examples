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

SDL_GpuShader* LoadShader(SDL_GpuDevice* device, const char* shaderFilename)
{
	// Auto-detect the shader type from the file name for convenience
	SDL_GpuShaderStageFlagBits stage;
	if (SDL_strstr(shaderFilename, ".vert"))
	{
		stage = SDL_GPU_SHADERSTAGE_VERTEX;
	}
	else if (SDL_strstr(shaderFilename, ".frag"))
	{
		stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
	}
	else if (SDL_strstr(shaderFilename, ".comp"))
	{
		stage = SDL_GPU_SHADERSTAGE_COMPUTE;
	}
	else
	{
		SDL_Log("Invalid shader stage!");
		return NULL;
	}


	char fullPath[256];
	SDL_snprintf(fullPath, sizeof(fullPath), "%sContent/Shaders/Compiled/%s", BasePath, shaderFilename);

	size_t codeSize;
	void* code = SDL_LoadFile(fullPath, &codeSize);
	if (code == NULL)
	{
		SDL_Log("Failed to load shader from disk! %s", fullPath);
		return NULL;
	}

	SDL_GpuShader* shader = SDL_GpuCreateShader(device, &(SDL_GpuShaderCreateInfo){
		.stage = stage,
		.code = code,
		.codeSize = codeSize,
		.entryPointName = "main",
		.format = SDL_GPU_SHADERFORMAT_SPIRV
	});
	if (shader == NULL)
	{
		SDL_Log("Failed to create shader!");
		SDL_free(code);
		return NULL;
	}

	SDL_free(code);
	return shader;
}

void* LoadImage(const char* imageFilename, int* pWidth, int* pHeight, int* pChannels, int desiredChannels, SDL_bool hdr)
{
	char fullPath[256];
	SDL_snprintf(fullPath, sizeof(fullPath), "%sContent/Images/%s", BasePath, imageFilename);
	return hdr ?
		stbi_loadf(fullPath, pWidth, pHeight, pChannels, desiredChannels) :
		stbi_load(fullPath, pWidth, pHeight, pChannels, desiredChannels);
}
