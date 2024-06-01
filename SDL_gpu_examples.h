#ifndef SDL_GPU_EXAMPLES_H
#define SDL_GPU_EXAMPLES_H

#include <SDL3/SDL.h>

typedef struct Context
{
	const char* ExampleName;
	const char* BasePath;
	SDL_Window* Window;
	SDL_GpuDevice* Device;
	SDL_bool LeftPressed;
	SDL_bool RightPressed;
	SDL_bool DownPressed;
	SDL_bool UpPressed;
} Context;

int CommonInit(Context* context, SDL_WindowFlags windowFlags);
void CommonQuit(Context* context);
void InitializeAssetLoader();
SDL_GpuShader* LoadShader(SDL_GpuDevice* device, const char* shaderFilename);
void* LoadImage(const char* imageFilename, int* pWidth, int* pHeight, int* pChannels, int desiredChannels, SDL_bool hdr);

// Vertex Formats
typedef struct PositionColorVertex
{
	float x, y, z;
	Uint8 r, g, b, a;
} PositionColorVertex;

typedef struct PositionTextureVertex
{
    float x, y, z;
    float u, v;
} PositionTextureVertex;

// Examples
typedef struct Example
{
	const char* Name;
	int (*Init)(Context* context);
	int (*Update)(Context* context);
	int (*Draw)(Context* context);
	void (*Quit)(Context* context);
} Example;

extern Example ClearScreen_Example;
extern Example ClearScreenMultiWindow_Example;
extern Example BasicTriangle_Example;
extern Example BasicVertexBuffer_Example;
extern Example CullMode_Example;
extern Example BasicStencil_Example;
extern Example InstancedIndexed_Example;
extern Example TexturedQuad_Example;
extern Example BasicCompute_Example;
extern Example ComputeUniforms_Example;
extern Example ToneMapping_Example;

#endif
