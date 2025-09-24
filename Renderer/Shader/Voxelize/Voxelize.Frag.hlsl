// Voxelize.Frag.hlsl
// Stage: Pixel Shader - 保守光栅化版本

// Shared constants for the voxelization passes
struct VoxelConstants {
    float4x4 model;
    float4x4 view;
    float4x4 projection;
    uint3 voxelGridSize;
};
ConstantBuffer<VoxelConstants> cb : register(b1);

// UAV to store the winding number counter for each voxel
RWTexture3D<int> VoxelCounter : register(u0);

struct PS_INPUT {
    float4 position : SV_POSITION;
};

// main entry point for the pixel shader - 保守光栅化版本
void main(PS_INPUT input, bool isFrontFace : SV_IsFrontFace) {
    uint3 voxelCoord;

    // X轴直接映射
    voxelCoord.x = uint(input.position.x);

    // Y轴翻转，以匹配“Y轴向上”的常规坐标系
    voxelCoord.y = uint(input.position.y);

    // Z轴缩放并翻转，以匹配“近处为高索引”的直觉
    float z_depth = input.position.z; // 获取 [0, 1] 的深度值
    voxelCoord.z = uint(z_depth * (cb.voxelGridSize.z));

    // 2. 安全检查：确保计算出的坐标不会超出纹理边界
    voxelCoord = min(voxelCoord, cb.voxelGridSize - 1);

    // 3. 根据正面/背面信息执行原子操作（这部分逻辑不变）
    int value = isFrontFace ? 1 : -1;
    InterlockedAdd(VoxelCounter[voxelCoord], value);
}

