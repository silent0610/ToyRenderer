// JumpFloodFinalize.Comp.hlsl - 跳跃洪水算法完成
// 根据跳跃洪水结果计算最终距离

#include "MeshToSDFCommon.hlsl"

// 获取跳跃缓冲区值
int GetVoxelJump(int voxelIndex) {
    return JumpBuffer[voxelIndex];
}

// 获取体素值
float GetVoxel(int voxelIndex) {
    return SdfBuffer[voxelIndex];
}

// 获取体素坐标
int3 GetVoxelCoords(int voxelIndex) {
    return GetLocalCellPositionFromIndex(voxelIndex, pc.voxelResolution.xyz);
}

// 跳跃洪水完成 - 每个体素一个线程
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint GIndex : SV_GroupIndex, uint3 GId : SV_GroupID, uint3 DTid : SV_DispatchThreadID) {
    int voxelIndex = GetVoxelIndex(GIndex, GId);
    if (voxelIndex >= pc.voxelResolution.w)
        return;

    int closestSeedVoxelIndex = GetVoxelJump(voxelIndex);
    float distanceToClosestSeedVoxel = length(GetVoxelCoords(voxelIndex) - GetVoxelCoords(closestSeedVoxelIndex)) * pc.cellSize;
    float distanceOfClosestSeedVoxelToSurface = GetVoxel(closestSeedVoxelIndex);
    
    // Jump Flood通常用于无符号距离场，所以直接相加
    // 对于有符号距离场，这里的逻辑可能需要调整符号
    SdfBufferRW[voxelIndex] = distanceToClosestSeedVoxel + distanceOfClosestSeedVoxelToSurface;
}