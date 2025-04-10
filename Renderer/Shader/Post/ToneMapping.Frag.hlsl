Texture2D textureLight : register(t1);
SamplerState samplerLight : register(s1);
Texture2D textureHighLight : register(t2);
SamplerState samplerHighLight : register(s2);

struct UBO
{
    float exposure;
    float gamma;
};

cbuffer ubo : register(b0) { UBO ubo; }

struct FSOutput
{
    float4 color : SV_TARGET0;
};

float3 Uncharted2Tonemap(float3 color)
{
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    float W = 11.2;
    return ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
}

float3 ToneMapping(float3 color)
{
    color = Uncharted2Tonemap(color * ubo.exposure);
    color = color * (1.0f / Uncharted2Tonemap((11.2f).xxx));

    // Gamma correction
    color = pow(color, (1.0f / ubo.gamma).xxx);
    return color;
}
float3 GetColor(float2 uv)
{

    float4 lighting = textureLight.Sample(samplerLight, uv);
    float4 highLight = textureHighLight.Sample(samplerHighLight, uv);
    float3 color = lighting.rgb + highLight.rgb;
    return color;
}

FSOutput main([[vk::location(0)]] float2 inUV : TEXCOORD0)
{
    FSOutput output = (FSOutput)0;

    float3 color = GetColor(inUV.rg);
    color = ToneMapping(color);
    output.color = float4(color, 1.0f);
    return output;
}
