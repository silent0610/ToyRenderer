// Finalize.Comp.hlsl - MeshToSDF完成处理
// 将用于比较的float值(uint格式)重新转换回float格式.
// 对于同样的32位数据, uint和float的解释方式不同

#include "MeshToSDFCommon.hlsl"

// 完成处理内核 - 每个体素一个线程
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint GIndex : SV_GroupIndex, uint3 GId : SV_GroupID, uint3 DTid : SV_DispatchThreadID) {
    int sdfCellIndex = GetVoxelIndex(GIndex, GId);
    if (sdfCellIndex >= pc.voxelResolution.w)
        return;

    // 将uint格式的距离转换回float格式
    uint distance = SignedDistanceField[sdfCellIndex];
    SignedDistanceField[sdfCellIndex] = IFloatFlip3(distance);
}