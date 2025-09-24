// JumpFloodFinalize.Comp.hlsl - 跳跃洪水算法完成
// 根据跳跃洪水结果计算最终距离

#include "MeshToSDFCommon.hlsl"

// 获取跳跃缓冲区值
int GetVoxelJump(int voxelIndex) {
    return jumpBufferRead[voxelIndex];
}

// 获取体素值
float GetVoxel(int voxelIndex) {
    return sdfBufferRead[voxelIndex];
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

    // TODO: 实现跳跃洪水完成逻辑
    // 1. 获取最近种子体素索引
    // 2. 计算到种子体素的距离
    // 3. 加上种子体素到表面的距离
    // 4. 更新SDF缓冲区

    sdfBufferRW[voxelIndex] = 0.0f;
}