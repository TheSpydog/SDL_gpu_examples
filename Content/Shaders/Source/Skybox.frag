#version 450

layout(location = 0) in vec3 TexCoord;
layout(location = 0) out vec4 FragColor;

layout(binding = 0, set = 2) uniform samplerCube SkyboxSampler;

void main()
{
    FragColor = texture(SkyboxSampler, TexCoord);
}