struct VSInput
{
    [[vk::location(0)]] float3 Pos : POSITION;
    [[vk::location(1)]] float2 UV : TEXCOORD0;
    [[vk::location(2)]] float3 Color : COLOR0;
    [[vk::location(3)]] float3 Normal : NORMAL0;
};

struct UBO
{
    float4x4 view;
    float4x4 proj;
    float3 lightPos;
    float3 camPos;
};

cbuffer ubo : register(b0) { UBO ubo; }

struct PushConsts
{
    float4x4 Model;
};

[[vk::push_constant]] PushConsts primitive;

struct VSOutput
{
    float4 Pos : SV_POSITION;
    [[vk::location(0)]] float3 Normal : NORMAL0;
    [[vk::location(1)]] float3 Color : COLOR0;
    [[vk::location(2)]] float3 ViewVec : TEXCOORD1;
    [[vk::location(3)]] float3 LightVec : TEXCOORD2;
};

VSOutput main(VSInput input)
{
    VSOutput output = (VSOutput)0;
    output.Color = input.Color;
    output.Pos = mul(ubo.proj, mul(ubo.view, mul(primitive.Model, float4(input.Pos.xyz, 1.0))));
    output.Normal = normalize(mul((float3x3)primitive.Model, input.Normal));
    float3 modelWorldPos = mul((float3x3)primitive.Model, input.Pos);

    output.LightVec = normalize(ubo.lightPos.xyz - modelWorldPos);
    output.ViewVec = normalize(ubo.camPos.xyz - modelWorldPos);
    return output;
}
