// JumpFloodInitialize.Comp.hlsl - 跳跃洪水算法初始化
// 初始化跳跃洪水算法的种子体素

#include "MeshToSDFCommon.hlsl"

// 获取体素值
float GetVoxel(int voxelIndex) {
    return SdfBuffer[voxelIndex];
}

// 跳跃洪水初始化 - 每个体素一个线程
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint GIndex : SV_GroupIndex, uint3 GId : SV_GroupID, uint3 DTid : SV_DispatchThreadID) {
    int voxelIndex = GetVoxelIndex(GIndex, GId);
    if (voxelIndex >= pc.voxelResolution.w)
        return;

    // TODO: 实现跳跃洪水初始化逻辑
    // 将距离表面较近的体素标记为种子
    float distance = GetVoxel(voxelIndex);
    JumpBufferRW[voxelIndex] = distance > pc.cellSize * SQRT_3 ? 0 : voxelIndex;
}