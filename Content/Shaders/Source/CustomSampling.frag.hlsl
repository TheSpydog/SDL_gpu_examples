cbuffer UBO : register(b0, space3)
{
    int mode : packoffset(c0);
};

Texture2D<float4> Texture : register(t0, space2);

float4 main(float2 TexCoord : TEXCOORD0) : SV_Target0
{
    float w, h;
    Texture.GetDimensions(w, h);
    int2 texelPos = int2(float2(w, h) * TexCoord);
    float4 mainTexel = Texture[texelPos];
    if (mode == 0)
    {
        return mainTexel;
    }
    else
    {
        float4 bottomTexel = Texture[texelPos + int2(0, 1)];
        float4 leftTexel = Texture[texelPos + int2(-1, 0)];
        float4 topTexel = Texture[texelPos + int2(0, -1)];
        float4 rightTexel = Texture[texelPos + int2(1, 0)];
        return ((((mainTexel * 0.2f) + (bottomTexel * 0.2f)) + (leftTexel * 0.20000000298023223876953125f)) + (topTexel * 0.20000000298023223876953125f)) + (rightTexel * 0.2f);
    }
}
