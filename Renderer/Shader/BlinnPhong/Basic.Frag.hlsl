struct VSOutput
{
    [[vk::location(0)]] float2 UV : TEXCOORD0;
    [[vk::location(1)]] float3 Normal : NORMAL0;
    [[vk::location(2)]] float3 Color : COLOR0;
    [[vk::location(3)]] float3 ViewVec : TEXCOORD1;
    [[vk::location(4)]] float3 LightVec : TEXCOORD2;
};

Texture2D textureColorMap : register(t0, space1);
SamplerState samplerColorMap : register(s0, space1);

#define ambientIntensity 0.1
static const float3 lightColor = float3(1.0, 1.0, 1.0);
float4 main(VSOutput input) : SV_TARGET
{
    float4 color = textureColorMap.Sample(samplerColorMap, input.UV);
    float3 N = normalize(input.Normal);
    float3 L = normalize(input.LightVec);
    float3 V = normalize(input.ViewVec);

    float3 halfwayDir = normalize(L + V); // 计算半角向量
    // float3 R = normalize(-reflect(L, N));

    float3 ambient = ambientIntensity * input.Color;
    float3 diffuse = lightColor * input.Color * max(0, dot(N, L));
    float3 specular = pow(max(dot(N, halfwayDir), 0.0f), 16.0f) * lightColor;
    return color;
}
