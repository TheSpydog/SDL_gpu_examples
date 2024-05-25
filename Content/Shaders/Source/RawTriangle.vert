#version 450

layout (location = 0) out vec4 outColor;

void main()
{
	vec2 pos;

	if (gl_VertexIndex == 0)
	{
		pos = vec2(-1, -1);
		outColor = vec4(1, 0, 0, 1);
	}
	else if (gl_VertexIndex == 1)
	{
		pos = vec2(1, -1);
		outColor = vec4(0, 1, 0, 1);
	}
	else if (gl_VertexIndex == 2)
	{
		pos = vec2(0, 1);
		outColor = vec4(0, 0, 1, 1);
	}

	gl_Position = vec4(pos, 0, 1);
}