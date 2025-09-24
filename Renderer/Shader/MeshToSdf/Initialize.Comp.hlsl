// Initialize.Comp.hlsl - MeshToSDF初始化计算着色器
// 初始化SDF缓冲区为最大距离值

#include "MeshToSDFCommon.hlsl"

// 初始化内核 - 每个体素一个线程
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint GIndex : SV_GroupIndex, uint3 GId : SV_GroupID, uint3 DTid : SV_DispatchThreadID) {
    int sdfCellIndex = GetVoxelIndex(GIndex, GId);
    if (sdfCellIndex >= pc.voxelResolution.w)
        return;

    // TODO: 实现初始化逻辑
    // 将SDF缓冲区初始化为初始距离值
    sdfBuffer[sdfCellIndex] = FloatFlip3(pc.initialDistance);
}