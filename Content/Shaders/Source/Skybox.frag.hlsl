TextureCube<float4> SkyboxTexture : register(t0, space2);
SamplerState SkyboxSampler : register(s0, space2);

float4 main(float3 TexCoord : TEXCOORD0) : SV_Target0
{
    return SkyboxTexture.Sample(SkyboxSampler, TexCoord);
}
