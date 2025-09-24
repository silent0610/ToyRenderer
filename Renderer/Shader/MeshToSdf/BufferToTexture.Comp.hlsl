// BufferToTexture.Comp.hlsl - 缓冲区到3D纹理
// 将SDF缓冲区数据写入3D纹理

#include "MeshToSDFCommon.hlsl"

// 缓冲区到纹理 - 每个体素一个线程
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint GIndex : SV_GroupIndex, uint3 GId : SV_GroupID, uint3 DTid : SV_DispatchThreadID) {
    int voxelIndex = GetVoxelIndex(GIndex, GId);
    if (voxelIndex >= pc.voxelResolution.w)
        return;

    // TODO: 实现缓冲区到纹理逻辑
    // 1. 获取体素3D坐标
    // 2. 从SDF缓冲区读取距离值
    // 3. 应用偏移
    // 4. 写入3D纹理

    int3 center = GetLocalCellPositionFromIndex(voxelIndex, pc.voxelResolution.xyz);
    float sdfValue = asfloat(sdfBuffer[voxelIndex]) + pc.offset;

    outputTexture[center] = sdfValue;
}