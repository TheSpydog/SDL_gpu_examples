#include "Common.h"

#define SDL_GPU_SHADERCROSS_IMPLEMENTATION
#include <SDL_gpu_shadercross.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC SDL_malloc
#define STBI_REALLOC SDL_realloc
#define STBI_FREE SDL_free
#define STBI_ONLY_HDR
#include "../stb_image.h"

int CommonInit(Context* context, SDL_WindowFlags windowFlags)
{
	context->Device = SDL_GpuCreateDevice(SDL_ShaderCross_GetShaderFormats(), SDL_TRUE, SDL_FALSE, NULL);
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

	if (!SDL_GpuClaimWindow(context->Device, context->Window))
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

static const char* BasePath = NULL;
void InitializeAssetLoader()
{
	BasePath = SDL_GetBasePath();
}

SDL_GpuShader* LoadShader(
	SDL_GpuDevice* device,
	const char* shaderFilename,
	Uint32 samplerCount,
	Uint32 uniformBufferCount,
	Uint32 storageBufferCount,
	Uint32 storageTextureCount
) {
	// Auto-detect the shader stage from the file name for convenience
	SDL_GpuShaderStage stage;
	if (SDL_strstr(shaderFilename, ".vert"))
	{
		stage = SDL_GPU_SHADERSTAGE_VERTEX;
	}
	else if (SDL_strstr(shaderFilename, ".frag"))
	{
		stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
	}
	else
	{
		SDL_Log("Invalid shader stage!");
		return NULL;
	}

	char fullPath[256];
	SDL_snprintf(fullPath, sizeof(fullPath), "%sContent/Shaders/Compiled/%s.spv", BasePath, shaderFilename);

	size_t codeSize;
	void* code = SDL_LoadFile(fullPath, &codeSize);
	if (code == NULL)
	{
		SDL_Log("Failed to load shader from disk! %s", fullPath);
		return NULL;
	}

	SDL_GpuShader* shader;
	SDL_GpuShaderCreateInfo shaderInfo = {
		.code = code,
		.codeSize = codeSize,
		.entryPointName = "main",
		.format = SDL_GPU_SHADERFORMAT_SPIRV,
		.stage = stage,
		.samplerCount = samplerCount,
		.uniformBufferCount = uniformBufferCount,
		.storageBufferCount = storageBufferCount,
		.storageTextureCount = storageTextureCount
	};
	if (SDL_GpuGetDriver(device) == SDL_GPU_DRIVER_VULKAN)
	{
		shader = SDL_GpuCreateShader(device, &shaderInfo);
	}
	else
	{
		shader = SDL_ShaderCross_CompileFromSPIRV(device, &shaderInfo, SDL_FALSE);
	}
	if (shader == NULL)
	{
		SDL_Log("Failed to create shader!");
		SDL_free(code);
		return NULL;
	}

	SDL_free(code);
	return shader;
}

SDL_GpuComputePipeline* CreateComputePipelineFromShader(
	SDL_GpuDevice* device,
	const char* shaderFilename,
	SDL_GpuComputePipelineCreateInfo *createInfo
) {
	char fullPath[256];
	SDL_snprintf(fullPath, sizeof(fullPath), "%sContent/Shaders/Compiled/%s.spv", BasePath, shaderFilename);

	size_t codeSize;
	void* code = SDL_LoadFile(fullPath, &codeSize);
	if (code == NULL)
	{
		SDL_Log("Failed to load compute shader from disk! %s", fullPath);
		return NULL;
	}

	// Make a copy of the create data, then overwrite the parts we need
	SDL_GpuComputePipelineCreateInfo newCreateInfo = *createInfo;
	newCreateInfo.code = code;
	newCreateInfo.codeSize = codeSize;
	newCreateInfo.entryPointName = "main";
	newCreateInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;

	SDL_GpuComputePipeline* pipeline;
	if (SDL_GpuGetDriver(device) == SDL_GPU_DRIVER_VULKAN)
	{
		pipeline = SDL_GpuCreateComputePipeline(device, &newCreateInfo);
	}
	else
	{
		pipeline = SDL_ShaderCross_CompileFromSPIRV(device, &newCreateInfo, SDL_TRUE);
	}
	if (pipeline == NULL)
	{
		SDL_Log("Failed to create compute pipeline!");
		SDL_free(code);
		return NULL;
	}

	SDL_free(code);
	return pipeline;
}

SDL_Surface* LoadImage(const char* imageFilename, int desiredChannels)
{
	char fullPath[256];
	SDL_Surface *result;
	SDL_PixelFormat format;

	SDL_snprintf(fullPath, sizeof(fullPath), "%sContent/Images/%s", BasePath, imageFilename);

	result = SDL_LoadBMP(fullPath);
	if (result == NULL)
	{
		SDL_Log("Failed to load BMP: %s", SDL_GetError());
		return NULL;
	}

	if (desiredChannels == 4)
	{
		format = SDL_PIXELFORMAT_ABGR8888;
	}
	else
	{
		SDL_assert(!"Unexpected desiredChannels");
		SDL_DestroySurface(result);
		return NULL;
	}
	if (result->format != format)
	{
		SDL_Surface *next = SDL_ConvertSurface(result, format);
		SDL_DestroySurface(result);
		result = next;
	}

	return result;
}

float* LoadHDRImage(const char* imageFilename, int* pWidth, int* pHeight, int* pChannels, int desiredChannels)
{
	char fullPath[256];
	SDL_snprintf(fullPath, sizeof(fullPath), "%sContent/Images/%s", BasePath, imageFilename);
	return stbi_loadf(fullPath, pWidth, pHeight, pChannels, desiredChannels);
}

// Matrix Math

Matrix4x4 Matrix4x4_Multiply(Matrix4x4 matrix1, Matrix4x4 matrix2)
{
	Matrix4x4 result;

	result.m11 = (
		(matrix1.m11 * matrix2.m11) +
		(matrix1.m12 * matrix2.m21) +
		(matrix1.m13 * matrix2.m31) +
		(matrix1.m14 * matrix2.m41)
	);
	result.m12 = (
		(matrix1.m11 * matrix2.m12) +
		(matrix1.m12 * matrix2.m22) +
		(matrix1.m13 * matrix2.m32) +
		(matrix1.m14 * matrix2.m42)
	);
	result.m13 = (
		(matrix1.m11 * matrix2.m13) +
		(matrix1.m12 * matrix2.m23) +
		(matrix1.m13 * matrix2.m33) +
		(matrix1.m14 * matrix2.m43)
	);
	result.m14 = (
		(matrix1.m11 * matrix2.m14) +
		(matrix1.m12 * matrix2.m24) +
		(matrix1.m13 * matrix2.m34) +
		(matrix1.m14 * matrix2.m44)
	);
	result.m21 = (
		(matrix1.m21 * matrix2.m11) +
		(matrix1.m22 * matrix2.m21) +
		(matrix1.m23 * matrix2.m31) +
		(matrix1.m24 * matrix2.m41)
	);
	result.m22 = (
		(matrix1.m21 * matrix2.m12) +
		(matrix1.m22 * matrix2.m22) +
		(matrix1.m23 * matrix2.m32) +
		(matrix1.m24 * matrix2.m42)
	);
	result.m23 = (
		(matrix1.m21 * matrix2.m13) +
		(matrix1.m22 * matrix2.m23) +
		(matrix1.m23 * matrix2.m33) +
		(matrix1.m24 * matrix2.m43)
	);
	result.m24 = (
		(matrix1.m21 * matrix2.m14) +
		(matrix1.m22 * matrix2.m24) +
		(matrix1.m23 * matrix2.m34) +
		(matrix1.m24 * matrix2.m44)
	);
	result.m31 = (
		(matrix1.m31 * matrix2.m11) +
		(matrix1.m32 * matrix2.m21) +
		(matrix1.m33 * matrix2.m31) +
		(matrix1.m34 * matrix2.m41)
	);
	result.m32 = (
		(matrix1.m31 * matrix2.m12) +
		(matrix1.m32 * matrix2.m22) +
		(matrix1.m33 * matrix2.m32) +
		(matrix1.m34 * matrix2.m42)
	);
	result.m33 = (
		(matrix1.m31 * matrix2.m13) +
		(matrix1.m32 * matrix2.m23) +
		(matrix1.m33 * matrix2.m33) +
		(matrix1.m34 * matrix2.m43)
	);
	result.m34 = (
		(matrix1.m31 * matrix2.m14) +
		(matrix1.m32 * matrix2.m24) +
		(matrix1.m33 * matrix2.m34) +
		(matrix1.m34 * matrix2.m44)
	);
	result.m41 = (
		(matrix1.m41 * matrix2.m11) +
		(matrix1.m42 * matrix2.m21) +
		(matrix1.m43 * matrix2.m31) +
		(matrix1.m44 * matrix2.m41)
	);
	result.m42 = (
		(matrix1.m41 * matrix2.m12) +
		(matrix1.m42 * matrix2.m22) +
		(matrix1.m43 * matrix2.m32) +
		(matrix1.m44 * matrix2.m42)
	);
	result.m43 = (
		(matrix1.m41 * matrix2.m13) +
		(matrix1.m42 * matrix2.m23) +
		(matrix1.m43 * matrix2.m33) +
		(matrix1.m44 * matrix2.m43)
	);
	result.m44 = (
		(matrix1.m41 * matrix2.m14) +
		(matrix1.m42 * matrix2.m24) +
		(matrix1.m43 * matrix2.m34) +
		(matrix1.m44 * matrix2.m44)
	);

	return result;
}

Matrix4x4 Matrix4x4_CreateRotationZ(float radians)
{
	return (Matrix4x4) {
		 SDL_cosf(radians), SDL_sinf(radians), 0, 0,
		-SDL_sinf(radians), SDL_cosf(radians), 0, 0,
						 0, 				0, 1, 0,
						 0,					0, 0, 1
	};
}

Matrix4x4 Matrix4x4_CreateTranslation(float x, float y, float z)
{
	return (Matrix4x4) {
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		x, y, z, 1
	};
}

Matrix4x4 Matrix4x4_CreateOrthographicOffCenter(
	float left,
	float right,
	float bottom,
	float top,
	float zNearPlane,
	float zFarPlane
) {
	return (Matrix4x4) {
		2.0f / (right - left), 0, 0, 0,
		0, 2.0f / (top - bottom), 0, 0,
		0, 0, 1.0f / (zNearPlane - zFarPlane), 0,
		(left + right) / (left - right), (top + bottom) / (bottom - top), zNearPlane / (zNearPlane - zFarPlane), 1
	};
}

Matrix4x4 Matrix4x4_CreatePerspectiveFieldOfView(
	float fieldOfView,
	float aspectRatio,
	float nearPlaneDistance,
	float farPlaneDistance
) {
	float num = 1.0f / ((float) SDL_tanf(fieldOfView * 0.5f));
	return (Matrix4x4) {
		num / aspectRatio, 0, 0, 0,
		0, num, 0, 0,
		0, 0, farPlaneDistance / (nearPlaneDistance - farPlaneDistance), -1,
		0, 0, (nearPlaneDistance * farPlaneDistance) / (nearPlaneDistance - farPlaneDistance), 0
	};
}

Matrix4x4 Matrix4x4_CreateLookAt(
	Vector3 cameraPosition,
	Vector3 cameraTarget,
	Vector3 cameraUpVector
) {
	Vector3 targetToPosition = {
		cameraPosition.x - cameraTarget.x,
		cameraPosition.y - cameraTarget.y,
		cameraPosition.z - cameraTarget.z
	};
	Vector3 vectorA = Vector3_Normalize(targetToPosition);
	Vector3 vectorB = Vector3_Normalize(Vector3_Cross(cameraUpVector, vectorA));
	Vector3 vectorC = Vector3_Cross(vectorA, vectorB);

	return (Matrix4x4) {
		vectorB.x, vectorC.x, vectorA.x, 0,
		vectorB.y, vectorC.y, vectorA.y, 0,
		vectorB.z, vectorC.z, vectorA.z, 0,
		-Vector3_Dot(vectorB, cameraPosition), -Vector3_Dot(vectorC, cameraPosition), -Vector3_Dot(vectorA, cameraPosition), 1
	};
}

Vector3 Vector3_Normalize(Vector3 vec)
{
	float magnitude = SDL_sqrtf((vec.x * vec.x) + (vec.y * vec.y) + (vec.z * vec.z));
	return (Vector3) {
		vec.x / magnitude,
		vec.y / magnitude,
		vec.z / magnitude
	};
}

float Vector3_Dot(Vector3 vecA, Vector3 vecB)
{
	return (vecA.x * vecB.x) + (vecA.y * vecB.y) + (vecA.z * vecB.z);
}

Vector3 Vector3_Cross(Vector3 vecA, Vector3 vecB)
{
	return (Vector3) {
		vecA.y * vecB.z - vecB.y * vecA.z,
		-(vecA.x * vecB.z - vecB.x * vecA.z),
		vecA.x * vecB.y - vecB.x * vecA.y
	};
}
