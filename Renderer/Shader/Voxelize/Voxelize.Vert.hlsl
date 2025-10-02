// Voxelize.Vert.hlsl

// Shared constants for the voxelization passes
struct VoxelConstants {
    float4x4 model;
    float4x4 view;
    float4x4 projection;
    uint3 voxelGridSize;
};
ConstantBuffer<VoxelConstants> cb : register(b1);

struct VS_INPUT {
    float3 position : POSITION;
    float3 normal : NORMAL;
};

struct VS_OUTPUT {
    float4 position : SV_POSITION;
};

// main entry point for the vertex shader - 保守光栅化版本
VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT output;

    // Transform vertex position to world space
    float4 worldPos = mul(cb.model, float4(input.position, 1.0));

    // // Transform normal to world space
    // float4 worldNormal = mul(cb.model, float4(input.normal, 0.0));
    // output.worldNormal = normalize(worldNormal.xyz);

    // Transform vertex position to clip space
    output.position = mul(cb.projection, mul(cb.view, worldPos));

    return output;
}
