// LinearFloodStepUltra.Comp.hlsl - 高质量线性洪水填充
// 高质量线性洪水填充，检查26个相邻体素（包括对角线）

#include "MeshToSDFCommon.hlsl"

// 获取体素值
float GetVoxel(int voxelIndex) {
    return sdfBufferRead[voxelIndex];
}

float GetVoxel(int3 voxelCoords) {
    voxelCoords = clamp(voxelCoords, 0, pc.voxelResolution.xyz - 1);
    int voxelIndex = GetSdfCellIndex(voxelCoords);
    return GetVoxel(voxelIndex);
}

// 获取体素坐标
int3 GetVoxelCoords(int voxelIndex) {
    return GetLocalCellPositionFromIndex(voxelIndex, pc.voxelResolution.xyz);
}

// 最小距离计算
float MinDist(float currentDist, float adjacentDist, float step) {
    // TODO: 实现最小距离计算逻辑
    return currentDist;
}

// 高质量线性洪水填充步骤 - 每个体素一个线程
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint GIndex : SV_GroupIndex, uint3 GId : SV_GroupID, uint3 DTid : SV_DispatchThreadID) {
    int voxelIndex = GetVoxelIndex(GIndex, GId);
    if (voxelIndex >= pc.voxelResolution.w)
        return;

    // TODO: 实现高质量线性洪水填充逻辑
    // 1. 获取当前体素值
    // 2. 检查是否为种子体素
    // 3. 检查6个正交相邻体素
    // 4. 检查20个对角相邻体素
    // 5. 检查8个对角体素
    // 6. 更新距离值

    float centerValue = GetVoxel(voxelIndex);
    sdfBufferRW[voxelIndex] = centerValue;
}