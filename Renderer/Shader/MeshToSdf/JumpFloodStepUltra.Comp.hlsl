// JumpFloodStepUltra.Comp.hlsl - 高质量跳跃洪水算法步骤
// 高质量跳跃洪水算法，检查27个方向的跳跃

#include "MeshToSDFCommon.hlsl"


// 获取跳跃缓冲区值
int GetVoxelJump(int voxelIndex) {
    return JumpBuffer[voxelIndex];
}

int GetVoxelJump(int3 voxelCoords) {
    voxelCoords = clamp(voxelCoords, 0, pc.voxelResolution.xyz - 1);
    return GetVoxelJump(GetSdfCellIndex(voxelCoords));
}

// 获取体素坐标
int3 GetVoxelCoords(int voxelIndex) {
    return GetLocalCellPositionFromIndex(voxelIndex, pc.voxelResolution.xyz);
}

// 跳跃采样
void JumpSample(int3 centerCoord, int3 offset, inout float bestDistance, inout int bestIndex) {
    // 要搜索的邻居点
    int3 sampleCoord = centerCoord + offset;
    // 邻居点内保存的最近种子点的体素索引
    int voxelSampleIndex = GetVoxelJump(sampleCoord);
    // 邻居点内保存的最近种子点的体素3D坐标, 单位是体素
    int3 voxelSampleCoord = GetVoxelCoords(voxelSampleIndex);
    // 计算当前点到目标种子点体素中心的距离, 取最小者
    float dist = length(centerCoord - voxelSampleCoord);
    if (voxelSampleIndex != 0 && dist < bestDistance)
    {
        bestDistance = dist;
        bestIndex = voxelSampleIndex;
    }
}

// 跳跃洪水步骤 - 每个体素一个线程
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint GIndex : SV_GroupIndex, uint3 GId : SV_GroupID, uint3 DTid : SV_DispatchThreadID) {
    // 当前处理的体素index
    int voxelIndex = GetVoxelIndex(GIndex, GId);
    if (voxelIndex >= pc.voxelResolution.w)
        return;

    // 转换到局部坐标系, 以体素为单位
    int3 centerCoord = GetVoxelCoords(voxelIndex);
    float bestDistance = 100000;
    int bestIndex = 0;

    // Ultra质量：检查3x3x3=27个邻居
    // 使用pc.jumpOffset作为跳跃距离（不使用jumpOffsetInterleaved）
    for (int z = -1; z <= 1; ++z) {
        for (int y = -1; y <= 1; ++y) {
            for (int x = -1; x <= 1; ++x) {
                int3 offset = int3(x, y, z) * pc.jumpOffset;
                JumpSample(centerCoord, offset, bestDistance, bestIndex);
            }
        }
    };

    JumpBufferRW[voxelIndex] = bestIndex;
}