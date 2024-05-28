#include <SDL_gpu_examples.h>

static SDL_GpuGraphicsPipeline* DrawPipeline;
static SDL_GpuTexture* RenderTexture;
static SDL_GpuTexture* DepthTexture;
static SDL_GpuBuffer* VertexBuffer;

static float matrix_rotate[16], matrix_modelview[16], matrix_perspective[16], matrix_final[16];
static float angle_x, angle_y, angle_z;
static int w, h;

typedef struct PositionColorVertex
{
	float x, y, z;
	float r, g, b, a;
} PositionColorVertex;

static const PositionColorVertex VertexData[] = {
    /* Front face. */
    /* Bottom left */
    { -0.5,  0.5, -0.5, 1.0, 0.0, 0.0, 1.0 }, /* red */
    {  0.5, -0.5, -0.5, 0.0, 0.0, 1.0, 1.0 }, /* blue */
    { -0.5, -0.5, -0.5, 0.0, 1.0, 0.0, 1.0 }, /* green */

    /* Top right */
    { -0.5, 0.5, -0.5, 1.0, 0.0, 0.0, 1.0 }, /* red */
    { 0.5,  0.5, -0.5, 1.0, 1.0, 0.0, 1.0 }, /* yellow */
    { 0.5, -0.5, -0.5, 0.0, 0.0, 1.0, 1.0 }, /* blue */

    /* Left face */
    /* Bottom left */
    { -0.5,  0.5,  0.5, 1.0, 1.0, 1.0, 1.0 }, /* white */
    { -0.5, -0.5, -0.5, 0.0, 1.0, 0.0, 1.0 }, /* green */
    { -0.5, -0.5,  0.5, 0.0, 1.0, 1.0, 1.0 }, /* cyan */

    /* Top right */
    { -0.5,  0.5,  0.5, 1.0, 1.0, 1.0, 1.0 }, /* white */
    { -0.5,  0.5, -0.5, 1.0, 0.0, 0.0, 1.0 }, /* red */
    { -0.5, -0.5, -0.5, 0.0, 1.0, 0.0, 1.0 }, /* green */

    /* Top face */
    /* Bottom left */
    { -0.5, 0.5,  0.5, 1.0, 1.0, 1.0, 1.0 }, /* white */
    {  0.5, 0.5, -0.5, 1.0, 1.0, 0.0, 1.0 }, /* yellow */
    { -0.5, 0.5, -0.5, 1.0, 0.0, 0.0, 1.0 }, /* red */

    /* Top right */
    { -0.5, 0.5,  0.5, 1.0, 1.0, 1.0, 1.0 }, /* white */
    {  0.5, 0.5,  0.5, 0.0, 0.0, 0.0, 1.0 }, /* black */
    {  0.5, 0.5, -0.5, 1.0, 1.0, 0.0, 1.0 }, /* yellow */

    /* Right face */
    /* Bottom left */
    { 0.5,  0.5, -0.5, 1.0, 1.0, 0.0, 1.0 }, /* yellow */
    { 0.5, -0.5,  0.5, 1.0, 0.0, 1.0, 1.0 }, /* magenta */
    { 0.5, -0.5, -0.5, 0.0, 0.0, 1.0, 1.0 }, /* blue */

    /* Top right */
    { 0.5,  0.5, -0.5, 1.0, 1.0, 0.0, 1.0 }, /* yellow */
    { 0.5,  0.5,  0.5, 0.0, 0.0, 0.0, 1.0 }, /* black */
    { 0.5, -0.5,  0.5, 1.0, 0.0, 1.0, 1.0 }, /* magenta */

    /* Back face */
    /* Bottom left */
    {  0.5,  0.5, 0.5, 0.0, 0.0, 0.0, 1.0 }, /* black */
    { -0.5, -0.5, 0.5, 0.0, 1.0, 1.0, 1.0 }, /* cyan */
    {  0.5, -0.5, 0.5, 1.0, 0.0, 1.0, 1.0 }, /* magenta */

    /* Top right */
    {  0.5,  0.5,  0.5, 0.0, 0.0, 0.0, 1.0 }, /* black */
    { -0.5,  0.5,  0.5, 1.0, 1.0, 1.0, 1.0 }, /* white */
    { -0.5, -0.5,  0.5, 0.0, 1.0, 1.0, 1.0 }, /* cyan */

    /* Bottom face */
    /* Bottom left */
    { -0.5, -0.5, -0.5, 0.0, 1.0, 0.0, 1.0 }, /* green */
    {  0.5, -0.5,  0.5, 1.0, 0.0, 1.0, 1.0 }, /* magenta */
    { -0.5, -0.5,  0.5, 0.0, 1.0, 1.0, 1.0 }, /* cyan */

    /* Top right */
    { -0.5, -0.5, -0.5, 0.0, 1.0, 0.0, 1.0 }, /* green */
    {  0.5, -0.5, -0.5, 0.0, 0.0, 1.0, 1.0 }, /* blue */
    {  0.5, -0.5,  0.5, 1.0, 0.0, 1.0, 1.0 } /* magenta */
};

/*
 * Simulates desktop's glRotatef. The matrix is returned in column-major
 * order.
 */
static void
rotate_matrix(float angle, float x, float y, float z, float *r)
{
    float radians, c, s, c1, u[3], length;
    int i, j;

    radians = angle * SDL_PI_F / 180.0f;

    c = SDL_cosf(radians);
    s = SDL_sinf(radians);

    c1 = 1.0f - SDL_cosf(radians);

    length = (float)SDL_sqrt(x * x + y * y + z * z);

    u[0] = x / length;
    u[1] = y / length;
    u[2] = z / length;

    for (i = 0; i < 16; i++) {
        r[i] = 0.0;
    }

    r[15] = 1.0;

    for (i = 0; i < 3; i++) {
        r[i * 4 + (i + 1) % 3] = u[(i + 2) % 3] * s;
        r[i * 4 + (i + 2) % 3] = -u[(i + 1) % 3] * s;
    }

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            r[i * 4 + j] += c1 * u[i] * u[j] + (i == j ? c : 0.0f);
        }
    }
}

/*
 * Simulates gluPerspectiveMatrix
 */
static void
perspective_matrix(float fovy, float aspect, float znear, float zfar, float *r)
{
    int i;
    float f;

    f = 1.0f/SDL_tanf(fovy * 0.5f);

    for (i = 0; i < 16; i++) {
        r[i] = 0.0;
    }

    r[0] = f / aspect;
    r[5] = f;
    r[10] = (znear + zfar) / (znear - zfar);
    r[11] = -1.0f;
    r[14] = (2.0f * znear * zfar) / (znear - zfar);
    r[15] = 0.0f;
}

/*
 * Multiplies lhs by rhs and writes out to r. All matrices are 4x4 and column
 * major. In-place multiplication is supported.
 */
static void
multiply_matrix(float *lhs, float *rhs, float *r)
{
    int i, j, k;
    float tmp[16];

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            tmp[j * 4 + i] = 0.0;

            for (k = 0; k < 4; k++) {
                tmp[j * 4 + i] += lhs[k * 4 + i] * rhs[j * 4 + k];
            }
        }
    }

    for (i = 0; i < 16; i++) {
        r[i] = tmp[i];
    }
}

static int Init(Context* context)
{
    /* Manually set up example for HDR rendering */
    context->Device = SDL_GpuCreateDevice(SDL_GPU_BACKEND_ALL, SDL_TRUE);
	if (context->Device == NULL)
	{
		SDL_Log("GpuCreateDevice failed");
		return -1;
	}

    context->Window = SDL_CreateWindow(context->ExampleName, 640, 480, 0);
	if (context->Window == NULL)
	{
		SDL_Log("CreateWindow failed: %s", SDL_GetError());
		return -1;
	}

	if (!SDL_GpuClaimWindow(context->Device, context->Window, SDL_GPU_SWAPCHAINCOMPOSITION_HDR_ADVANCED, SDL_GPU_PRESENTMODE_VSYNC))
	{
		SDL_Log("GpuClaimWindow failed");
		return -1;
	}

    SDL_GetWindowSizeInPixels(context->Window, &w, &h);

	size_t vsCodeSize;
	void* vsBytes = LoadAsset("Content/Shaders/Compiled/PositionColorTransform.vert.spv", &vsCodeSize);
	if (vsBytes == NULL)
	{
		SDL_Log("Could not load vertex shader from disk!");
		return -1;
	}

	size_t fsCodeSize;
	void *fsBytes = LoadAsset("Content/Shaders/Compiled/SolidColor.frag.spv", &fsCodeSize);
	if (fsBytes == NULL)
	{
		SDL_Log("Could not load fragment shader from disk!");
		return -1;
	}

	SDL_GpuShader* vertexShader = SDL_GpuCreateShader(context->Device, &(SDL_GpuShaderCreateInfo){
		.stage = SDL_GPU_SHADERSTAGE_VERTEX,
		.code = vsBytes,
		.codeSize = vsCodeSize,
		.entryPointName = "vs_main",
		.format = SDL_GPU_SHADERFORMAT_SPIRV,
	});
	if (vertexShader == NULL)
	{
		SDL_Log("Failed to create vertex shader!");
		return -1;
	}

	SDL_GpuShader* fragmentShader = SDL_GpuCreateShader(context->Device, &(SDL_GpuShaderCreateInfo){
		.stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
		.code = fsBytes,
		.codeSize = fsCodeSize,
		.entryPointName = "fs_main",
		.format = SDL_GPU_SHADERFORMAT_SPIRV
	});
	if (fragmentShader == NULL)
	{
		SDL_Log("Failed to create fragment shader!");
		return -1;
	}

    RenderTexture = SDL_GpuCreateTexture(context->Device, &(SDL_GpuTextureCreateInfo){
        .format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_SFLOAT,
        .width = w,
        .height = h,
        .depth = 1,
        .layerCount = 1,
        .levelCount = 1,
        .usageFlags = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT
    });

    DepthTexture = SDL_GpuCreateTexture(context->Device, &(SDL_GpuTextureCreateInfo){
        .format = SDL_GPU_TEXTUREFORMAT_D16_UNORM,
        .width = w,
        .height = h,
        .depth = 1,
        .layerCount = 1,
        .levelCount = 1,
        .usageFlags = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET_BIT
    });

	VertexBuffer = SDL_GpuCreateGpuBuffer(
		context->Device,
		SDL_GPU_BUFFERUSAGE_VERTEX_BIT,
		sizeof(VertexData)
	);

    DrawPipeline = SDL_GpuCreateGraphicsPipeline(context->Device, &(SDL_GpuGraphicsPipelineCreateInfo){
        .attachmentInfo = (SDL_GpuGraphicsPipelineAttachmentInfo){
            .colorAttachmentCount = 1,
            .colorAttachmentDescriptions = (SDL_GpuColorAttachmentDescription[]){{
                .format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_SFLOAT,
                .blendState.colorWriteMask = 0xF
            }},
            .hasDepthStencilAttachment = SDL_TRUE,
            .depthStencilFormat = SDL_GPU_TEXTUREFORMAT_D16_UNORM
        },
        .depthStencilState = (SDL_GpuDepthStencilState){
            .depthTestEnable = SDL_TRUE,
            .depthWriteEnable = SDL_TRUE,
            .compareOp = SDL_GPU_COMPAREOP_LESS_OR_EQUAL
        },
		.rasterizerState = (SDL_GpuRasterizerState){
			.cullMode = SDL_GPU_CULLMODE_NONE,
			.fillMode = SDL_GPU_FILLMODE_FILL,
			.frontFace = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE
		},
        .vertexInputState = (SDL_GpuVertexInputState){
			.vertexBindingCount = 1,
			.vertexBindings = (SDL_GpuVertexBinding[]){{
				.binding = 0,
				.inputRate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
				.stepRate = 0,
				.stride = sizeof(PositionColorVertex)
			}},
			.vertexAttributeCount = 2,
			.vertexAttributes = (SDL_GpuVertexAttribute[]){{
				.binding = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_VECTOR3,
				.location = 0,
				.offset = 0
			}, {
				.binding = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_VECTOR4,
				.location = 1,
				.offset = sizeof(float) * 3
			}}
		},
        .multisampleState.sampleMask = 0xFFFF,
        .primitiveType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .vertexShader = vertexShader,
        .vertexResourceLayoutInfo.uniformBufferCount = 1,
        .fragmentShader = fragmentShader,
    });

    SDL_GpuQueueDestroyShader(context->Device, vertexShader);
    SDL_GpuQueueDestroyShader(context->Device, fragmentShader);
    SDL_free(vsBytes);
    SDL_free(fsBytes);

    SDL_GpuTransferBuffer* transferBuffer = SDL_GpuCreateTransferBuffer(
        context->Device,
        SDL_GPU_TRANSFERUSAGE_BUFFER,
        SDL_GPU_TRANSFER_MAP_WRITE,
        sizeof(VertexData)
    );

    PositionColorVertex* mapPointer;

    SDL_GpuMapTransferBuffer(
        context->Device,
        transferBuffer,
        SDL_FALSE,
        (void**) &mapPointer
    );

    SDL_memcpy(mapPointer, VertexData, sizeof(VertexData));

    SDL_GpuUnmapTransferBuffer(
        context->Device,
        transferBuffer
    );

    SDL_GpuCommandBuffer* uploadCmdBuf = SDL_GpuAcquireCommandBuffer(context->Device);
    SDL_GpuCopyPass* copyPass = SDL_GpuBeginCopyPass(uploadCmdBuf);

    SDL_GpuUploadToBuffer(
        copyPass,
        transferBuffer,
        VertexBuffer,
        &(SDL_GpuBufferCopy) {
            .srcOffset = 0,
            .dstOffset = 0,
            .size = sizeof(VertexData)
        },
        SDL_FALSE
    );

    SDL_GpuEndCopyPass(copyPass);
    SDL_GpuSubmit(uploadCmdBuf);
    SDL_GpuQueueDestroyTransferBuffer(context->Device, transferBuffer);

    return 0;
}

static int Update(Context* context)
{
    /*
     * Do some rotation with Euler angles. It is not a fixed axis as
     * quaternions would be, but the effect is cool.
     */
    rotate_matrix(angle_x, 1.0f, 0.0f, 0.0f, matrix_modelview);
    rotate_matrix(angle_y, 0.0f, 1.0f, 0.0f, matrix_rotate);

    multiply_matrix(matrix_rotate, matrix_modelview, matrix_modelview);

    rotate_matrix(angle_z, 0.0f, 1.0f, 0.0f, matrix_rotate);

    multiply_matrix(matrix_rotate, matrix_modelview, matrix_modelview);

    /* Pull the camera back from the cube */
    matrix_modelview[14] -= 2.5;

    perspective_matrix(45.0f, (float)w/h, 0.01f, 100.0f, matrix_perspective);
    multiply_matrix(matrix_perspective, matrix_modelview, (float*) &matrix_final);

    angle_x += 3;
    angle_y += 2;
    angle_z += 1;

    if(angle_x >= 360) angle_x -= 360;
    if(angle_x < 0) angle_x += 360;
    if(angle_y >= 360) angle_y -= 360;
    if(angle_y < 0) angle_y += 360;
    if(angle_z >= 360) angle_z -= 360;
    if(angle_z < 0) angle_z += 360;

    return 0;
}

static int Draw(Context* context)
{
    SDL_GpuCommandBuffer* cmdbuf = SDL_GpuAcquireCommandBuffer(context->Device);
    if (cmdbuf == NULL)
    {
        SDL_Log("GpuAcquireCommandBuffer failed");
        return -1;
    }

    Uint32 swapchainWidth, swapchainHeight;
    SDL_GpuTexture* swapchainTexture = SDL_GpuAcquireSwapchainTexture(cmdbuf, context->Window, &swapchainWidth, &swapchainHeight);
    if (swapchainTexture != NULL)
    {
        SDL_GpuRenderPass* renderPass = SDL_GpuBeginRenderPass(
            cmdbuf,
            &(SDL_GpuColorAttachmentInfo){
                .textureSlice.texture = RenderTexture,
                .loadOp = SDL_GPU_LOADOP_CLEAR,
                .storeOp = SDL_GPU_STOREOP_STORE,
                .clearColor.a = 1.0f,
                .cycle = SDL_TRUE
            },
            1,
            &(SDL_GpuDepthStencilAttachmentInfo){
                .textureSlice.texture = DepthTexture,
                .loadOp = SDL_GPU_LOADOP_CLEAR,
                .storeOp = SDL_GPU_STOREOP_DONT_CARE,
                .depthStencilClearValue.depth = 1.0f,
                .cycle = SDL_TRUE
            }
        );

        SDL_GpuBindGraphicsPipeline(renderPass, DrawPipeline);
        SDL_GpuBindVertexBuffers(renderPass, 0, &(SDL_GpuBufferBinding){ .gpuBuffer = VertexBuffer, .offset = 0 }, 1);
        SDL_GpuPushVertexUniformData(renderPass, 0, matrix_final, sizeof(matrix_final));
        SDL_GpuDrawPrimitives(renderPass, 0, 12);

        SDL_GpuEndRenderPass(renderPass);

        SDL_GpuBlit(
            cmdbuf,
            &(SDL_GpuTextureRegion){
                .textureSlice.texture = RenderTexture,
                .w = w,
                .h = h,
                .d = 1
            },
            &(SDL_GpuTextureRegion){
                .textureSlice.texture = swapchainTexture,
                .w = w,
                .h = h,
                .d = 1
            },
            SDL_GPU_FILTER_LINEAR,
            SDL_FALSE
        );
    }

    SDL_GpuSubmit(cmdbuf);

    return 0;
}

static void Quit(Context* context)
{
    SDL_GpuQueueDestroyGraphicsPipeline(context->Device, DrawPipeline);
    SDL_GpuQueueDestroyTexture(context->Device, RenderTexture);
    SDL_GpuQueueDestroyTexture(context->Device, DepthTexture);
    SDL_GpuQueueDestroyGpuBuffer(context->Device, VertexBuffer);

    SDL_GpuUnclaimWindow(context->Device, context->Window);
    SDL_DestroyWindow(context->Window);
    SDL_GpuDestroyDevice(context->Device);
}

Example ToneMapping_Example = { "ToneMapping", Init, Update, Draw, Quit };
