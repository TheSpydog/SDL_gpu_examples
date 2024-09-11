#version 450

layout(location = 0) in vec3 TexCoord;
layout(location = 0) out vec4 FragColor;

layout(binding = 0, set = 2) uniform samplerCubeArray SkyboxSampler;

layout(set = 3, binding = 0) uniform UBO
{
	uint arrayIndex;
} ubo;

void main()
{
    FragColor = texture(SkyboxSampler, vec4(TexCoord, float(ubo.arrayIndex)));
}
