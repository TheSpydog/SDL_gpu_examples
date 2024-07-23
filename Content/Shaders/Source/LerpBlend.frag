#version 450

layout (location = 0) in vec2 TexCoord;
layout (location = 0) out vec4 FragColor;
layout (set = 2, binding = 0) uniform sampler2D Primary;
layout (set = 2, binding = 1) uniform sampler2D Secondary;
layout (set = 3, binding = 0) uniform UniformBlock
{
    float Weight;
};

void main()
{
    vec4 primary = texture(Primary, TexCoord);
    vec4 secondary = texture(Secondary, TexCoord);

    FragColor = mix(primary, secondary, Weight);
}