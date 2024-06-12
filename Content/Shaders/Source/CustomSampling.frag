#version 450

layout (location = 0) in vec2 TexCoord;
layout (location = 0) out vec4 FragColor;
layout (set = 2, binding = 0, rgba8) uniform readonly image2D Texture;
layout (set = 3, binding = 0) uniform UBO
{
    int mode;
};

void main()
{
    ivec2 texelPos = ivec2(imageSize(Texture) * TexCoord);
    vec4 mainTexel = imageLoad(Texture, texelPos);
    if (mode == 0)
    {
        FragColor = mainTexel;
    }
    else
    {
        vec4 bottomTexel = imageLoad(Texture, texelPos + ivec2(0, 1));
        vec4 leftTexel = imageLoad(Texture, texelPos + ivec2(-1, 0));
        vec4 topTexel = imageLoad(Texture, texelPos + ivec2(0, -1));
        vec4 rightTexel = imageLoad(Texture, texelPos + ivec2(1, 0));
        FragColor = 0.2 * mainTexel + 0.2 * bottomTexel + 0.2 * leftTexel + 0.2 * topTexel + 0.2 * rightTexel;
    }
}
