struct SpriteData
{
    float3 Position;
    float Rotation;
    float2 Scale;
    float2 Padding;
    float TexU, TexV, TexW, TexH;
    float4 Color;
};

struct Output
{
    float2 Texcoord : TEXCOORD0;
    float4 Color : TEXCOORD1;
    float4 Position : SV_Position;
};

StructuredBuffer<SpriteData> DataBuffer : register(t0, space0);

cbuffer UniformBlock : register(b0, space1)
{
    float4x4 ViewProjectionMatrix : packoffset(c0);
};

static const uint triangleIndices[6] = {0, 1, 2, 3, 2, 1};
static const float2 vertexPos[4] = {
    {0.0f, 0.0f},
    {1.0f, 0.0f},
    {0.0f, 1.0f},
    {1.0f, 1.0f}
};

Output main(uint id : SV_VertexID)
{
    uint spriteIndex = id / 6;
    uint vert = triangleIndices[id % 6];
    SpriteData sprite = DataBuffer[spriteIndex];

    float2 texcoord[4] = {
        {sprite.TexU,               sprite.TexV              },
        {sprite.TexU + sprite.TexW, sprite.TexV              },
        {sprite.TexU,               sprite.TexV + sprite.TexH},
        {sprite.TexU + sprite.TexW, sprite.TexV + sprite.TexH}
    };

    float c = cos(sprite.Rotation);
    float s = sin(sprite.Rotation);

    float2 coord = vertexPos[vert];
    coord *= sprite.Scale;
    float2x2 rotation = {c, s, -s, c};
    coord = mul(coord, rotation);

    float3 coordWithDepth = float3(coord + sprite.Position.xy, sprite.Position.z);

    Output output;

    output.Position = mul(ViewProjectionMatrix, float4(coordWithDepth, 1.0f));
    output.Texcoord = texcoord[vert];
    output.Color = sprite.Color;

    return output;
}
