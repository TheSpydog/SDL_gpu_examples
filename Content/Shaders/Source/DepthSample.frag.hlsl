Texture2DArray DepthTexture : register(t0, space2);
SamplerState DepthSampler : register(s0, space2);

float4 main(float2 TexCoord : TEXCOORD0) : SV_Target0
{
    uint arrayIndex = uint(int(TexCoord.y > 0.5f));

    // get our color & depth value
    float depth = DepthTexture.Sample(DepthSampler, float3(TexCoord, float(arrayIndex))).r;

    // combine results
    return float4(depth, depth, depth, 1.0);
}
