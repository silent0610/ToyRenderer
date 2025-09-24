// MeshToSDFCommon.hlsl - MeshToSDF计算着色器通用定义
// 基于Unity com.unity.demoteam.mesh-to-sdf实现

#ifndef MESH_TO_SDF_COMMON_HLSL
#define MESH_TO_SDF_COMMON_HLSL

#define THREAD_GROUP_SIZE 64

// 推送常量结构 - 所有MeshToSDF着色器共用
struct MeshToSDFConstants {
    float4x4 worldToLocal;
    int4 voxelResolution;      // w = total voxel count
    float maxDistance;
    float initialDistance;
    float offset;
    float padding1;
    float4 origin;
    float cellSize;
    int numCellsX;
    int numCellsY;
    int numCellsZ;
    int indexFormat16bit;
    int vertexBufferStride;
    int vertexBufferPosOffset;
    int jumpOffset;
    int4 jumpOffsetInterleaved;
    int dispatchSizeX;
};

[[vk::push_constant]] MeshToSDFConstants pc;

// 通用缓冲区绑定
RWStructuredBuffer<uint> SignedDistanceField : register(u0);           // 主SDF缓冲区（原子操作用）
StructuredBuffer<float> SdfBuffer : register(t1);        // 只读SDF缓冲区
RWStructuredBuffer<float> SdfBufferRW : register(u2);        // 读写SDF缓冲区（洪水填充用）
ByteAddressBuffer VertexBuffer : register(t3);               // 顶点缓冲区
ByteAddressBuffer IndexBuffer : register(t4);                // 索引缓冲区
RWTexture3D<float> OutputTexture : register(u5);             // 输出3D纹理

// 跳跃洪水算法缓冲区
StructuredBuffer<int> JumpBuffer : register(t6);
RWStructuredBuffer<int> JumpBufferRW : register(u7);


// 常量定义
#define MARGIN pc.cellSize
#define GRID_MARGIN int3(1, 1, 1)
#define SQRT_2 1.41421356
#define SQRT_3 1.73205081

// ==== 通用辅助函数 ====

// 获取索引（支持16位和32位索引）
uint GetIndex(uint i) {
    if (pc.indexFormat16bit) {
        uint entryIndex = i >> 1u;
        uint entryOffset = i & 1u;
        uint read = IndexBuffer.Load(entryIndex << 2);
        return entryOffset == 1u ? ((read >> 16) & 0xffff) : read & 0xffff;
    } else {
        return IndexBuffer.Load(i << 2);
    }
}

// 获取顶点位置
float3 GetPos(uint i) {
    return asfloat(VertexBuffer.Load3(i * pc.vertexBufferStride + pc.vertexBufferPosOffset));
}

// 获取体素索引
// (展开的组索引)*组内线程数 = 起始线程
int GetVoxelIndex(uint GIndex, uint3 GId) {
    return (GId.x + GId.y * pc.dispatchSizeX) * THREAD_GROUP_SIZE + GIndex;
}

// 从索引获取3D坐标
int3 GetLocalCellPositionFromIndex(uint localCellIndex, int3 cellsPerDimensionLocal) {
    uint cellsPerLine = (uint)cellsPerDimensionLocal.x;
    uint cellsPerPlane = (uint)(cellsPerDimensionLocal.x * cellsPerDimensionLocal.y);

    uint numPlanesZ = localCellIndex / cellsPerPlane;
    uint remainder = localCellIndex % cellsPerPlane;

    uint numLinesY = remainder / cellsPerLine;
    uint numCellsX = remainder % cellsPerLine;

    return int3((int)numCellsX, (int)numLinesY, (int)numPlanesZ);
}

// 获取SDF单元格索引
int GetSdfCellIndex(int3 gridPosition) {
    int cellsPerLine = pc.numCellsX;
    int cellsPerPlane = pc.numCellsX * pc.numCellsY;
    return cellsPerPlane * gridPosition.z + cellsPerLine * gridPosition.y + gridPosition.x;
}

// 获取SDF单元格世界位置
float3 GetSdfCellPosition(int3 gridPosition) {
    float3 cellCenter = float3(gridPosition.x, gridPosition.y, gridPosition.z);
    cellCenter += 0.5;
    cellCenter *= pc.cellSize;
    cellCenter += pc.origin.xyz;
    return cellCenter;
}

// 从世界坐标获取SDF坐标
int3 GetSdfCoordinates(float3 positionInWorld) {
    float3 sdfPosition = (positionInWorld - pc.origin.xyz) / pc.cellSize;
    return int3((int)sdfPosition.x, (int)sdfPosition.y, (int)sdfPosition.z);
}

// Float到uint转换（用于原子操作）
uint FloatFlip3(float fl) {
    uint f = asuint(fl);
    return (f << 1) | (f >> 31);
}

// uint到float转换
uint IFloatFlip3(uint f2) {
    return (f2 >> 1) | (f2 << 31);
}

// 从点p到点x0和x1构成的边的距离. 返回距离和一个从线段上最近点指向点 p 的单位方向向量
float DistancePointToEdge(float3 p, float3 x0, float3 x1, out float3 n) {
    if (x0.x > x1.x) {
        float3 temp = x0;
        x0 = x1;
        x1 = temp;
    }
    float3 x10 = x1 - x0;
    // t: p在 直线上投影点的位置
    float t = dot(x1 - p, x10) / dot(x10, x10);
    t = max(0.0f,min(t,1.0f));
    // a: 最近点到p的方向向量
    float3 a = p - (t*x0+(1.0f-t)*x1);
    float d = length(a);
    n = a/(d+1e-30f);
    return d;
}

// Check if p is in the positive or negative side of triangle (x0, x1, x2)
// Positive side is where the normal vector of triangle ( (x1-x0) x (x2-x0) ) is pointing to.
float SignedDistancePointToTriangle(float3 p, float3 x0, float3 x1, float3 x2) {
    float d = 0;
    float3 x02 = x0 - x2;
    float l0 = length(x02) + 1e-30f;
    x02 = x02 / l0;
    float3 x12 = x1 - x2;
    float l1 = dot(x12, x02);
    x12 = x12 - l1*x02;
    float l2 = length(x12) + 1e-30f;
    x12 = x12 / l2;
    float3 px2 = p - x2;

    float b = dot(x12, px2) / l2;
    float a = (dot(x02, px2) - l1*b) / l0;
    float c = 1 - a - b;

    // normal vector of triangle. Don't need to normalize this yet.
    float3 nTri = cross((x1 - x0), (x2 - x0));
    float3 n;

    float tol = 1e-8f;

    if (a >= -tol && b >= -tol && c >= -tol) {
        n = p - (a*x0 + b*x1 + c*x2);
        d = length(n);

        float3 n1 = n / d;
        float3 n2 = nTri / (length(nTri) + 1e-30f);		// if d == 0

        n = (d > 0) ? n1 : n2;
    }
    else {
        float3 n_12;
        float3 n_02;
        d = DistancePointToEdge(p, x0, x1, n);

        float d12 = DistancePointToEdge(p, x1, x2, n_12);
        float d02 = DistancePointToEdge(p, x0, x2, n_02);

        d = min(d, d12);
        d = min(d, d02);

        n = (d == d12) ? n_12 : n;
        n = (d == d02) ? n_02 : n;
    }

#ifdef SIGNED
    d = (dot(p - x0, nTri) < 0.f) ? -d : d;
#endif

    return d;
}

#endif // MESH_TO_SDF_COMMON_HLSL