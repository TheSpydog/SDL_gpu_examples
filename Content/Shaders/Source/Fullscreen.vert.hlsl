struct Output
{
    float2 TexCoord : TEXCOORD0;
    float4 Position : SV_Position;
};

Output main(uint VertexIndex : SV_VertexID)
{
    Output output;
    output.TexCoord = float2(float((VertexIndex << 1) & 2), float(VertexIndex & 2));
    output.Position = float4((output.TexCoord * float2(2.0f, -2.0f)) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}
