#version 450

layout (location = 0) in vec2 TexCoord;
layout (location = 0) out vec4 FragColor;
layout (set = 2, binding = 0, rgba8) uniform readonly image2D Texture;

void main()
{
    ivec2 coord = ivec2(vec2(imageSize(Texture)) * TexCoord);
    FragColor = imageLoad(Texture, coord);
}
