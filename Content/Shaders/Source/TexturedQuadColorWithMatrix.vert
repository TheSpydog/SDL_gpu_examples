#version 450

layout (location = 0) in vec4 Position;
layout (location = 1) in vec2 TexCoord;
layout (location = 2) in vec4 Color;

layout (location = 0) out vec2 outTexCoord;
layout (location = 1) out vec4 outColor;

layout (set = 1, binding = 0) uniform UniformBlock
{
	mat4x4 MatrixTransform;
};

void main()
{
	outTexCoord = TexCoord;
	outColor = Color;
	gl_Position = MatrixTransform * Position;
}
