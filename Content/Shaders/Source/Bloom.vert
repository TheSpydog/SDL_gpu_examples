#version 450

layout (location = 0) in vec3 Position;
layout (location = 1) in vec2 TexCoord;
layout (location = 0) out vec2 outTexCoord;

void main()
{
	outTexCoord = TexCoord;
	gl_Position = vec4(Position, 1);
}
