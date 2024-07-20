#version 450

// This shader performs downsampling on a texture,
// as taken from the Call Of Duty method, presented at ACM Siggraph 2014.

layout (location = 0) in vec2 TexCoord;
layout (location = 0) out vec4 FragColor;
layout (set = 2, binding = 0) uniform sampler2D Sampler;

void main()
{
    vec2 srcTexelSize = vec2(1.0, 1.0) / textureSize(Sampler, 0);
    float x = srcTexelSize.x;
    float y = srcTexelSize.y;

    // Take 13 samples around current texel:
    // a - b - c
    // - j - k -
    // d - e - f
    // - l - m -
    // g - h - i
    // === ('e' is the current texel) ===
    vec3 a = texture(Sampler, vec2(TexCoord.x - 2*x, TexCoord.y + 2*y)).rgb;
    vec3 b = texture(Sampler, vec2(TexCoord.x,       TexCoord.y + 2*y)).rgb;
    vec3 c = texture(Sampler, vec2(TexCoord.x + 2*x, TexCoord.y + 2*y)).rgb;

    vec3 d = texture(Sampler, vec2(TexCoord.x - 2*x, TexCoord.y)).rgb;
    vec3 e = texture(Sampler, vec2(TexCoord.x,       TexCoord.y)).rgb;
    vec3 f = texture(Sampler, vec2(TexCoord.x + 2*x, TexCoord.y)).rgb;

    vec3 g = texture(Sampler, vec2(TexCoord.x - 2*x, TexCoord.y - 2*y)).rgb;
    vec3 h = texture(Sampler, vec2(TexCoord.x,       TexCoord.y - 2*y)).rgb;
    vec3 i = texture(Sampler, vec2(TexCoord.x + 2*x, TexCoord.y - 2*y)).rgb;

    vec3 j = texture(Sampler, vec2(TexCoord.x - x, TexCoord.y + y)).rgb;
    vec3 k = texture(Sampler, vec2(TexCoord.x + x, TexCoord.y + y)).rgb;
    vec3 l = texture(Sampler, vec2(TexCoord.x - x, TexCoord.y - y)).rgb;
    vec3 m = texture(Sampler, vec2(TexCoord.x + x, TexCoord.y - y)).rgb;

    // Apply weighted distribution:
    // 0.5 + 0.125 + 0.125 + 0.125 + 0.125 = 1
    // a,b,d,e * 0.125
    // b,c,e,f * 0.125
    // d,e,g,h * 0.125
    // e,f,h,i * 0.125
    // j,k,l,m * 0.5
    // This shows 5 square areas that are being sampled. But some of them overlap,
    // so to have an energy preserving downsample we need to make some adjustments.
    // The weights are distributed, so that the sum of j,k,l,m (e.g.)
    // contribute 0.5 to the final color output. The code below is written
    // to effectively yield this sum. We get:
    // 0.125*5 + 0.03125*4 + 0.0625*4 = 1
	vec3 color = e * 0.125;
    color = e*0.125;
    color += (a+c+g+i)*0.03125;
    color += (b+d+f+h)*0.0625;
    color += (j+k+l+m)*0.125;

	FragColor = vec4(color, 0.0);
}