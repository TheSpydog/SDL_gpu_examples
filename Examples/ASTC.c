#include "Common.h"

static SDL_GPUGraphicsPipeline* Pipeline;
static SDL_GPUBuffer* VertexBuffer;
static SDL_GPUBuffer* IndexBuffer;
static SDL_GPUTexture* Textures[14];
static SDL_GPUSampler* Sampler;

static SDL_GPUTextureFormat TextureFormats[] =
{
	SDL_GPU_TEXTUREFORMAT_ASTC_4x4_UNORM,
	SDL_GPU_TEXTUREFORMAT_ASTC_5x4_UNORM,
	SDL_GPU_TEXTUREFORMAT_ASTC_5x5_UNORM,
	SDL_GPU_TEXTUREFORMAT_ASTC_6x5_UNORM,
	SDL_GPU_TEXTUREFORMAT_ASTC_6x6_UNORM,
	SDL_GPU_TEXTUREFORMAT_ASTC_8x5_UNORM,
	SDL_GPU_TEXTUREFORMAT_ASTC_8x6_UNORM,
	SDL_GPU_TEXTUREFORMAT_ASTC_8x8_UNORM,
	SDL_GPU_TEXTUREFORMAT_ASTC_10x5_UNORM,
	SDL_GPU_TEXTUREFORMAT_ASTC_10x6_UNORM,
	SDL_GPU_TEXTUREFORMAT_ASTC_10x8_UNORM,
	SDL_GPU_TEXTUREFORMAT_ASTC_10x10_UNORM,
	SDL_GPU_TEXTUREFORMAT_ASTC_12x10_UNORM,
	SDL_GPU_TEXTUREFORMAT_ASTC_12x12_UNORM
};

static const char* TextureNames[] =
{
	"4x4.astc",
	"5x4.astc",
	"5x5.astc",
	"6x5.astc",
	"6x6.astc",
	"8x5.astc",
	"8x6.astc",
	"8x8.astc",
	"10x5.astc",
	"10x6.astc",
	"10x8.astc",
	"10x10.astc",
	"12x10.astc",
	"12x12.astc",
};

static bool SupportsASTC;
static int CurrentTextureIndex;

static int Init(Context* context)
{
	int result = CommonInit(context, 0);
	if (result < 0)
	{
		return result;
	}

	SupportsASTC = SDL_GPUTextureSupportsFormat(context->Device, SDL_GPU_TEXTUREFORMAT_ASTC_4x4_UNORM, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_SAMPLER);
	if (!SupportsASTC)
	{
		SDL_Log("ASTC not supported on this device");
		return 0;
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

	// Create the pipeline
	SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {
		.target_info = {
			.num_color_targets = 1,
			.color_target_descriptions = (SDL_GPUColorTargetDescription[]){{
				.format = SDL_GetGPUSwapchainTextureFormat(context->Device, context->Window)
			}},
		},
		.vertex_input_state = (SDL_GPUVertexInputState){
			.num_vertex_buffers = 1,
			.vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]){{
				.slot = 0,
				.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
				.instance_step_rate = 0,
				.pitch = sizeof(PositionTextureVertex)
			}},
			.num_vertex_attributes = 2,
			.vertex_attributes = (SDL_GPUVertexAttribute[]){{
				.buffer_slot = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
				.location = 0,
				.offset = 0
			}, {
				.buffer_slot = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
				.location = 1,
				.offset = sizeof(float) * 3
			}}
		},
		.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
		.vertex_shader = vertexShader,
		.fragment_shader = fragmentShader
	};

	Pipeline = SDL_CreateGPUGraphicsPipeline(context->Device, &pipelineCreateInfo);
	if (Pipeline == NULL)
	{
		SDL_Log("Failed to create pipeline!");
		return -1;
	}

	SDL_ReleaseGPUShader(context->Device, vertexShader);
	SDL_ReleaseGPUShader(context->Device, fragmentShader);

	Sampler = SDL_CreateGPUSampler(context->Device, &(SDL_GPUSamplerCreateInfo){
		.min_filter = SDL_GPU_FILTER_NEAREST,
		.mag_filter = SDL_GPU_FILTER_NEAREST,
		.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
		.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
		.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
		.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
	});

	// Create the buffers
	VertexBuffer = SDL_CreateGPUBuffer(
		context->Device,
		&(SDL_GPUBufferCreateInfo) {
			.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
			.size = sizeof(PositionTextureVertex) * 4
		}
	);

	IndexBuffer = SDL_CreateGPUBuffer(
		context->Device,
		&(SDL_GPUBufferCreateInfo) {
			.usage = SDL_GPU_BUFFERUSAGE_INDEX,
			.size = sizeof(Uint16) * 6
		}
	);

	// Set up buffer data
	SDL_GPUTransferBuffer* bufferTransferBuffer = SDL_CreateGPUTransferBuffer(
		context->Device,
		&(SDL_GPUTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = (sizeof(PositionTextureVertex) * 4) + (sizeof(Uint16) * 6)
		}
	);

	PositionTextureVertex* transferData = SDL_MapGPUTransferBuffer(
		context->Device,
		bufferTransferBuffer,
		false
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

	// Upload the transfer data to the vertex/index buffers
	SDL_GPUCommandBuffer* uploadCmdBuf = SDL_AcquireGPUCommandBuffer(context->Device);
	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);

	SDL_UploadToGPUBuffer(
		copyPass,
		&(SDL_GPUTransferBufferLocation) {
			.transfer_buffer = bufferTransferBuffer,
			.offset = 0
		},
		&(SDL_GPUBufferRegion) {
			.buffer = VertexBuffer,
			.offset = 0,
			.size = sizeof(PositionTextureVertex) * 4
		},
		false
	);

	SDL_UploadToGPUBuffer(
		copyPass,
		&(SDL_GPUTransferBufferLocation) {
			.transfer_buffer = bufferTransferBuffer,
			.offset = sizeof(PositionTextureVertex) * 4
		},
		&(SDL_GPUBufferRegion) {
			.buffer = IndexBuffer,
			.offset = 0,
			.size = sizeof(Uint16) * 6
		},
		false
	);

	SDL_ReleaseGPUTransferBuffer(context->Device, bufferTransferBuffer);

	// Upload texture data
	for (int i = 0; i < SDL_arraysize(Textures); i += 1)
	{
		// Load the image
		int imageWidth, imageHeight, imageDataLength;
		void* imageData = LoadASTCImage(TextureNames[i], &imageWidth, &imageHeight, &imageDataLength);
		if (imageData == NULL)
		{
			SDL_Log("Failed to load image data! %s", TextureNames[i]);
			return 1;
		}

		// Create the texture
		Textures[i] = SDL_CreateGPUTexture(
			context->Device,
			&(SDL_GPUTextureCreateInfo) {
				.format = TextureFormats[i],
				.width = imageWidth,
				.height = imageHeight,
				.layer_count_or_depth = 1,
				.type = SDL_GPU_TEXTURETYPE_2D,
				.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
				.num_levels = 1,
			}
		);

		// Set up texture transfer data
		SDL_GPUTransferBuffer* textureTransferBuffer = SDL_CreateGPUTransferBuffer(
			context->Device,
			&(SDL_GPUTransferBufferCreateInfo) {
				.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
				.size = imageDataLength
			}
		);

		Uint8* textureTransferPtr = SDL_MapGPUTransferBuffer(
			context->Device,
			textureTransferBuffer,
			false
		);
		SDL_memcpy(textureTransferPtr, imageData, imageDataLength);
		SDL_UnmapGPUTransferBuffer(context->Device, textureTransferBuffer);

		SDL_UploadToGPUTexture(
			copyPass,
			&(SDL_GPUTextureTransferInfo) {
				.transfer_buffer = textureTransferBuffer,
				.offset = 0, /* Zeros out the rest */
			},
			&(SDL_GPUTextureRegion){
				.texture = Textures[i],
				.w = imageWidth,
				.h = imageHeight,
				.d = 1
			},
			false
		);

		SDL_ReleaseGPUTransferBuffer(context->Device, textureTransferBuffer);
	}

	SDL_EndGPUCopyPass(copyPass);
	SDL_SubmitGPUCommandBuffer(uploadCmdBuf);

	// Finally, print instructions!
	SDL_Log("Press Left/Right to switch between textures");

	return 0;
}

static int Update(Context* context)
{
	if (!SupportsASTC)
	{
		return 0;
	}

	if (context->LeftPressed)
	{
		CurrentTextureIndex -= 1;
		if (CurrentTextureIndex < 0)
		{
			CurrentTextureIndex = SDL_arraysize(Textures) - 1;
		}
		SDL_Log("Setting texture to: %s", TextureNames[CurrentTextureIndex]);
	}

	if (context->RightPressed)
	{
		CurrentTextureIndex = (CurrentTextureIndex + 1) % SDL_arraysize(Textures);
		SDL_Log("Setting texture to: %s", TextureNames[CurrentTextureIndex]);
	}

	return 0;
}

static int Draw(Context* context)
{
    SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(context->Device);
    if (cmdbuf == NULL)
    {
        SDL_Log("AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return -1;
    }

    SDL_GPUTexture* swapchainTexture;
    if (!SDL_AcquireGPUSwapchainTexture(cmdbuf, context->Window, &swapchainTexture, NULL, NULL)) {
        SDL_Log("AcquireGPUSwapchainTexture failed: %s", SDL_GetError());
        return -1;
    }

	if (swapchainTexture != NULL)
	{
		SDL_GPUColorTargetInfo colorTargetInfo = { 0 };
		colorTargetInfo.texture = swapchainTexture;
		colorTargetInfo.clear_color = (SDL_FColor){ 0.0f, 0.0f, 0.0f, 1.0f };
		colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
		colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

		SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTargetInfo, 1, NULL);

		if (SupportsASTC)
		{
			SDL_BindGPUGraphicsPipeline(renderPass, Pipeline);
			SDL_BindGPUVertexBuffers(renderPass, 0, &(SDL_GPUBufferBinding){ VertexBuffer, 0 }, 1);
			SDL_BindGPUIndexBuffer(renderPass, &(SDL_GPUBufferBinding){ IndexBuffer, 0 }, SDL_GPU_INDEXELEMENTSIZE_16BIT);
			SDL_BindGPUFragmentSamplers(renderPass, 0, &(SDL_GPUTextureSamplerBinding){ Textures[CurrentTextureIndex], Sampler }, 1);
			SDL_DrawGPUIndexedPrimitives(renderPass, 6, 1, 0, 0, 0);
		}

		SDL_EndGPURenderPass(renderPass);
	}

	SDL_SubmitGPUCommandBuffer(cmdbuf);

	return 0;
}

static void Quit(Context* context)
{
	if (SupportsASTC)
	{
		SDL_ReleaseGPUGraphicsPipeline(context->Device, Pipeline);
		SDL_ReleaseGPUBuffer(context->Device, VertexBuffer);
		SDL_ReleaseGPUBuffer(context->Device, IndexBuffer);
		SDL_ReleaseGPUSampler(context->Device, Sampler);

		for (int i = 0; i < SDL_arraysize(Textures); i += 1)
		{
			SDL_ReleaseGPUTexture(context->Device, Textures[i]);
		}
	}

	CurrentTextureIndex = 0;
	SupportsASTC = false;

	CommonQuit(context);
}

Example ASTC_Example = { "ASTC", Init, Update, Draw, Quit };
