struct SpriteComputeData
{
    float3 Position;
    float Rotation;
    float2 Scale;
    float2 Padding;
    float TexU, TexV, TexW, TexH;
    float4 Color;
};

struct SpriteVertex
{
    float4 Position;
    float2 Texcoord;
    float4 Color;
};

StructuredBuffer<SpriteComputeData> ComputeBuffer : register(t0, space0);
RWStructuredBuffer<SpriteVertex> VertexBuffer : register(u0, space1);

[numthreads(64, 1, 1)]
void main(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
    uint n = GlobalInvocationID.x;

    SpriteComputeData sprite = ComputeBuffer[n];

    float4x4 Scale = float4x4(
        float4(sprite.Scale.x, 0.0f, 0.0f, 0.0f),
        float4(0.0f, sprite.Scale.y, 0.0f, 0.0f),
        float4(0.0f, 0.0f, 1.0f, 0.0f),
        float4(0.0f, 0.0f, 0.0f, 1.0f)
    );

    float c = cos(sprite.Rotation);
    float s = sin(sprite.Rotation);

    float4x4 Rotation = float4x4(
        float4(   c,    s, 0.0f, 0.0f),
        float4(  -s,    c, 0.0f, 0.0f),
        float4(0.0f, 0.0f, 1.0f, 0.0f),
        float4(0.0f, 0.0f, 0.0f, 1.0f)
    );

    float4x4 Translation = float4x4(
        float4(1.0f, 0.0f, 0.0f, 0.0f),
        float4(0.0f, 1.0f, 0.0f, 0.0f),
        float4(0.0f, 0.0f, 1.0f, 0.0f),
        float4(sprite.Position.x, sprite.Position.y, sprite.Position.z, 1.0f)
    );

    float4x4 Model = mul(Scale, mul(Rotation, Translation));

    float4 topLeft = float4(0.0f, 0.0f, 0.0f, 1.0f);
    float4 topRight = float4(1.0f, 0.0f, 0.0f, 1.0f);
    float4 bottomLeft = float4(0.0f, 1.0f, 0.0f, 1.0f);
    float4 bottomRight = float4(1.0f, 1.0f, 0.0f, 1.0f);

    VertexBuffer[n * 4u]    .Position = mul(topLeft, Model);
    VertexBuffer[n * 4u + 1].Position = mul(topRight, Model);
    VertexBuffer[n * 4u + 2].Position = mul(bottomLeft, Model);
    VertexBuffer[n * 4u + 3].Position = mul(bottomRight, Model);

    VertexBuffer[n * 4u]    .Texcoord = float2(sprite.TexU,               sprite.TexV);
    VertexBuffer[n * 4u + 1].Texcoord = float2(sprite.TexU + sprite.TexW, sprite.TexV);
    VertexBuffer[n * 4u + 2].Texcoord = float2(sprite.TexU,               sprite.TexV + sprite.TexH);
    VertexBuffer[n * 4u + 3].Texcoord = float2(sprite.TexU + sprite.TexW, sprite.TexV + sprite.TexH);

    VertexBuffer[n * 4u]    .Color = sprite.Color;
    VertexBuffer[n * 4u + 1].Color = sprite.Color;
    VertexBuffer[n * 4u + 2].Color = sprite.Color;
    VertexBuffer[n * 4u + 3].Color = sprite.Color;
}
