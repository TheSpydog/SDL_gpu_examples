Texture2D<float4> InImage : register(t0, space0);
RWTexture2D<unorm float4> OutImage : register(u0, space1);

float3 NormalizeHDRSceneValue(float3 hdrSceneValue, float paperWhiteNits)
{
    return (hdrSceneValue * paperWhiteNits) / 10000.0f.xxx;
}

float3 LinearToST2084(float3 normalizedLinearValue)
{
    return pow((0.8359375f.xxx + (pow(abs(normalizedLinearValue), 0.1593017578125f.xxx) * 18.8515625f)) / (1.0f.xxx + (pow(abs(normalizedLinearValue), 0.1593017578125f.xxx) * 18.6875f)), 78.84375f.xxx);
}

float4 ConvertToHDR10(float4 hdrSceneValue, float paperWhiteNits)
{
    float3 rec2020 = mul(float3x3(float3(0.6274039745330810546875f, 0.329281985759735107421875f, 0.043313600122928619384765625f), float3(0.06909699738025665283203125f, 0.919539988040924072265625f, 0.0113612003624439239501953125f), float3(0.01639159955084323883056640625f, 0.0880132019519805908203125f, 0.895595014095306396484375f)), hdrSceneValue.xyz);
    float3 normalizedLinearValue = NormalizeHDRSceneValue(rec2020, paperWhiteNits);
    float3 HDR10 = LinearToST2084(normalizedLinearValue);
    return float4(HDR10, hdrSceneValue.w);
}

[numthreads(8, 8, 1)]
void main(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
    int2 coord = int2(GlobalInvocationID.xy);
    float4 inPixel = InImage[coord];
    OutImage[coord] = ConvertToHDR10(inPixel, 200.0f);
}
