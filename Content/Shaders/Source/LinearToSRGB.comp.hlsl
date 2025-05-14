Texture2D<float4> InImage : register(t0, space0);
[[vk::image_format("rgba8")]]
RWTexture2D<float4> OutImage : register(u0, space1);

float3 LinearToSRGB(float3 color)
{
    return pow(abs(color), float(1.0f/2.2f).xxx);
}

[numthreads(8, 8, 1)]
void main(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
    int2 coord = int2(GlobalInvocationID.xy);
    float4 inPixel = InImage[coord];
    float3 param = inPixel.xyz;
    float3 outColor = LinearToSRGB(param);
    OutImage[coord] = float4(outColor, 1.0f);
}
