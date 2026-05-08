// This shader performs upsampling with blur on a texture
// as taken from the Call Of Duty method, presented at ACM Siggraph 2014.

Texture2D ColorTexture : register(t0, space2);
SamplerState ColorSampler : register(s0, space2);

cbuffer UBO : register(b0, space3)
{
    float FilterRadius;
};

float4 main(float2 TexCoord : TEXCOORD0) : SV_Target0
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
    float3 a = ColorTexture.Sample(ColorSampler, float2(TexCoord.x - x, TexCoord.y + y)).rgb;
    float3 b = ColorTexture.Sample(ColorSampler, float2(TexCoord.x,     TexCoord.y + y)).rgb;
    float3 c = ColorTexture.Sample(ColorSampler, float2(TexCoord.x + x, TexCoord.y + y)).rgb;

    float3 d = ColorTexture.Sample(ColorSampler, float2(TexCoord.x - x, TexCoord.y)).rgb;
    float3 e = ColorTexture.Sample(ColorSampler, float2(TexCoord.x,     TexCoord.y)).rgb;
    float3 f = ColorTexture.Sample(ColorSampler, float2(TexCoord.x + x, TexCoord.y)).rgb;

    float3 g = ColorTexture.Sample(ColorSampler, float2(TexCoord.x - x, TexCoord.y - y)).rgb;
    float3 h = ColorTexture.Sample(ColorSampler, float2(TexCoord.x,     TexCoord.y - y)).rgb;
    float3 i = ColorTexture.Sample(ColorSampler, float2(TexCoord.x + x, TexCoord.y - y)).rgb;

    // Apply weighted distribution, by using a 3x3 tent filter:
    //  1   | 1 2 1 |
    // -- * | 2 4 2 |
    // 16   | 1 2 1 |
    float3 color = e*4.0;
    color += (b+d+f+h)*2.0;
    color += (a+c+g+i);
    color *= 1.0 / 16.0;

    return float4(color, 1.0);
}
