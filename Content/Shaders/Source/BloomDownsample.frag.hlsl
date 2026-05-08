// This shader performs downsampling on a texture,
// as taken from the Call Of Duty method, presented at ACM Siggraph 2014.

struct Input
{
    float2 TexCoord : TEXCOORD0;
};

Texture2D ColorTexture : register(t0, space2);
SamplerState ColorSampler : register(s0, space2);

float4 main(float2 TexCoord : TEXCOORD0) : SV_Target0
{
    float width, height;
    ColorTexture.GetDimensions(width, height);

    float2 srcTexelSize = float2(1.0, 1.0) / float2(width, height);
    float x = srcTexelSize.x;
    float y = srcTexelSize.y;

    // Take 13 samples around current texel:
    // a - b - c
    // - j - k -
    // d - e - f
    // - l - m -
    // g - h - i
    // === ('e' is the current texel) ===
    float3 a = ColorTexture.Sample(ColorSampler, float2(TexCoord.x - 2*x, TexCoord.y + 2*y)).rgb;
    float3 b = ColorTexture.Sample(ColorSampler, float2(TexCoord.x,       TexCoord.y + 2*y)).rgb;
    float3 c = ColorTexture.Sample(ColorSampler, float2(TexCoord.x + 2*x, TexCoord.y + 2*y)).rgb;

    float3 d = ColorTexture.Sample(ColorSampler, float2(TexCoord.x - 2*x, TexCoord.y)).rgb;
    float3 e = ColorTexture.Sample(ColorSampler, float2(TexCoord.x,       TexCoord.y)).rgb;
    float3 f = ColorTexture.Sample(ColorSampler, float2(TexCoord.x + 2*x, TexCoord.y)).rgb;

    float3 g = ColorTexture.Sample(ColorSampler, float2(TexCoord.x - 2*x, TexCoord.y - 2*y)).rgb;
    float3 h = ColorTexture.Sample(ColorSampler, float2(TexCoord.x,       TexCoord.y - 2*y)).rgb;
    float3 i = ColorTexture.Sample(ColorSampler, float2(TexCoord.x + 2*x, TexCoord.y - 2*y)).rgb;

    float3 j = ColorTexture.Sample(ColorSampler, float2(TexCoord.x - x, TexCoord.y + y)).rgb;
    float3 k = ColorTexture.Sample(ColorSampler, float2(TexCoord.x + x, TexCoord.y + y)).rgb;
    float3 l = ColorTexture.Sample(ColorSampler, float2(TexCoord.x - x, TexCoord.y - y)).rgb;
    float3 m = ColorTexture.Sample(ColorSampler, float2(TexCoord.x + x, TexCoord.y - y)).rgb;

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
	float3 color = e * 0.125;
    color = e*0.125;
    color += (a+c+g+i)*0.03125;
    color += (b+d+f+h)*0.0625;
    color += (j+k+l+m)*0.125;

	return float4(color, 0.0);
}
