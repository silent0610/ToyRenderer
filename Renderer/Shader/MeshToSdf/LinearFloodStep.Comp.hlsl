// LinearFloodStep.Comp.hlsl - 线性洪水填充步骤
// 基本线性洪水填充，仅检查6个正交相邻体素

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

// 线性洪水填充步骤 - 每个体素一个线程
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint GIndex : SV_GroupIndex, uint3 GId : SV_GroupID, uint3 DTid : SV_DispatchThreadID) {
    int voxelIndex = GetVoxelIndex(GIndex, GId);
    if (voxelIndex >= pc.voxelResolution.w)
        return;

    // TODO: 实现线性洪水填充逻辑
    // 1. 获取当前体素值
    // 2. 检查是否为种子体素
    // 3. 检查6个正交相邻体素
    // 4. 更新距离值

    float centerValue = GetVoxel(voxelIndex);
    sdfBufferRW[voxelIndex] = centerValue;
}