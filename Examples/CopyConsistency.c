#include "Common.h"

static SDL_GPUGraphicsPipeline* Pipeline;
static SDL_GPUBuffer* VertexBuffer;
static SDL_GPUBuffer* LeftVertexBuffer;
static SDL_GPUBuffer* RightVertexBuffer;
static SDL_GPUBuffer* IndexBuffer;
static SDL_GPUTexture* Texture;
static SDL_GPUTexture* LeftTexture;
static SDL_GPUTexture* RightTexture;
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
				.format = SDL_GetGPUSwapchainTextureFormat(context->Device, context->Window),
				.blend_state = {
					.enable_blend = true,
					.alpha_blend_op = SDL_GPU_BLENDOP_ADD,
					.color_blend_op = SDL_GPU_BLENDOP_ADD,
					.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
					.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
					.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
					.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA
				}
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
		.fragment_shader = fragmentShader,
	};

	Pipeline = SDL_CreateGPUGraphicsPipeline(context->Device, &pipelineCreateInfo);
	if (Pipeline == NULL)
	{
		SDL_Log("Failed to create pipeline!");
		return -1;
	}

	SDL_ReleaseGPUShader(context->Device, vertexShader);
	SDL_ReleaseGPUShader(context->Device, fragmentShader);

	// Create the textures
	SDL_GPUTextureCreateInfo textureCreateInfo = {
		.type = SDL_GPU_TEXTURETYPE_2D,
		.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
		.width = 16,
		.height = 16,
		.layer_count_or_depth = 1,
		.num_levels = 1,
		.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER
	};
	LeftTexture = SDL_CreateGPUTexture(context->Device, &textureCreateInfo);
	RightTexture = SDL_CreateGPUTexture(context->Device, &textureCreateInfo);
	Texture = SDL_CreateGPUTexture(context->Device, &textureCreateInfo);

	// Load the texture data
	SDL_Surface *leftImageData = LoadImage("ravioli.bmp", 4);
	if (leftImageData == NULL)
	{
		SDL_Log("Could not load image data!");
		return -1;
	}

	SDL_Surface *rightImageData = LoadImage("ravioli_inverted.bmp", 4);
	if (rightImageData == NULL)
	{
		SDL_Log("Could not load image data!");
		return -1;
	}

	SDL_assert(leftImageData->w == rightImageData->w);
	SDL_assert(leftImageData->h == rightImageData->h);

	// Create the sampler
	Sampler = SDL_CreateGPUSampler(context->Device, &(SDL_GPUSamplerCreateInfo){
		.min_filter = SDL_GPU_FILTER_NEAREST,
		.mag_filter = SDL_GPU_FILTER_NEAREST,
		.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
		.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
	});

	// Create the buffers
	VertexBuffer = SDL_CreateGPUBuffer(
		context->Device,
		&(SDL_GPUBufferCreateInfo) {
			.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
			.size = sizeof(PositionTextureVertex) * 4
		}
	);
	LeftVertexBuffer = SDL_CreateGPUBuffer(
		context->Device,
		&(SDL_GPUBufferCreateInfo) {
			.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
			.size = sizeof(PositionTextureVertex) * 4
		}
	);
	RightVertexBuffer = SDL_CreateGPUBuffer(
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
			.size = sizeof(PositionTextureVertex) * 8 + sizeof(Uint16) * 6
		}
	);

	PositionTextureVertex* transferData = SDL_MapGPUTransferBuffer(
		context->Device,
		bufferTransferBuffer,
		false
	);

	transferData[0] = (PositionTextureVertex){ -1.0f,  1.0f, 0, 0, 0 };
	transferData[1] = (PositionTextureVertex){  0.0f,  1.0f, 0, 1, 0 };
	transferData[2] = (PositionTextureVertex){  0.0f, -1.0f, 0, 1, 1 };
	transferData[3] = (PositionTextureVertex){ -1.0f, -1.0f, 0, 0, 1 };

	transferData[4] = (PositionTextureVertex){  0.0f,  1.0f, 0, 0, 0 };
	transferData[5] = (PositionTextureVertex){  1.0f,  1.0f, 0, 1, 0 };
	transferData[6] = (PositionTextureVertex){  1.0f, -1.0f, 0, 1, 1 };
	transferData[7] = (PositionTextureVertex){  0.0f, -1.0f, 0, 0, 1 };

	Uint16* indexData = (Uint16*) &transferData[8];
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
			.size = leftImageData->w * leftImageData->h * 8
		}
	);

	Uint8* textureTransferPtr = SDL_MapGPUTransferBuffer(
		context->Device,
		textureTransferBuffer,
		false
	);
	SDL_memcpy(textureTransferPtr, leftImageData->pixels, leftImageData->w * leftImageData->h * 4);
	SDL_memcpy(textureTransferPtr + (leftImageData->w * leftImageData->h * 4), rightImageData->pixels, rightImageData->w * rightImageData->h * 4);
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
			.buffer = LeftVertexBuffer,
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
			.buffer = RightVertexBuffer,
			.offset = 0,
			.size = sizeof(PositionTextureVertex) * 4
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
			.texture = LeftTexture,
			.w = leftImageData->w,
			.h = leftImageData->h,
			.d = 1
		},
		false
	);

	SDL_UploadToGPUTexture(
		copyPass,
		&(SDL_GPUTextureTransferInfo) {
			.transfer_buffer = textureTransferBuffer,
			.offset = leftImageData->w * leftImageData->w * 4,
		},
		&(SDL_GPUTextureRegion){
			.texture = RightTexture,
			.w = rightImageData->w,
			.h = rightImageData->h,
			.d = 1
		},
		false
	);

	SDL_DestroySurface(leftImageData);
	SDL_DestroySurface(rightImageData);
	SDL_EndGPUCopyPass(copyPass);
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
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdbuf, context->Window, &swapchainTexture, NULL, NULL)) {
        SDL_Log("WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
        return -1;
    }

	if (swapchainTexture != NULL)
	{
		SDL_GPUCopyPass* copyPass;
		SDL_GPURenderPass* renderPass;
		SDL_GPUColorTargetInfo colorTargetInfo = {
			.texture = swapchainTexture,
			.clear_color = (SDL_FColor){ 0.0f, 0.0f, 0.0f, 1.0f },
			.load_op = SDL_GPU_LOADOP_CLEAR,
			.store_op = SDL_GPU_STOREOP_STORE
		};

		// Copy left-side resources
		copyPass = SDL_BeginGPUCopyPass(cmdbuf);
		SDL_CopyGPUBufferToBuffer(
			copyPass,
			&(SDL_GPUBufferLocation){
				.buffer = LeftVertexBuffer,
			},
			&(SDL_GPUBufferLocation){
				.buffer = VertexBuffer,
			},
			sizeof(PositionTextureVertex) * 4,
			false
		);
		SDL_CopyGPUTextureToTexture(
			copyPass,
			&(SDL_GPUTextureLocation){
				.texture = LeftTexture
			},
			&(SDL_GPUTextureLocation){
				.texture = Texture
			},
			16,
			16,
			1,
			false
		);
		SDL_EndGPUCopyPass(copyPass);

		// Draw the left side
		renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTargetInfo, 1, NULL);
		SDL_BindGPUGraphicsPipeline(renderPass, Pipeline);
		SDL_BindGPUVertexBuffers(renderPass, 0, &(SDL_GPUBufferBinding){ .buffer = VertexBuffer, .offset = 0 }, 1);
		SDL_BindGPUIndexBuffer(renderPass, &(SDL_GPUBufferBinding){ .buffer = IndexBuffer, .offset = 0 }, SDL_GPU_INDEXELEMENTSIZE_16BIT);
		SDL_BindGPUFragmentSamplers(renderPass, 0, &(SDL_GPUTextureSamplerBinding){ .texture = Texture, .sampler = Sampler }, 1);
		SDL_DrawGPUIndexedPrimitives(renderPass, 6, 1, 0, 0, 0);
		SDL_EndGPURenderPass(renderPass);

		// Copy right-side resources
		copyPass = SDL_BeginGPUCopyPass(cmdbuf);
		SDL_CopyGPUBufferToBuffer(
			copyPass,
			&(SDL_GPUBufferLocation){
				.buffer = RightVertexBuffer,
			},
			&(SDL_GPUBufferLocation){
				.buffer = VertexBuffer,
			},
			sizeof(PositionTextureVertex) * 4,
			false
		);
		SDL_CopyGPUTextureToTexture(
			copyPass,
			&(SDL_GPUTextureLocation){
				.texture = RightTexture
			},
			&(SDL_GPUTextureLocation){
				.texture = Texture
			},
			16,
			16,
			1,
			false
		);
		SDL_EndGPUCopyPass(copyPass);

		// Draw the right side
		colorTargetInfo.load_op = SDL_GPU_LOADOP_LOAD;
		renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTargetInfo, 1, NULL);
		SDL_BindGPUGraphicsPipeline(renderPass, Pipeline);
		SDL_BindGPUVertexBuffers(renderPass, 0, &(SDL_GPUBufferBinding){ .buffer = VertexBuffer, .offset = 0 }, 1);
		SDL_BindGPUIndexBuffer(renderPass, &(SDL_GPUBufferBinding){ .buffer = IndexBuffer, .offset = 0 }, SDL_GPU_INDEXELEMENTSIZE_16BIT);
		SDL_BindGPUFragmentSamplers(renderPass, 0, &(SDL_GPUTextureSamplerBinding){ .texture = Texture, .sampler = Sampler }, 1);
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
	SDL_ReleaseGPUBuffer(context->Device, LeftVertexBuffer);
	SDL_ReleaseGPUBuffer(context->Device, RightVertexBuffer);
	SDL_ReleaseGPUBuffer(context->Device, IndexBuffer);
	SDL_ReleaseGPUTexture(context->Device, Texture);
	SDL_ReleaseGPUTexture(context->Device, LeftTexture);
	SDL_ReleaseGPUTexture(context->Device, RightTexture);
	SDL_ReleaseGPUSampler(context->Device, Sampler);

	CommonQuit(context);
}

Example CopyConsistency_Example = { "CopyConsistency", Init, Update, Draw, Quit };
