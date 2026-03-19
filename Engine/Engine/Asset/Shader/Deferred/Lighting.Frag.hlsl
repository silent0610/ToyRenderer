// lighting.frag.hlsl

struct VSOutput
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
};

// 对应 C++: LightingPass::PushConstants
struct PushConsts
{
    float4 CameraPos;
    float4 LightDir;   // 指向光的方向 (如 1, 1, 1) 或者光线方向 (如 -1, -1, -1)，需约定一致
    float4 LightColor;
};
[[vk::push_constant]] PushConsts pConst;

// =========================================================
// Bindings (Set 0: GBuffer Input)
// 对应 C++: LightingPass::Init 中的 Layout
// =========================================================

[[vk::binding(0, 0)]] Texture2D tGBufferA; // Albedo + Metal
[[vk::binding(0, 0)]] SamplerState sSampler; // 共用采样器

[[vk::binding(1, 0)]] Texture2D tGBufferB; // Normal + Rough
[[vk::binding(2, 0)]] Texture2D tGBufferC; // Position
[[vk::binding(3, 0)]] Texture2D tDepth;    // Depth

float4 main(VSOutput input) : SV_Target
{
    // 1. 采样 GBuffer
    // 注意：使用同一个 SamplerState 对所有纹理进行采样
    float4 dataA = tGBufferA.Sample(sSampler, input.UV);
    float4 dataB = tGBufferB.Sample(sSampler, input.UV);
    float4 dataC = tGBufferC.Sample(sSampler, input.UV);
    
    // 2. 解包数据
    float3 albedo    = dataA.rgb;
    return float4(albedo, 1.0);
    float  metallic  = dataA.a;
    
    float3 worldNormal = normalize(dataB.rgb); 
    float  roughness   = dataB.a;
    
    float3 worldPos = dataC.rgb;

    // 简单检查：如果是背景（例如 Position 极小或者是空的），这里假设 Alpha 0 或 Position 0 为背景
    // 实际项目中通常使用 Depth 缓冲区判断是否是天空盒
    if (dot(worldPos, worldPos) == 0.0) 
    {
        return float4(0.05, 0.05, 0.05, 1.0); // 返回背景色
    }

    // 3. 计算光照 (Blinn-Phong 近似 PBR)
    float3 N = worldNormal;
    float3 V = normalize(pConst.CameraPos.xyz - worldPos);
    float3 L = normalize(-pConst.LightDir.xyz); // 假设传入的是光线方向(如-1,-1,-1)，取反指向光源
    float3 H = normalize(V + L);

    // 基础点积
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);

    // Diffuse
    float3 diffuse = albedo * NdotL;

    // Specular
    float specPower = max(1.0, (1.0 - roughness) * 64.0);
    float specIntensity = pow(NdotH, specPower);
    
    // 金属流：金属的 Specular 颜色是 Albedo，非金属是 0.04
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float3 specular = F0 * specIntensity;

    // 简单组合 (非物理精确，但足够看)
    // 金属度越高，漫反射越少
    float3 finalColor = (diffuse * (1.0 - metallic)) + specular;
    
    // 乘光强
    finalColor *= pConst.LightColor.rgb;

    // 加上微弱环境光
    finalColor += albedo * 0.03;

    return float4(albedo, 1.0);
}