#version 450

// This shader performs upsampling with blur on a texture
// as taken from the Call Of Duty method, presented at ACM Siggraph 2014.

layout (location = 0) in vec2 TexCoord;
layout (location = 0) out vec4 FragColor;
layout (set = 2, binding = 0) uniform sampler2D Sampler;
layout (set = 3, binding = 0) uniform UniformBlock
{
    float FilterRadius;
};

void main()
{
    // The filter kernel is applied with a radius, specified in texture
    // coordinates, so that the radius will vary across mip resolutions.
    float x = FilterRadius;
    float y = FilterRadius;

    // Take 9 samples around current texel:
    // a - b - c
    // d - e - f
    // g - h - i
    // === ('e' is the current texel) ===
    vec3 a = texture(Sampler, vec2(TexCoord.x - x, TexCoord.y + y)).rgb;
    vec3 b = texture(Sampler, vec2(TexCoord.x,     TexCoord.y + y)).rgb;
    vec3 c = texture(Sampler, vec2(TexCoord.x + x, TexCoord.y + y)).rgb;

    vec3 d = texture(Sampler, vec2(TexCoord.x - x, TexCoord.y)).rgb;
    vec3 e = texture(Sampler, vec2(TexCoord.x,     TexCoord.y)).rgb;
    vec3 f = texture(Sampler, vec2(TexCoord.x + x, TexCoord.y)).rgb;

    vec3 g = texture(Sampler, vec2(TexCoord.x - x, TexCoord.y - y)).rgb;
    vec3 h = texture(Sampler, vec2(TexCoord.x,     TexCoord.y - y)).rgb;
    vec3 i = texture(Sampler, vec2(TexCoord.x + x, TexCoord.y - y)).rgb;

    // Apply weighted distribution, by using a 3x3 tent filter:
    //  1   | 1 2 1 |
    // -- * | 2 4 2 |
    // 16   | 1 2 1 |
    vec3 color = e*4.0;
    color += (b+d+f+h)*2.0;
    color += (a+c+g+i);
    color *= 1.0 / 16.0;

    FragColor = vec4(color, 1.0);
}
