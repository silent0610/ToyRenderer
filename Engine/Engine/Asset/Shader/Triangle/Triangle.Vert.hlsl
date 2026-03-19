struct VSInput {
    [[vk::location(0)]] float3 Pos   : POSITION;
    [[vk::location(1)]] float3 Color : COLOR;
    [[vk::location(2)]] float2 UV    : TEXCOORD0; 
    [[vk::location(3)]] float4 Tangent : TANGENT0;
};

struct VSOutput {
    float4 Pos   : SV_POSITION;
    float4 Color : COLOR0;
    float2 UV    : TEXCOORD0; 
};

struct PushConstants {
    float4x4  Model;
    float4x4 mvp;
    float4 color;
};
[[vk::push_constant]] PushConstants pc;

VSOutput main(VSInput input) {
    VSOutput output;
    
    // 1. 位置变换
    output.Pos = mul(pc.mvp, float4(input.Pos, 1.0));
    
    // 2. 颜色传递
    output.Color = float4(input.Color, 1.0) * pc.color;
    
    // 3. UV 透传 (不再使用硬编码数组)
    output.UV = input.UV;
    
    return output;
}