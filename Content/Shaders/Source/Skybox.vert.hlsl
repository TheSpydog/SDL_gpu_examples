cbuffer UniformBlock : register(b0, space1)
{
    float4x4 MatrixTransform : packoffset(c0);
};

struct SPIRV_Cross_Input
{
    float3 inTexCoord : TEXCOORD0;
};

struct Output
{
    float3 TexCoord : TEXCOORD0;
    float4 Position : SV_Position;
};

Output main(float3 inTexCoord : TEXCOORD0)
{
    Output output;
    output.TexCoord = inTexCoord;
    output.Position = mul(MatrixTransform, float4(inTexCoord, 1.0));
    return output;
}
