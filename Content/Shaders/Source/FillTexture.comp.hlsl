RWTexture2D<unorm float4> outImage : register(u0, space1);

[numthreads(8, 8, 1)]
void main(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
    int2 coord = int2(GlobalInvocationID.xy);
    outImage[coord] = float4(1.0f, 1.0f, 0.0f, 1.0f);
}
