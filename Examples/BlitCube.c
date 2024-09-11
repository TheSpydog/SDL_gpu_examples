#include "Common.h"

static SDL_GPUGraphicsPipeline* Pipeline;
static SDL_GPUBuffer* VertexBuffer;
static SDL_GPUBuffer* IndexBuffer;
static SDL_GPUTexture* SourceTexture;
static SDL_GPUTexture* DestinationTexture;
static SDL_GPUSampler* Sampler;

static Vector3 CamPos = { 0, 0, 4 };

static int Init(Context* context)
{
	int result = CommonInit(context, 0);
	if (result < 0)
	{
		return result;
	}

	// Create the shaders
	SDL_GPUShader* vertexShader = LoadShader(context->Device, "Skybox.vert", 0, 1, 0, 0);
	if (vertexShader == NULL)
	{
		SDL_Log("Failed to create vertex shader!");
		return -1;
	}

	SDL_GPUShader* fragmentShader = LoadShader(context->Device, "Skybox.frag", 1, 0, 0, 0);
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
			.num_vertex_bindings = 1,
			.vertex_bindings = (SDL_GPUVertexBinding[]){{
				.index = 0,
				.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
				.instance_step_rate = 0,
				.pitch = sizeof(PositionVertex)
			}},
			.num_vertex_attributes = 1,
			.vertex_attributes = (SDL_GPUVertexAttribute[]){{
				.binding_index = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
				.location = 0,
				.offset = 0
			}}
		},
		.multisample_state.sample_mask = 0xFFFF,
		.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
		.vertex_shader = vertexShader,
		.fragment_shader = fragmentShader
	};

	Pipeline = SDL_CreateGPUGraphicsPipeline(context->Device, &pipelineCreateInfo);

	SDL_ReleaseGPUShader(context->Device, vertexShader);
	SDL_ReleaseGPUShader(context->Device, fragmentShader);

	// Create the GPU resources
	VertexBuffer = SDL_CreateGPUBuffer(
		context->Device,
		&(SDL_GPUBufferCreateInfo) {
			.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
			.size = sizeof(PositionVertex) * 24
		}
	);

	IndexBuffer = SDL_CreateGPUBuffer(
		context->Device,
		&(SDL_GPUBufferCreateInfo) {
			.usage = SDL_GPU_BUFFERUSAGE_INDEX,
			.size = sizeof(Uint16) * 36
		}
	);

	SourceTexture = SDL_CreateGPUTexture(context->Device, &(SDL_GPUTextureCreateInfo){
		.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
		.type = SDL_GPU_TEXTURETYPE_CUBE,
		.width = 32,
		.height = 32,
		.layer_count_or_depth = 6,
		.num_levels = 1,
		.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER
	});

	DestinationTexture = SDL_CreateGPUTexture(context->Device, &(SDL_GPUTextureCreateInfo){
		.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
		.type = SDL_GPU_TEXTURETYPE_CUBE,
		.width = 32,
		.height = 32,
		.layer_count_or_depth = 6,
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
			.size = (sizeof(PositionVertex) * 24) + (sizeof(Uint16) * 36)
		}
	);

	PositionVertex* bufferTransferData = SDL_MapGPUTransferBuffer(
		context->Device,
		bufferTransferBuffer,
		SDL_FALSE
	);

	bufferTransferData[0] = (PositionVertex) { -10, -10, -10 };
	bufferTransferData[1] = (PositionVertex) { 10, -10, -10 };
	bufferTransferData[2] = (PositionVertex) { 10, 10, -10 };
	bufferTransferData[3] = (PositionVertex) { -10, 10, -10 };

	bufferTransferData[4] = (PositionVertex) { -10, -10, 10 };
	bufferTransferData[5] = (PositionVertex) { 10, -10, 10 };
	bufferTransferData[6] = (PositionVertex) { 10, 10, 10 };
	bufferTransferData[7] = (PositionVertex) { -10, 10, 10 };

	bufferTransferData[8] = (PositionVertex) { -10, -10, -10 };
	bufferTransferData[9] = (PositionVertex) { -10, 10, -10 };
	bufferTransferData[10] = (PositionVertex) { -10, 10, 10 };
	bufferTransferData[11] = (PositionVertex) { -10, -10, 10 };

	bufferTransferData[12] = (PositionVertex) { 10, -10, -10 };
	bufferTransferData[13] = (PositionVertex) { 10, 10, -10 };
	bufferTransferData[14] = (PositionVertex) { 10, 10, 10 };
	bufferTransferData[15] = (PositionVertex) { 10, -10, 10 };

	bufferTransferData[16] = (PositionVertex) { -10, -10, -10 };
	bufferTransferData[17] = (PositionVertex) { -10, -10, 10 };
	bufferTransferData[18] = (PositionVertex) { 10, -10, 10 };
	bufferTransferData[19] = (PositionVertex) { 10, -10, -10 };

	bufferTransferData[20] = (PositionVertex) { -10, 10, -10 };
	bufferTransferData[21] = (PositionVertex) { -10, 10, 10 };
	bufferTransferData[22] = (PositionVertex) { 10, 10, 10 };
	bufferTransferData[23] = (PositionVertex) { 10, 10, -10 };

	Uint16* indexData = (Uint16*) &bufferTransferData[24];
	Uint16 indices[] = {
		 0,  1,  2,  0,  2,  3,
		 6,  5,  4,  7,  6,  4,
		 8,  9, 10,  8, 10, 11,
		14, 13, 12, 15, 14, 12,
		16, 17, 18, 16, 18, 19,
		22, 21, 20, 23, 22, 20
	};
	SDL_memcpy(indexData, indices, sizeof(indices));

	SDL_UnmapGPUTransferBuffer(context->Device, bufferTransferBuffer);

	// Set up texture data
	const Uint32 bytesPerImage = 32 * 32 * 4;
	SDL_GPUTransferBuffer* textureTransferBuffer = SDL_CreateGPUTransferBuffer(
		context->Device,
		&(SDL_GPUTransferBufferCreateInfo) {
			.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
			.size = bytesPerImage * 6
		}
	);
	Uint8* textureTransferData = SDL_MapGPUTransferBuffer(
		context->Device,
		textureTransferBuffer,
		SDL_FALSE
	);

	const char* imageNames[] = {
		"cube0.bmp", "cube1.bmp", "cube2.bmp",
		"cube3.bmp", "cube4.bmp", "cube5.bmp",
	};

	for (int i = 0; i < SDL_arraysize(imageNames); i += 1) {
		SDL_Surface* imageData = LoadImage(imageNames[i], 4);
		if (imageData == NULL)
		{
			SDL_Log("Could not load image data!");
			return -1;
		}
		SDL_memcpy(textureTransferData + (bytesPerImage * i), imageData->pixels, bytesPerImage);
		SDL_DestroySurface(imageData);
	}
	SDL_UnmapGPUTransferBuffer(context->Device, textureTransferBuffer);

	// Upload the transfer data to the GPU buffers
	SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(context->Device);
	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdbuf);

	SDL_UploadToGPUBuffer(
		copyPass,
		&(SDL_GPUTransferBufferLocation) {
			.transfer_buffer = bufferTransferBuffer,
			.offset = 0
		},
		&(SDL_GPUBufferRegion) {
			.buffer = VertexBuffer,
			.offset = 0,
			.size = sizeof(PositionVertex) * 24
		},
		SDL_FALSE
	);

	SDL_UploadToGPUBuffer(
		copyPass,
		&(SDL_GPUTransferBufferLocation) {
			.transfer_buffer = bufferTransferBuffer,
			.offset = sizeof(PositionVertex) * 24
		},
		&(SDL_GPUBufferRegion) {
			.buffer = IndexBuffer,
			.offset = 0,
			.size = sizeof(Uint16) * 36
		},
		SDL_FALSE
	);

	for (int i = 0; i < 6; i += 1) {
		SDL_UploadToGPUTexture(
			copyPass,
			&(SDL_GPUTextureTransferInfo) {
				.transfer_buffer = textureTransferBuffer,
				.offset = bytesPerImage * i
			},
			&(SDL_GPUTextureRegion) {
				.texture = SourceTexture,
				.layer = i,
				.w = 32,
				.h = 32,
				.d = 1,
			},
			SDL_FALSE
		);
	}

	SDL_EndGPUCopyPass(copyPass);

	// Blit to destination texture.
	// This serves no real purpose other than demonstrating cube->cube blits are possible!
	for (int i = 0; i < 6; i += 1) {
		SDL_BlitGPUTexture(
			cmdbuf,
			&(SDL_GPUBlitInfo){
				.source.texture = SourceTexture,
				.source.layer_or_depth_plane = i,
				.source.w = 32,
				.source.h = 32,
				.destination.texture = DestinationTexture,
				.destination.layer_or_depth_plane = i,
				.destination.w = 32,
				.destination.h = 32,
				.load_op = SDL_GPU_LOADOP_DONT_CARE,
				.filter = SDL_GPU_FILTER_LINEAR
			}
		);
	}

	SDL_ReleaseGPUTransferBuffer(context->Device, bufferTransferBuffer);
	SDL_ReleaseGPUTransferBuffer(context->Device, textureTransferBuffer);

	SDL_SubmitGPUCommandBuffer(cmdbuf);

	// Print the instructions
	SDL_Log("Press Left/Right to view the opposite direction!");

	return 0;
}

static int Update(Context* context)
{
	if (context->LeftPressed || context->RightPressed)
	{
		CamPos.z *= -1;
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
		Matrix4x4 proj = Matrix4x4_CreatePerspectiveFieldOfView(
			75.0f * SDL_PI_F / 180.0f,
			640.0f / 480.0f,
			0.01f,
			100.0f
		);
		Matrix4x4 view = Matrix4x4_CreateLookAt(
			CamPos,
			(Vector3) { 0, 0, 0 },
			(Vector3) { 0, 1, 0 }
		);

		Matrix4x4 viewproj = Matrix4x4_Multiply(view, proj);

		SDL_GPUColorTargetInfo colorTargetInfo = {
			.texture = swapchainTexture,
			.clear_color = (SDL_FColor){ 0.0f, 0.0f, 0.0f, 1.0f },
			.load_op = SDL_GPU_LOADOP_CLEAR,
			.store_op = SDL_GPU_STOREOP_STORE
		};

		SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTargetInfo, 1, NULL);

		SDL_BindGPUGraphicsPipeline(renderPass, Pipeline);
		SDL_BindGPUVertexBuffers(renderPass, 0, &(SDL_GPUBufferBinding){ VertexBuffer, 0 }, 1);
		SDL_BindGPUIndexBuffer(renderPass, &(SDL_GPUBufferBinding){ IndexBuffer, 0 }, SDL_GPU_INDEXELEMENTSIZE_16BIT);
		SDL_BindGPUFragmentSamplers(renderPass, 0, &(SDL_GPUTextureSamplerBinding){ DestinationTexture, Sampler }, 1);
		SDL_PushGPUVertexUniformData(cmdbuf, 0, &viewproj, sizeof(viewproj));
		SDL_DrawGPUIndexedPrimitives(renderPass, 36, 1, 0, 0, 0);

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

	CamPos.z = SDL_fabsf(CamPos.z);

	CommonQuit(context);
}

Example BlitCube_Example = { "BlitCube", Init, Update, Draw, Quit };
