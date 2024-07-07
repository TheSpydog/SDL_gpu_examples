#version 450

layout(location = 0) in vec3 inTexCoord;
layout(location = 0) out vec3 outTexCoord;

layout(set = 1, binding = 0) uniform UniformBlock
{
	mat4 MatrixTransform;
};

void main()
{
    outTexCoord = inTexCoord;
    gl_Position = MatrixTransform * vec4(inTexCoord, 1.0);
}