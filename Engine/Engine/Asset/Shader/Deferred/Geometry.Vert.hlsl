// geometry.vert.hlsl

// 对应 C++: InputElement posElement(0), colorElement(1/Normal), uvElement(2), tangentElement(3)
struct VSInput
{
    [[vk::location(0)]] float3 Pos      : POSITION;
    [[vk::location(1)]] float3 Normal   : NORMAL;
    [[vk::location(2)]] float2 UV       : TEXCOORD0;
    [[vk::location(3)]] float4 Tangent  : TANGENT;
};

struct VSOutput
{
    float4 Pos          : SV_POSITION; // 裁剪空间坐标
    float3 WorldPos     : POSITION0;   // 世界空间坐标
    float3 Normal       : NORMAL;      // 世界空间法线
    float2 UV           : TEXCOORD0;
    float3 Tangent      : TANGENT;     // 世界空间切线
    float3 Binormal     : BINORMAL;    // 世界空间副切线
};

// 对应 C++: GeometryPass::PushConstants
struct PushConsts
{
    float4x4 Model;
    float4x4 MVP;
};
[[vk::push_constant]] PushConsts pConst;

VSOutput main(VSInput input)
{
    VSOutput output;

    // 1. 计算世界坐标
    float4 worldPos = mul(pConst.Model, float4(input.Pos, 1.0));
    output.WorldPos = worldPos.xyz;

    // 2. 计算裁剪空间坐标 (SV_POSITION)
    output.Pos = mul(pConst.MVP, float4(input.Pos, 1.0));

    // 3. 传递 UV
    output.UV = input.UV;

    // 4. 计算 TBN (切线空间)
    // 注意：这里简单取 Model 矩阵旋转部分，若有非均匀缩放需使用逆转置矩阵
    float3x3 normalMatrix = (float3x3)pConst.Model;
    
    output.Normal   = normalize(mul(normalMatrix, input.Normal));
    output.Tangent  = normalize(mul(normalMatrix, input.Tangent.xyz));
    
    // 计算副切线 (Binormal) = Cross(N, T) * Tangent.w (Handedness)
    output.Binormal = cross(output.Normal, output.Tangent) * input.Tangent.w;

    return output;
}