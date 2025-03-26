struct VSInput
{
    [[vk::location(0)]] float3 Pos : POSITION;
    [[vk::location(1)]] float3 Normal : NORMAL;
    [[vk::location(2)]] float2 UV : TEXCOORD;
    [[vk::location(3)]] float3 Color : COLOR;
};

struct UBO
{
    float4x4 View;
    float4x4 Projection;
    float4 LightPos;
    float4 ViewPos;
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
    [[vk::location(0)]] float3 Normal : NORMAL;
    [[vk::location(1)]] float3 Color : COLOR;
    [[vk::location(2)]] float2 UV : TEXCOORD0;
    [[vk::location(3)]] float3 ViewVec : TEXCOORD1;
    [[vk::location(4)]] float3 LightVec : TEXCOORD2;
};

VSOutput main(VSInput input)
{
    VSOutput output = (VSOutput)0;
    // output.Normal = input.Normal;
    output.Color = input.Color;
    output.UV = input.UV;
    output.Pos = mul(ubo.Projection, mul(ubo.View, mul(primitive.Model, float4(input.Pos.xyz, 1.0)))); // 裁剪坐标

    float4 pos = mul(primitive.Model, float4(input.Pos, 1.0));
    output.Normal = mul((float3x3)primitive.Model, input.Normal);
    output.LightVec = ubo.LightPos.xyz - pos.xyz;
    output.ViewVec = ubo.ViewPos.xyz - pos.xyz;
    return output;
}