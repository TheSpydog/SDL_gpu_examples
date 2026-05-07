Texture2D DepthTexture : register(t0, space2);
SamplerState DepthSampler : register(s0, space2);

float4 main(float2 TexCoord : TEXCOORD0) : SV_Target0
{
    // get our color & depth value
    float depth = DepthTexture.Sample(DepthSampler, TexCoord).r;

    // combine results
    return float4(depth, depth, depth, 1.0);
}
