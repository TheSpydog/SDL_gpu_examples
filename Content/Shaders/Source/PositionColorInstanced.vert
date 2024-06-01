#version 450

layout (location = 0) in vec3 Position;
layout (location = 1) in vec4 Color;

layout (location = 0) out vec4 outColor;

void main()
{
	outColor = Color;

	vec3 pos = (Position * 0.25) - vec3(0.75, 0.75, 0);
	pos.x += (gl_InstanceIndex % 4) * 0.5;
	pos.y += floor(gl_InstanceIndex / 4) * 0.5;
	gl_Position = vec4(pos, 1);
}