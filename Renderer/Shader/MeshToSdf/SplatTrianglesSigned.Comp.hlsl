// SplatTrianglesSigned.Comp.hlsl - 有符号三角形距离计算
// 计算每个体素到最近三角形的有符号距离
#define SIGNED 
#include "MeshToSDFCommon.hlsl"

// 有符号三角形距离Splatting - 每个三角形一个线程
[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint GIndex : SV_GroupIndex, uint3 GId : SV_GroupID, uint3 DTid : SV_DispatchThreadID) {
    uint triangleIndex = GId.x * THREAD_GROUP_SIZE + GIndex;
    triangleIndex *= 3;

    // TODO: 实现有符号三角形距离计算
    // 1. 获取三角形顶点
    // 2. 变换到SDF空间
    // 3. 计算AABB
    // 4. 遍历影响的体素
    // 5. 计算距离并原子更新

    // 获取三角形的三个顶点
    float3 tri0 = GetPos(GetIndex(triangleIndex + 0));
    float3 tri1 = GetPos(GetIndex(triangleIndex + 1));
    float3 tri2 = GetPos(GetIndex(triangleIndex + 2));

    // 变换到SDF空间(仍然是世界空间, 因为后面cellPosition是世界坐标, 所以tri只能在世界空间下)
    tri0 = mul(pc.worldToLocal, float4(tri0, 1)).xyz;
    tri1 = mul(pc.worldToLocal, float4(tri1, 1)).xyz;
    tri2 = mul(pc.worldToLocal, float4(tri2, 1)).xyz;

    // 计算三角形AABB包围盒（加上边距, 在世界空间）
    float3 aabbMin = min(tri0, min(tri1, tri2)) - float3(MARGIN, MARGIN, MARGIN);
    float3 aabbMax = max(tri0, max(tri1, tri2)) + float3(MARGIN, MARGIN, MARGIN);

    // 将AABB转换为sdf局部坐标, 最小角点为000)
    int3 gridMin = GetSdfCoordinates(aabbMin) - GRID_MARGIN;
    int3 gridMax = GetSdfCoordinates(aabbMax) + GRID_MARGIN;

    // 确保在有效范围内
    gridMin.x = max(0, min(gridMin.x, pc.numCellsX - 1));
    gridMin.y = max(0, min(gridMin.y, pc.numCellsY - 1));
    gridMin.z = max(0, min(gridMin.z, pc.numCellsZ - 1));

    gridMax.x = max(0, min(gridMax.x, pc.numCellsX - 1));
    gridMax.y = max(0, min(gridMax.y, pc.numCellsY - 1));
    gridMax.z = max(0, min(gridMax.z, pc.numCellsZ - 1));

    for (int z = gridMin.z;z<=gridMax.z;++z)
    {
        for(int y = gridMin.y;y<=gridMax.y;++y)
        {
            for(int x = gridMin.x;x<=gridMax.x;++x)
            {
                int3 gridCellCoordinate = int3(x,y,z); // 最小角点为0,0,0. 对应的世界坐标为-1,-1,-1
                int gridCellIndex = GetSdfCellIndex(gridCellCoordinate);
                // cell中心在世界坐标系下的位置. 尺度和世界坐标系相同
                float3 cellPosition = GetSdfCellPosition(gridCellCoordinate);

                float distance = SignedDistancePointToTriangle(cellPosition,tri0,tri1,tri2);
                uint distanceAsUint = FloatFlip3(distance);
                InterlockedMin(SignedDistanceField[gridCellIndex], distanceAsUint);
            }
        }
    }
}