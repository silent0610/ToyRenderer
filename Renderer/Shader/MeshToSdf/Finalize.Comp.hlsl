// Finalize.Comp.hlsl - MeshToSDF完成处理
// 完成SDF距离计算的后处理

#include "MeshToSDFCommon.hlsl"

// 完成处理内核 - 每个体素一个线程
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint GIndex : SV_GroupIndex, uint3 GId : SV_GroupID, uint3 DTid : SV_DispatchThreadID) {
    int sdfCellIndex = GetVoxelIndex(GIndex, GId);
    if (sdfCellIndex >= pc.voxelResolution.w)
        return;

    // TODO: 实现完成处理逻辑
    // 将uint格式的距离转换回float格式
    uint distance = sdfBuffer[sdfCellIndex];
    sdfBuffer[sdfCellIndex] = IFloatFlip3(distance);
}