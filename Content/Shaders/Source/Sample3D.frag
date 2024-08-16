#version 450

layout (location = 0) in vec2 TexCoord;
layout (location = 0) out vec4 FragColor;
layout (set = 2, binding = 0) uniform sampler3D Sampler;

void main()
{
    float depthIndex;
    if (TexCoord.x < 0.5 && TexCoord.y < 0.5)
    {
        depthIndex = 0;
    }
    else if (TexCoord.x >= 0.5 && TexCoord.y < 0.5)
    {
        depthIndex = 0.25;
    }
    else if (TexCoord.x < 0.5 && TexCoord.y >= 0.5)
    {
        depthIndex = 0.5;
    }
    else
    {
        depthIndex = 0.75;
    }

    FragColor = texture(Sampler, vec3(TexCoord, depthIndex));
}
