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
	float DeltaTime;
} Context;

int CommonInit(Context* context, SDL_WindowFlags windowFlags);
void CommonQuit(Context* context);

void InitializeAssetLoader();
void* LoadImage(const char* imageFilename, int* pWidth, int* pHeight, int* pChannels, int desiredChannels, SDL_bool hdr);

SDL_GpuShader* LoadShader(
	SDL_GpuDevice* device,
	const char* shaderFilename,
	Uint32 samplerCount,
	Uint32 uniformBufferCount,
	Uint32 storageBufferCount,
	Uint32 storageTextureCount
);
SDL_GpuComputePipeline* CreateComputePipelineFromShader(
	SDL_GpuDevice* device,
	const char* shaderFilename,
	SDL_GpuComputePipelineCreateInfo* createInfo
);

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

// Matrix Math
typedef struct Matrix4x4
{
	float m11, m12, m13, m14;
	float m21, m22, m23, m24;
	float m31, m32, m33, m34;
	float m41, m42, m43, m44;
} Matrix4x4;

Matrix4x4 Matrix4x4_Multiply(Matrix4x4 matrix1, Matrix4x4 matrix2);
Matrix4x4 Matrix4x4_CreateRotationZ(float radians);
Matrix4x4 Matrix4x4_CreateTranslation(float x, float y, float z);
Matrix4x4 Matrix4x4_CreateOrthographicOffCenter(float left, float right, float bottom, float top, float zNearPlane, float zFarPlane);

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
extern Example TexturedAnimatedQuad_Example;
extern Example BasicCompute_Example;
extern Example ComputeUniforms_Example;
extern Example ToneMapping_Example;
extern Example CustomSampling_Example;
extern Example DrawIndirect_Example;
extern Example ComputeSpriteBatch_Example;
extern Example CopyAndReadback_Example;
extern Example CopyConsistency_Example;

#endif
