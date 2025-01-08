cbuffer UBO : register(b0, space3)
{
    float2 PixelSize : packoffset(c0);
};

Texture2D ColorTexture : register(t0, space2);
SamplerState ColorSampler : register(s0, space2);

Texture2D DepthTexture : register(t1, space2);
SamplerState DepthSampler : register(s1, space2);

// Gets the difference between a depth value and adjacent depth pixels
// This is used to detect "edges", where the depth falls off.
float GetDifference(float depth, float2 TexCoord, float distance)
{
    return
        max(DepthTexture.Sample(DepthSampler, TexCoord + float2(PixelSize.x, 0) * distance).r - depth,
        max(DepthTexture.Sample(DepthSampler, TexCoord + float2(-PixelSize.x, 0) * distance).r - depth,
        max(DepthTexture.Sample(DepthSampler, TexCoord + float2(0, PixelSize.y) * distance).r - depth,
        DepthTexture.Sample(DepthSampler, TexCoord + float2(0, -PixelSize.y) * distance).r - depth)));
}

float4 main(float2 TexCoord : TEXCOORD0) : SV_Target0
{
    // get out depth value
    float4 color = ColorTexture.Sample(ColorSampler, TexCoord);
    float depth = DepthTexture.Sample(DepthSampler, TexCoord).r;

    // get the difference between the edges at 1px and 2px away
    float edge = step(0.1, GetDifference(depth, TexCoord, 1.0f));
    float edge2 = step(0.1, GetDifference(depth, TexCoord, 2.0f));

    // turn inner edges black
    float3 res = lerp(color.rgb, 0, edge2);

    // turn the outer edges white
    res = lerp(res, 1, edge);

    // combine results
    return float4(res, color.a);
}
