#include "Common.h"

// This is just a basic upload stride test

static SDL_GPUGraphicsPipeline* Pipeline;
static SDL_GPUTexture* Texture;
static SDL_GPUSampler* Sampler;

Uint32 ImageWidth, ImageHeight;

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

    // Load the image.
    SDL_Surface *imageData = LoadImage("cube0.bmp", 4);
    if (imageData == NULL)
    {
        SDL_Log("Could not load image data!");
        return -1;
    }
    ImageWidth = imageData->w;
    ImageHeight = imageData->h;

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

    // PointClamp
	Sampler = SDL_CreateGPUSampler(context->Device, &(SDL_GPUSamplerCreateInfo){
		.min_filter = SDL_GPU_FILTER_NEAREST,
		.mag_filter = SDL_GPU_FILTER_NEAREST,
		.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
		.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
	});

    // Texture setup
    Uint32 layerCount = 4;
    Texture = SDL_CreateGPUTexture(
        context->Device,
        &(SDL_GPUTextureCreateInfo){
            .type = SDL_GPU_TEXTURETYPE_3D,
            .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
            .width = imageData->w,
            .height = imageData->h,
            .num_levels = 1,
            .layer_count_or_depth = layerCount,
            .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER
        }
    );

    // Crazy padding numbers.
    Uint32 rowPadding = 18;
    Uint32 layerPadding = 28;

    Uint32 layerSize = 4 * (imageData->w + rowPadding) * (imageData->h + layerPadding);
    Uint32 transferBufferSize = layerCount * layerSize;

    SDL_GPUTransferBuffer *transferBuffer = SDL_CreateGPUTransferBuffer(
        context->Device,
        &(SDL_GPUTransferBufferCreateInfo) {
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .size = transferBufferSize
        }
    );

    Uint8 *mapPointer = SDL_MapGPUTransferBuffer(context->Device, transferBuffer, false);
    SDL_memset(mapPointer, 0, transferBufferSize);

    SDL_LockSurface(imageData);
    Uint8 *pixels = imageData->pixels;

    // Pad the memory.
    for (Uint32 layer = 0; layer < layerCount; layer += 1)
    {
        for (Sint32 row = 0; row < imageData->h; row += 1)
        {
            SDL_memcpy(
                mapPointer + (layer * layerSize) + 4 * (imageData->w + rowPadding) * row,
                &pixels[4 * imageData->w * row],
                4 * imageData->w);
        }
    }

    SDL_UnlockSurface(imageData);
    SDL_UnmapGPUTransferBuffer(context->Device, transferBuffer);

    // Upload
    SDL_GPUCommandBuffer *commandBuffer = SDL_AcquireGPUCommandBuffer(context->Device);
    SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(commandBuffer);

    SDL_UploadToGPUTexture(
        copyPass,
        &(SDL_GPUTextureTransferInfo){
            .transfer_buffer = transferBuffer,
            .offset = 0,
            .pixels_per_row = imageData->w + rowPadding,
            .rows_per_layer = imageData->h + layerPadding
        },
        &(SDL_GPUTextureRegion){
            .texture = Texture,
            .w = imageData->w,
            .h = imageData->h,
            .d = 4,
        },
        false
    );

    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(commandBuffer);

    SDL_ReleaseGPUTransferBuffer(context->Device, transferBuffer);
    SDL_DestroySurface(imageData);

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
    Uint32 swapchainWidth, swapchainHeight;
	if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdbuf, context->Window, &swapchainTexture, &swapchainWidth, &swapchainHeight)) {
		SDL_Log("WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
		return -1;
	}

	if (swapchainTexture != NULL)
	{
        SDL_BlitGPUTexture(
            cmdbuf,
            &(SDL_GPUBlitInfo){
                .clear_color = { 0 },
                .source.texture = Texture,
                .source.w = ImageWidth,
                .source.h = ImageHeight,
                .source.layer_or_depth_plane = 0,
                .destination.texture = swapchainTexture,
                .destination.x = 0,
                .destination.y = 0,
                .destination.w = swapchainWidth / 2,
                .destination.h = swapchainHeight / 2,
                .load_op = SDL_GPU_LOADOP_DONT_CARE,
                .filter = SDL_GPU_FILTER_NEAREST
            }
        );

        SDL_BlitGPUTexture(
            cmdbuf,
            &(SDL_GPUBlitInfo){
                .clear_color = { 0 },
                .source.texture = Texture,
                .source.w = ImageWidth,
                .source.h = ImageHeight,
                .source.layer_or_depth_plane = 1,
                .destination.texture = swapchainTexture,
                .destination.x = swapchainWidth / 2,
                .destination.y = 0,
                .destination.w = swapchainWidth / 2,
                .destination.h = swapchainHeight / 2,
                .load_op = SDL_GPU_LOADOP_LOAD,
                .filter = SDL_GPU_FILTER_NEAREST
            }
        );

        SDL_BlitGPUTexture(
            cmdbuf,
            &(SDL_GPUBlitInfo){
                .clear_color = { 0 },
                .source.texture = Texture,
                .source.w = ImageWidth,
                .source.h = ImageHeight,
                .source.layer_or_depth_plane = 2,
                .destination.texture = swapchainTexture,
                .destination.x = 0,
                .destination.y = swapchainHeight / 2,
                .destination.w = swapchainWidth / 2,
                .destination.h = swapchainHeight / 2,
                .load_op = SDL_GPU_LOADOP_LOAD,
                .filter = SDL_GPU_FILTER_NEAREST
            }
        );

        SDL_BlitGPUTexture(
            cmdbuf,
            &(SDL_GPUBlitInfo){
                .clear_color = { 0 },
                .source.texture = Texture,
                .source.w = ImageWidth,
                .source.h = ImageHeight,
                .source.layer_or_depth_plane = 3,
                .destination.texture = swapchainTexture,
                .destination.x = swapchainWidth / 2,
                .destination.y = swapchainHeight / 2,
                .destination.w = swapchainWidth / 2,
                .destination.h = swapchainHeight / 2,
                .load_op = SDL_GPU_LOADOP_LOAD,
                .filter = SDL_GPU_FILTER_NEAREST
            }
        );
	}

	SDL_SubmitGPUCommandBuffer(cmdbuf);

	return 0;
}

static void Quit(Context* context)
{
    SDL_ReleaseGPUGraphicsPipeline(context->Device, Pipeline);
    SDL_ReleaseGPUTexture(context->Device, Texture);
    SDL_ReleaseGPUSampler(context->Device, Sampler);

    CommonQuit(context);
}

Example UploadStride_Example = { "UploadStride", Init, Update, Draw, Quit };
