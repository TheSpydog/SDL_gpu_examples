Texture2D<float4> inImage : register(t0, space0);
[[vk::image_format("rgba16f")]]
RWTexture2D<float4> outImage : register(u0, space1);

float3 hable_tonemap_partial(float3 x)
{
    float A = 0.15f;
    float B = 0.50f;
    float C = 0.10f;
    float D = 0.20f;
    float E = 0.02f;
    float F = 0.30f;
    return (((x * ((x * A) + (C * B).xxx)) + (D * E).xxx) / ((x * ((x * A) + B.xxx)) + (D * F).xxx)) - (E / F).xxx;
}

float3 hable_filmic(float3 v)
{
    float exposure_bias = 2.0f;
    float3 curr = hable_tonemap_partial(v * exposure_bias);

    float3 W = 11.2f.xxx;
    float3 white_scale = 1.0f.xxx / hable_tonemap_partial(W);
    return curr * white_scale;
}

[numthreads(8, 8, 1)]
void main(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
    int2 coord = int2(GlobalInvocationID.xy);
    float4 inPixel = inImage[coord];
    float3 outColor = hable_filmic(inPixel.xyz);
    outImage[coord] = float4(outColor, 1.0f);
}
