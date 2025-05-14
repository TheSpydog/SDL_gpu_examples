Texture2D<float4> inImage : register(t0, space0);
[[vk::image_format("rgba16f")]]
RWTexture2D<float4> outImage : register(u0, space1);

float3 reinhard(float3 v)
{
    return v / (1.0f.xxx + v);
}

[numthreads(8, 8, 1)]
void main(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
    int2 coord = int2(GlobalInvocationID.xy);
    float4 inPixel = inImage[coord];
    float3 outColor = reinhard(inPixel.xyz);
    outImage[coord] = float4(outColor, 1.0f);
}
