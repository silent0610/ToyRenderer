// geometry.frag.hlsl

struct VSOutput
{
    float4 Pos          : SV_POSITION;
    float3 WorldPos     : POSITION0;
    float3 Normal       : NORMAL;
    float2 UV           : TEXCOORD0;
    float3 Tangent      : TANGENT;
    float3 Binormal     : BINORMAL;
};

struct PSOutput
{
    float4 GBufferA : SV_Target0; // Albedo (RGB) + Metallic (A)
    float4 GBufferB : SV_Target1; // Normal (RGB) + Roughness (A)
    float4 GBufferC : SV_Target2; // Position (RGB)
};

// =========================================================
// Bindings (Set 0: Material)
// 对应 C++: BakeMaterials
// 注意：在 Vulkan HLSL 中，Texture 和 Sampler 指定相同的 binding index 
// 即可对应 C++ 里的 CombinedImageSampler
// =========================================================

// Binding 0: Albedo Map
[[vk::binding(0, 0)]] Texture2D tAlbedo;
[[vk::binding(0, 0)]] SamplerState sAlbedo;

// Binding 1: Normal Map
[[vk::binding(1, 0)]] Texture2D tNormal;
[[vk::binding(1, 0)]] SamplerState sNormal;

// Binding 2: Metallic/Roughness Map (GLTF: B=Metal, G=Rough)
[[vk::binding(2, 0)]] Texture2D tPBR;
[[vk::binding(2, 0)]] SamplerState sPBR;

PSOutput main(VSOutput input)
{
    PSOutput output;

    // 1. 采样材质
    float4 albedo = tAlbedo.Sample(sAlbedo, input.UV);
    
    // Alpha Test (如果需要)
    if (albedo.a < 0.1) discard;

    // PBR 数据采样
    float4 pbrInfo = tPBR.Sample(sPBR, input.UV);
    // 假设标准 GLTF 流程: G=Roughness, B=Metallic
    float roughness = pbrInfo.g;
    float metallic = pbrInfo.b;

    // 2. 计算世界空间法线 (Normal Mapping)
    float3 normalMap = tNormal.Sample(sNormal, input.UV).rgb;
    normalMap = normalMap * 2.0 - 1.0; // [0,1] -> [-1,1]

    float3 N = normalize(input.Normal);
    float3 T = normalize(input.Tangent);
    float3 B = normalize(input.Binormal);
    float3x3 TBN = float3x3(T, B, N);
    
    float3 worldNormal = normalize(mul(normalMap, TBN));

    // 3. 写入 GBuffer
    
    // Target 0: Albedo + Metallic
    output.GBufferA = float4(albedo.rgb, metallic);
    
    // Target 1: World Normal + Roughness
    // 注意：Normal 范围是 [-1, 1]，如果 RT 格式是 UNORM 需要 *0.5+0.5
    // 但你在 C++ 选了 R16G16B16A16_FLOAT，所以直接存 [-1, 1] 是安全的且精度更高
    output.GBufferB = float4(worldNormal, roughness);
    
    // Target 2: World Position
    output.GBufferC = float4(input.WorldPos, 1.0);

    return output;
}