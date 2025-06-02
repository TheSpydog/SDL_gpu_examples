cbuffer UBO : register(b0, space2)
{
    float ubo_texcoord_multiplier : packoffset(c0);
};

Texture2D<float4> inImage : register(t0, space0);
SamplerState inImageSampler : register(s0, space0);

[[vk::image_format("rgba8")]]
RWTexture2D<float4> outImage : register(u0, space1);

[numthreads(8, 8, 1)]
void main(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
    float w, h;
    inImage.GetDimensions(w, h);
    int2 coord = int2(GlobalInvocationID.xy);
    float2 texcoord = (float2(coord) * ubo_texcoord_multiplier) / float2(w, h);
    float4 inPixel = inImage.SampleLevel(inImageSampler, texcoord, 0.0f);
    outImage[coord] = inPixel;
}
