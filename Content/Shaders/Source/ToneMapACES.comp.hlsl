Texture2D<float4> inImage : register(t0, space0);
RWTexture2D<float4> outImage : register(u0, space1);

float3 rtt_and_odt_fit(float3 v)
{
    float3 a = (v * (v + 0.0245786f.xxx)) - 0.000090537f.xxx;
    float3 b = (v * ((v * 0.983729f) + 0.4329510f.xxx)) + 0.238081f.xxx;
    return a / b;
}

float3 aces_fitted(float3 v)
{
    v = mul(float3x3(float3(0.59719f, 0.35458f, 0.04823f), float3(0.07600f, 0.90834f, 0.01566f), float3(0.02840f, 0.13383f, 0.83777f)), v);
    v = rtt_and_odt_fit(v);
    return mul(float3x3(float3(1.60475f, -0.53108f, -0.07367f), float3(-0.10208f, 1.10813f, -0.00605f), float3(-0.00327f, -0.07276f, 1.07602f)), v);
}

[numthreads(8, 8, 1)]
void main(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
    int2 coord = int2(GlobalInvocationID.xy);
    float4 inPixel = inImage[coord];
    float3 outColor = aces_fitted(inPixel.xyz);
    outImage[coord] = float4(outColor, 1.0f);
}
