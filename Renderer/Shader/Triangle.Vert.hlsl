struct VSInput
{
    [[vk::location(0)]] float3 pos : POSITIONT;
    // [[vk::location(1)]] float2 uv : TEXCOORD;
    [[vk::location(1)]] float3 color : COLOR;
};
struct UBO
{
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
};

cbuffer ubo : register(b0)
{
    UBO ubo;
}

struct VSOutput
{
    float4 pos : SV_Position;
    [[vk::location(0)]] float3 color : COLOR;
};

struct PushConsts
{
    float4x4 modelMatrix;
};
[[vk::push_constant]] PushConsts pushConsts;

VSOutput main(VSInput input)
{
    VSOutput output = (VSOutput) 0;
    output.color = input.color;
    float4 worldPos = mul(pushConsts.modelMatrix, float4(input.pos.xyz, 1.0));
    output.pos = mul(ubo.projectionMatrix, mul(ubo.viewMatrix, worldPos));
    return output;
}