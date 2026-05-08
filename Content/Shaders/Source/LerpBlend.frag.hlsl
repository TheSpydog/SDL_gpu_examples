Texture2D PrimaryTexture : register(t0, space2);
SamplerState PrimarySampler : register(s0, space2);

Texture2D SecondaryTexture : register(t1, space2);
SamplerState SecondarySampler : register(s1, space2);

cbuffer UBO : register(b0, space3)
{
    float Weight;
};

float4 main(float2 TexCoord : TEXCOORD0) : SV_Target0
{
    float4 primary = PrimaryTexture.Sample(PrimarySampler, TexCoord);
    float4 secondary = SecondaryTexture.Sample(SecondarySampler, TexCoord);

    return lerp(primary, secondary, Weight);
}
