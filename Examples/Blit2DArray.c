#include "Common.h"

static SDL_GPUGraphicsPipeline* Pipeline;
static SDL_GPUBuffer* VertexBuffer;
static SDL_GPUBuffer* IndexBuffer;
static SDL_GPUTexture* SourceTexture;
static SDL_GPUTexture* DestinationTexture;
static SDL_GPUSampler* Sampler;

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

	SDL_GPUShader* fragmentShader = LoadShader(context->Device, "TexturedQuadArray.frag", 1, 0, 0, 0);
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

	// Load the images
	SDL_Surface *imageData1 = LoadImage("ravioli.bmp", 4);
	if (imageData1 == NULL)
	{
		SDL_Log("Could not load first image data!");
		return -1;
	}

	SDL_Surface *imageData2 = LoadImage("ravioli_inverted.bmp", 4);
	if (imageData2 == NULL)
	{
		SDL_Log("Could not load second image data!");
		return -1;
	}

	SDL_assert(imageData1->w == imageData2->w);
	SDL_assert(imageData1->h == imageData2->h);

	Uint32 srcWidth = imageData1->w;
	Uint32 srcHeight = imageData1->h;

	// Create the GPU resources
	VertexBuffer = SDL_CreateGPUBuffer(
		context->Device,
		&(SDL_GPUBufferCreateInfo) {
			.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
			.size = sizeof(PositionTextureVertex) * 8
		}
	);

	IndexBuffer = SDL_CreateGPUBuffer(
		context->Device,
		&(SDL_GPUBufferCreateInfo) {
			.usage = SDL_GPU_BUFFERUSAGE_INDEX,
			.size = sizeof(Uint16) * 6
		}
	);

	SourceTexture = SDL_CreateGPUTexture(context->Device, &(SDL_GPUTextureCreateInfo){
		.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
		.type = SDL_GPU_TEXTURETYPE_2D_ARRAY,
		.width = srcWidth,
		.height = srcHeight,
		.layer_count_or_depth = 2,
		.num_levels = 1,
		.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER
	});

	DestinationTexture = SDL_CreateGPUTexture(context->Device, &(SDL_GPUTextureCreateInfo){
		.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
		.type = SDL_GPU_TEXTURETYPE_2D_ARRAY,
		.width = srcWidth / 2,
		.height = srcHeight / 2,
		.layer_count_or_depth = 2,
		.num_levels = 1,
		.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET
	});

	Sampler = SDL_CreateGPUSampler(context->Device, &(SDL_GPUSamplerCreateInfo){
		.min_filter = SDL_GPU_FILTER_NEAREST,
		.mag_filter = SDL_GPU_FILTER_NEAREST,
		.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
		.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
	});

	// Set up buffer data
	SDL_GPUTransferBuffer* bufferTransferBuffer = SDL_CreateGPUTransferBuffer(
		context->Device,
		&(SDL_GPUTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = (sizeof(PositionTextureVertex) * 8) + (sizeof(Uint16) * 6)
		}
	);

	PositionTextureVertex* transferData = SDL_MapGPUTransferBuffer(
		context->Device,
		bufferTransferBuffer,
		false
	);

	transferData[0] = (PositionTextureVertex) { -1,  1, 0, 0, 0 };
	transferData[1] = (PositionTextureVertex) {  0,  1, 0, 1, 0 };
	transferData[2] = (PositionTextureVertex) {  0, -1, 0, 1, 1 };
	transferData[3] = (PositionTextureVertex) { -1, -1, 0, 0, 1 };
	transferData[4] = (PositionTextureVertex) {  0,  1, 0, 0, 0 };
	transferData[5] = (PositionTextureVertex) {  1,  1, 0, 1, 0 };
	transferData[6] = (PositionTextureVertex) {  1, -1, 0, 1, 1 };
	transferData[7] = (PositionTextureVertex) {  0, -1, 0, 0, 1 };

	Uint16* indexData = (Uint16*) &transferData[8];
	indexData[0] = 0;
	indexData[1] = 1;
	indexData[2] = 2;
	indexData[3] = 0;
	indexData[4] = 2;
	indexData[5] = 3;

	SDL_UnmapGPUTransferBuffer(context->Device, bufferTransferBuffer);

	// Set up texture data
	const Uint32 imageSizeInBytes = srcWidth * srcHeight * 4;
	SDL_GPUTransferBuffer* textureTransferBuffer = SDL_CreateGPUTransferBuffer(
		context->Device,
		&(SDL_GPUTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = imageSizeInBytes * 2
		}
	);

	Uint8* textureTransferPtr = SDL_MapGPUTransferBuffer(
		context->Device,
		textureTransferBuffer,
		false
	);
	SDL_memcpy(textureTransferPtr, imageData1->pixels, imageSizeInBytes);
	SDL_memcpy(textureTransferPtr + imageSizeInBytes, imageData2->pixels, imageSizeInBytes);
	SDL_UnmapGPUTransferBuffer(context->Device, textureTransferBuffer);

	// Upload the transfer data to the GPU resources
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
			.size = sizeof(PositionTextureVertex) * 8
		},
		false
	);

	SDL_UploadToGPUBuffer(
		copyPass,
		&(SDL_GPUTransferBufferLocation) {
			.transfer_buffer = bufferTransferBuffer,
			.offset = sizeof(PositionTextureVertex) * 8
		},
		&(SDL_GPUBufferRegion) {
			.buffer = IndexBuffer,
			.offset = 0,
			.size = sizeof(Uint16) * 6
		},
		false
	);

	SDL_UploadToGPUTexture(
		copyPass,
		&(SDL_GPUTextureTransferInfo) {
			.transfer_buffer = textureTransferBuffer,
			.offset = 0,
		},
		&(SDL_GPUTextureRegion){
			.texture = SourceTexture,
			.layer = 0,
			.w = srcWidth,
			.h = srcHeight,
			.d = 1
		},
		false
	);

	SDL_UploadToGPUTexture(
		copyPass,
		&(SDL_GPUTextureTransferInfo) {
			.transfer_buffer = textureTransferBuffer,
			.offset = imageSizeInBytes,
		},
		&(SDL_GPUTextureRegion){
			.texture = SourceTexture,
			.layer = 1,
			.w = srcWidth,
			.h = srcHeight,
			.d = 1
		},
		false
	);

	SDL_DestroySurface(imageData1);
	SDL_DestroySurface(imageData2);
	SDL_EndGPUCopyPass(copyPass);

	SDL_BlitGPUTexture(
		uploadCmdBuf,
		&(SDL_GPUBlitInfo){
			.source.texture = SourceTexture,
			.source.w = srcWidth,
			.source.h = srcHeight,
			.destination.texture = DestinationTexture,
			.destination.w = srcWidth / 2,
			.destination.h = srcHeight / 2,
			.load_op = SDL_GPU_LOADOP_DONT_CARE,
			.filter = SDL_GPU_FILTER_LINEAR
		}
	);
	SDL_BlitGPUTexture(
		uploadCmdBuf,
		&(SDL_GPUBlitInfo){
			.source.texture = SourceTexture,
			.source.layer_or_depth_plane = 1,
			.source.w = srcWidth,
			.source.h = srcHeight,
			.destination.texture = DestinationTexture,
			.destination.layer_or_depth_plane = 1,
			.destination.w = srcWidth / 2,
			.destination.h = srcHeight / 2,
			.load_op = SDL_GPU_LOADOP_LOAD,
			.filter = SDL_GPU_FILTER_LINEAR
		}
	);

	SDL_SubmitGPUCommandBuffer(uploadCmdBuf);
	SDL_ReleaseGPUTransferBuffer(context->Device, bufferTransferBuffer);
	SDL_ReleaseGPUTransferBuffer(context->Device, textureTransferBuffer);

	return 0;
}

static int Update(Context* context)
{
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
    if (!SDL_AcquireGPUSwapchainTexture(cmdbuf, context->Window, &swapchainTexture)) {
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

		SDL_BindGPUGraphicsPipeline(renderPass, Pipeline);
		SDL_BindGPUVertexBuffers(renderPass, 0, &(SDL_GPUBufferBinding){ .buffer = VertexBuffer, .offset = 0 }, 1);
		SDL_BindGPUIndexBuffer(renderPass, &(SDL_GPUBufferBinding){ .buffer = IndexBuffer, .offset = 0 }, SDL_GPU_INDEXELEMENTSIZE_16BIT);
		SDL_BindGPUFragmentSamplers(renderPass, 0, &(SDL_GPUTextureSamplerBinding){ .texture = SourceTexture, .sampler = Sampler }, 1);
		SDL_DrawGPUIndexedPrimitives(renderPass, 6, 1, 0, 0, 0);
		SDL_BindGPUFragmentSamplers(renderPass, 0, &(SDL_GPUTextureSamplerBinding){.texture = DestinationTexture, .sampler = Sampler }, 1);
		SDL_DrawGPUIndexedPrimitives(renderPass, 6, 1, 0, 4, 0);

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
	SDL_ReleaseGPUTexture(context->Device, SourceTexture);
	SDL_ReleaseGPUTexture(context->Device, DestinationTexture);
	SDL_ReleaseGPUSampler(context->Device, Sampler);

	CommonQuit(context);
}

Example Blit2DArray_Example = { "Blit2DArray", Init, Update, Draw, Quit };
