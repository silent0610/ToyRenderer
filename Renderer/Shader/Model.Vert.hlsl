// Simple model vertex shader for glTF rendering
struct VSInput
{
    [[vk::location(0)]] float3 Pos : POSITION0;
    [[vk::location(1)]] float3 Normal : NORMAL0;
    [[vk::location(2)]] float2 UV : TEXCOORD0;
    [[vk::location(3)]] float4 Color : COLOR0;
    [[vk::location(4)]] float4 Joint0 : TEXCOORD1;
    [[vk::location(5)]] float4 Weight0 : TEXCOORD2;
    [[vk::location(6)]] float4 Tangent : TEXCOORD3;
};

struct MVPUniformData
{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float4x4 mvp;
};

cbuffer mvpUbo : register(b0) { MVPUniformData mvpUbo; }

struct VSOutput
{
    float4 Pos : SV_POSITION;
    [[vk::location(0)]] float3 Normal : NORMAL0;
    [[vk::location(1)]] float2 UV : TEXCOORD0;
    [[vk::location(2)]] float4 Color : COLOR0;
    [[vk::location(3)]] float3 WorldPos : POSITION0;
};

VSOutput main(VSInput input)
{
    VSOutput output = (VSOutput)0;

    // Transform vertex position using pre-computed MVP matrix
    output.Pos = mul(mvpUbo.mvp, float4(input.Pos, 1.0));

    // Pass through UV and color
    output.UV = input.UV;
    output.Color = input.Color;

    // Transform normal to world space (simplified - using model matrix)
    output.Normal = normalize(mul((float3x3)mvpUbo.model, input.Normal));
    
    // World position
    output.WorldPos = mul(mvpUbo.model, float4(input.Pos, 1.0)).xyz;

    return output;
}