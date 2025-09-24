// LinearFloodStep.Comp.hlsl - 线性洪水填充步骤
// 基本线性洪水填充，仅检查6个正交相邻体素

#include "MeshToSDFCommon.hlsl"

// 获取体素值(种子点到表面距离)
float GetVoxel(int voxelIndex) {
    return SdfBuffer[voxelIndex];
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
float MinDist(float currentDist, float adjacentDist, float step)
{
    if (adjacentDist < 0.0)
        step *= -1;

    // We're one more cell away now
    adjacentDist += step;

    float dist = 0;
    if (adjacentDist > pc.maxDistance)
        // ignore invalid adjacent
        dist = currentDist;
    else if (currentDist > pc.maxDistance)
        // ignore invalid current
        dist = adjacentDist;
    else
        // pick closer to 0
        dist = currentDist < 0 ? max(adjacentDist, currentDist) : min(adjacentDist, currentDist);

    return dist;
}

// 线性洪水填充步骤 - 每个体素一个线程
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint GIndex : SV_GroupIndex, uint3 GId : SV_GroupID, uint3 DTid : SV_DispatchThreadID) {
    int voxelIndex = GetVoxelIndex(GIndex, GId);
    if (voxelIndex >= pc.voxelResolution.w)
        return;

    // 从初始化中读取
    float centerValue = GetVoxel(voxelIndex);
    // 如果当前体素的的值(到最近表面距离)小于一个体素, 视其为种子点, 保存到最终buffer中,并直接返回,
    if(abs(centerValue) < pc.cellSize * SQRT_3)
    {
        SdfBufferRW[voxelIndex] = centerValue; 
        return;
    }
    // 体素坐标(单位为体素)
    int3 center = GetVoxelCoords(voxelIndex);
    int3 offset = int3(-1, 0, 1);

    float minDist = centerValue;
    // 6 orthogonally adjacent voxels
    // 前后左右上下
    float step = pc.cellSize;
    minDist = MinDist(minDist, GetVoxel(center + offset.zyy), step);
    minDist = MinDist(minDist, GetVoxel(center + offset.yzy), step);
    minDist = MinDist(minDist, GetVoxel(center + offset.yyz), step);
    minDist = MinDist(minDist, GetVoxel(center + offset.xyy), step);
    minDist = MinDist(minDist, GetVoxel(center + offset.yxy), step);
    minDist = MinDist(minDist, GetVoxel(center + offset.yyx), step);
    SdfBufferRW[voxelIndex] = minDist;
}