#include "Common.h"

static const char* SamplerNames[] =
{
	"PointClamp",
	"PointWrap",
	"LinearClamp",
	"LinearWrap",
	"AnisotropicClamp",
	"AnisotropicWrap",
};

static SDL_GPUGraphicsPipeline* Pipeline;
static SDL_GPUBuffer* VertexBuffer;
static SDL_GPUBuffer* IndexBuffer;
static SDL_GPUTexture* Texture;
static SDL_GPUSampler* Samplers[SDL_arraysize(SamplerNames)];

static int CurrentSamplerIndex;

static int Init(Context* context)
{
	int result = CommonInit(context, 0);
	if (result < 0)
	{
		return result;
	}

	// Create the shaders
	SDL_GPUShader* vertexShader = LoadShader(context->Device, "TexturedQuad.vert", 0, 0, 0, 0);
	if (vertexShader == NULL)
	{
		SDL_Log("Failed to create vertex shader!");
		return -1;
	}

	SDL_GPUShader* fragmentShader = LoadShader(context->Device, "TexturedQuad.frag", 1, 0, 0, 0);
	if (fragmentShader == NULL)
	{
		SDL_Log("Failed to create fragment shader!");
		return -1;
	}

	// Load the image
	SDL_Surface *imageData = LoadImage("ravioli.bmp", 4);
	if (imageData == NULL)
	{
		SDL_Log("Could not load image data!");
		return -1;
	}

	// Create the pipeline
	SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {
		.attachmentInfo = {
			.colorAttachmentCount = 1,
			.colorAttachmentDescriptions = (SDL_GPUColorAttachmentDescription[]){{
				.format = SDL_GetGPUSwapchainTextureFormat(context->Device, context->Window),
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
		.vertexInputState = (SDL_GPUVertexInputState){
			.vertexBindingCount = 1,
			.vertexBindings = (SDL_GPUVertexBinding[]){{
				.binding = 0,
				.inputRate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
				.instanceStepRate = 0,
				.stride = sizeof(PositionTextureVertex)
			}},
			.vertexAttributeCount = 2,
			.vertexAttributes = (SDL_GPUVertexAttribute[]){{
				.binding = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
				.location = 0,
				.offset = 0
			}, {
				.binding = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
				.location = 1,
				.offset = sizeof(float) * 3
			}}
		},
		.multisampleState.sampleMask = 0xFFFF,
		.primitiveType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
		.vertexShader = vertexShader,
		.fragmentShader = fragmentShader
	};

	Pipeline = SDL_CreateGPUGraphicsPipeline(context->Device, &pipelineCreateInfo);
	if (Pipeline == NULL)
	{
		SDL_Log("Failed to create pipeline!");
		return -1;
	}

	SDL_ReleaseGPUShader(context->Device, vertexShader);
	SDL_ReleaseGPUShader(context->Device, fragmentShader);

	// PointClamp
	Samplers[0] = SDL_CreateGPUSampler(context->Device, &(SDL_GPUSamplerCreateInfo){
		.minFilter = SDL_GPU_FILTER_NEAREST,
		.magFilter = SDL_GPU_FILTER_NEAREST,
		.mipmapMode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
		.addressModeU = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		.addressModeV = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		.addressModeW = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
	});
	// PointWrap
	Samplers[1] = SDL_CreateGPUSampler(context->Device, &(SDL_GPUSamplerCreateInfo){
		.minFilter = SDL_GPU_FILTER_NEAREST,
		.magFilter = SDL_GPU_FILTER_NEAREST,
		.mipmapMode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
		.addressModeU = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
		.addressModeV = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
		.addressModeW = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
	});
	// LinearClamp
	Samplers[2] = SDL_CreateGPUSampler(context->Device, &(SDL_GPUSamplerCreateInfo){
		.minFilter = SDL_GPU_FILTER_LINEAR,
		.magFilter = SDL_GPU_FILTER_LINEAR,
		.mipmapMode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
		.addressModeU = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		.addressModeV = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		.addressModeW = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
	});
	// LinearWrap
	Samplers[3] = SDL_CreateGPUSampler(context->Device, &(SDL_GPUSamplerCreateInfo){
		.minFilter = SDL_GPU_FILTER_LINEAR,
		.magFilter = SDL_GPU_FILTER_LINEAR,
		.mipmapMode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
		.addressModeU = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
		.addressModeV = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
		.addressModeW = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
	});
	// AnisotropicClamp
	Samplers[4] = SDL_CreateGPUSampler(context->Device, &(SDL_GPUSamplerCreateInfo){
		.minFilter = SDL_GPU_FILTER_LINEAR,
		.magFilter = SDL_GPU_FILTER_LINEAR,
		.mipmapMode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
		.addressModeU = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		.addressModeV = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		.addressModeW = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		.anisotropyEnable = SDL_TRUE,
		.maxAnisotropy = 4
	});
	// AnisotropicWrap
	Samplers[5] = SDL_CreateGPUSampler(context->Device, &(SDL_GPUSamplerCreateInfo){
		.minFilter = SDL_GPU_FILTER_LINEAR,
		.magFilter = SDL_GPU_FILTER_LINEAR,
		.mipmapMode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
		.addressModeU = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
		.addressModeV = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
		.addressModeW = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
		.anisotropyEnable = SDL_TRUE,
		.maxAnisotropy = 4
	});

	// Create the GPU resources
	VertexBuffer = SDL_CreateGPUBuffer(
		context->Device,
		&(SDL_GPUBufferCreateInfo) {
			.usageFlags = SDL_GPU_BUFFERUSAGE_VERTEX,
			.sizeInBytes = sizeof(PositionTextureVertex) * 4
		}
	);
	SDL_SetGPUBufferName(
		context->Device,
		VertexBuffer,
		"Ravioli Vertex Buffer 🥣"
	);

	IndexBuffer = SDL_CreateGPUBuffer(
		context->Device,
		&(SDL_GPUBufferCreateInfo) {
			.usageFlags = SDL_GPU_BUFFERUSAGE_INDEX,
			.sizeInBytes = sizeof(Uint16) * 6
		}
	);

	Texture = SDL_CreateGPUTexture(context->Device, &(SDL_GPUTextureCreateInfo){
		.type = SDL_GPU_TEXTURETYPE_2D,
		.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
		.width = imageData->w,
		.height = imageData->h,
		.layerCountOrDepth = 1,
		.levelCount = 1,
		.usageFlags = SDL_GPU_TEXTUREUSAGE_SAMPLER
	});
	SDL_SetGPUTextureName(
		context->Device,
		Texture,
		"Ravioli Texture 🖼️"
	);

	// Set up buffer data
	SDL_GPUTransferBuffer* bufferTransferBuffer = SDL_CreateGPUTransferBuffer(
		context->Device,
		&(SDL_GPUTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.sizeInBytes = (sizeof(PositionTextureVertex) * 4) + (sizeof(Uint16) * 6)
		}
	);

	PositionTextureVertex* transferData = SDL_MapGPUTransferBuffer(
		context->Device,
		bufferTransferBuffer,
		SDL_FALSE
	);

	transferData[0] = (PositionTextureVertex) { -1,  1, 0, 0, 0 };
	transferData[1] = (PositionTextureVertex) {  1,  1, 0, 4, 0 };
	transferData[2] = (PositionTextureVertex) {  1, -1, 0, 4, 4 };
	transferData[3] = (PositionTextureVertex) { -1, -1, 0, 0, 4 };

	Uint16* indexData = (Uint16*) &transferData[4];
	indexData[0] = 0;
	indexData[1] = 1;
	indexData[2] = 2;
	indexData[3] = 0;
	indexData[4] = 2;
	indexData[5] = 3;

	SDL_UnmapGPUTransferBuffer(context->Device, bufferTransferBuffer);

	// Set up texture data
	SDL_GPUTransferBuffer* textureTransferBuffer = SDL_CreateGPUTransferBuffer(
		context->Device,
		&(SDL_GPUTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.sizeInBytes = imageData->w * imageData->h * 4
		}
	);

	Uint8* textureTransferPtr = SDL_MapGPUTransferBuffer(
		context->Device,
		textureTransferBuffer,
		SDL_FALSE
	);
	SDL_memcpy(textureTransferPtr, imageData->pixels, imageData->w * imageData->h * 4);
	SDL_UnmapGPUTransferBuffer(context->Device, textureTransferBuffer);

	// Upload the transfer data to the GPU resources
	SDL_GPUCommandBuffer* uploadCmdBuf = SDL_AcquireGPUCommandBuffer(context->Device);
	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);

	SDL_UploadToGPUBuffer(
		copyPass,
		&(SDL_GPUTransferBufferLocation) {
			.transferBuffer = bufferTransferBuffer,
			.offset = 0
		},
		&(SDL_GPUBufferRegion) {
			.buffer = VertexBuffer,
			.offset = 0,
			.size = sizeof(PositionTextureVertex) * 4
		},
		SDL_FALSE
	);

	SDL_UploadToGPUBuffer(
		copyPass,
		&(SDL_GPUTransferBufferLocation) {
			.transferBuffer = bufferTransferBuffer,
			.offset = sizeof(PositionTextureVertex) * 4
		},
		&(SDL_GPUBufferRegion) {
			.buffer = IndexBuffer,
			.offset = 0,
			.size = sizeof(Uint16) * 6
		},
		SDL_FALSE
	);

	SDL_UploadToGPUTexture(
		copyPass,
		&(SDL_GPUTextureTransferInfo) {
			.transferBuffer = textureTransferBuffer,
			.offset = 0, /* Zeros out the rest */
		},
		&(SDL_GPUTextureRegion){
			.texture = Texture,
			.w = imageData->w,
			.h = imageData->h,
			.d = 1
		},
		SDL_FALSE
	);

	SDL_DestroySurface(imageData);
	SDL_EndGPUCopyPass(copyPass);
	SDL_SubmitGPUCommandBuffer(uploadCmdBuf);
	SDL_ReleaseGPUTransferBuffer(context->Device, bufferTransferBuffer);
	SDL_ReleaseGPUTransferBuffer(context->Device, textureTransferBuffer);

	// Finally, print instructions!
	SDL_Log("Press Left/Right to switch between sampler states");
	SDL_Log("Setting sampler state to: %s", SamplerNames[0]);

	return 0;
}

static int Update(Context* context)
{
	if (context->LeftPressed)
	{
		CurrentSamplerIndex -= 1;
		if (CurrentSamplerIndex < 0)
		{
			CurrentSamplerIndex = SDL_arraysize(Samplers) - 1;
		}
		SDL_Log("Setting sampler state to: %s", SamplerNames[CurrentSamplerIndex]);
	}

	if (context->RightPressed)
	{
		CurrentSamplerIndex = (CurrentSamplerIndex + 1) % SDL_arraysize(Samplers);
		SDL_Log("Setting sampler state to: %s", SamplerNames[CurrentSamplerIndex]);
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
		SDL_GPUColorAttachmentInfo colorAttachmentInfo = { 0 };
		colorAttachmentInfo.texture = swapchainTexture;
		colorAttachmentInfo.clearColor = (SDL_FColor){ 0.0f, 0.0f, 0.0f, 1.0f };
		colorAttachmentInfo.loadOp = SDL_GPU_LOADOP_CLEAR;
		colorAttachmentInfo.storeOp = SDL_GPU_STOREOP_STORE;

		SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorAttachmentInfo, 1, NULL);

		SDL_BindGPUGraphicsPipeline(renderPass, Pipeline);
		SDL_BindGPUVertexBuffers(renderPass, 0, &(SDL_GPUBufferBinding){ .buffer = VertexBuffer, .offset = 0 }, 1);
		SDL_BindGPUIndexBuffer(renderPass, &(SDL_GPUBufferBinding){ .buffer = IndexBuffer, .offset = 0 }, SDL_GPU_INDEXELEMENTSIZE_16BIT);
		SDL_BindGPUFragmentSamplers(renderPass, 0, &(SDL_GPUTextureSamplerBinding){ .texture = Texture, .sampler = Samplers[CurrentSamplerIndex] }, 1);
		SDL_DrawGPUIndexedPrimitives(renderPass, 6, 1, 0, 0, 0);

		SDL_EndGPURenderPass(renderPass);
	}

	SDL_SubmitGPUCommandBuffer(cmdbuf);

	return 0;
}

static void Quit(Context* context)
{
	SDL_ReleaseGPUGraphicsPipeline(context->Device, Pipeline);
	SDL_ReleaseGPUBuffer(context->Device, VertexBuffer);
	SDL_ReleaseGPUBuffer(context->Device, IndexBuffer);
	SDL_ReleaseGPUTexture(context->Device, Texture);

	for (int i = 0; i < SDL_arraysize(Samplers); i += 1)
	{
		SDL_ReleaseGPUSampler(context->Device, Samplers[i]);
	}

	CurrentSamplerIndex = 0;

	CommonQuit(context);
}

Example TexturedQuad_Example = { "TexturedQuad", Init, Update, Draw, Quit };
