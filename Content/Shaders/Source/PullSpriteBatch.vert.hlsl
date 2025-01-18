struct SpriteData
{
    float3 position;
    float rotation;
    float2 scale;
    float4 color;
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
    SpriteData sprite = DataBuffer[spriteIndex];
    uint vert = triangleIndices[id - spriteIndex * 6];

    float c = cos(sprite.rotation);
    float s = sin(sprite.rotation);

    float2 coord = vertexPos[vert];
    coord *= sprite.scale;
    float2x2 rotation = {c, s, -s, c};
    coord = mul(coord, rotation);

    float3 coordWithDepth = float3(coord + sprite.position.xy, sprite.position.z);

    Output output;

    output.Position = mul(ViewProjectionMatrix, float4(coordWithDepth, 1.0f));
    output.Texcoord = vertexPos[vert]; // replace this with sprite atlas data
    output.Color = sprite.color;

    return output;
}
