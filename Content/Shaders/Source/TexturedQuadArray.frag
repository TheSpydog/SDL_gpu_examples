#version 450

layout (location = 0) in vec2 TexCoord;
layout (location = 0) out vec4 FragColor;
layout (set = 2, binding = 0) uniform sampler2DArray Sampler;

void main()
{
	uint arrayIndex = (TexCoord.y > 0.5) ? 1 : 0;
	FragColor = texture(Sampler, vec3(TexCoord, arrayIndex));
}
