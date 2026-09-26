#define CUBQL_GPU_BUILDER_IMPLEMENTATION 1

#include "CubqlBvhGpu.h"

#include "cuBQL/bvh.h"
#include "cuBQL/queries/triangleData/math/pointToTriangleDistance.h"
#include "cuBQL/traversal/shrinkingRadiusQuery.h"

#include <cstdio>
#include <vector>

using cuBQL::box3f;
using cuBQL::divRoundUp;
using cuBQL::Triangle;
using cuBQL::vec3f;
using cuBQL::vec3i;

enum { kBvhWidth = 8 };
using WideBvh = cuBQL::WideBVH<float, 3, kBvhWidth>;

namespace {

struct GpuTimer
{
    cudaEvent_t start = nullptr;
    cudaEvent_t stop = nullptr;

    GpuTimer()
    {
        CUBQL_CUDA_CALL(EventCreate(&start));
        CUBQL_CUDA_CALL(EventCreate(&stop));
    }
    ~GpuTimer()
    {
        if (start)
            cudaEventDestroy(start);
        if (stop)
            cudaEventDestroy(stop);
    }
    void begin(cudaStream_t s = 0)
    {
        CUBQL_CUDA_CALL(EventRecord(start, s));
    }
    float endMs(cudaStream_t s = 0)
    {
        CUBQL_CUDA_CALL(EventRecord(stop, s));
        CUBQL_CUDA_CALL(EventSynchronize(stop));
        float ms = 0.f;
        CUBQL_CUDA_CALL(EventElapsedTime(&ms, start, stop));
        return ms;
    }
};

template <typename T>
T *deviceAlloc(size_t n)
{
    T *ptr = nullptr;
    CUBQL_CUDA_CALL(Malloc((void **)&ptr, n * sizeof(T)));
    return ptr;
}

__global__ void generateBoxes(box3f *boxes, const Triangle *tris, int n)
{
    const int tid = int(threadIdx.x + blockIdx.x * blockDim.x);
    if (tid >= n)
        return;
    boxes[tid] = tris[tid].bounds();
}

__device__ inline float closestSqrOnMesh(const Triangle *tris, WideBvh bvh, vec3f P)
{
    cuBQL::triangles::PointToTriangleTestResult best;
    auto perPrim = [tris, P, &best](uint32_t primID) -> float {
        cuBQL::triangles::computeClosestPoint(best, tris[primID], P);
        return best.sqrDist;
    };
    cuBQL::shrinkingRadiusQuery::forEachPrim(perPrim, bvh, P, CUBQL_INF);
    return best.sqrDist;
}

__global__ void computeSdfKernel(float *out, WideBvh bvh, const Triangle *tris, vec3i dims, vec3f origin, vec3f cellSize,
                                 int numCells)
{
    const int tid = int(threadIdx.x + blockIdx.x * blockDim.x);
    if (tid >= numCells)
        return;

    // Storage index (ix,iy,iz) matches BruteForce/JFA: sample world at (ix, Ny-1-iy, Nz-1-iz).
    const int ix = tid % dims.x;
    const int iy = (tid / dims.x) % dims.y;
    const int iz = tid / (dims.x * dims.y);
    const int sy = dims.y - 1 - iy;
    const int sz = dims.z - 1 - iz;
    const vec3f P = origin + vec3f(float(ix), float(sy), float(sz)) * cellSize;
    out[tid] = sqrtf(closestSqrOnMesh(tris, bvh, P));
}

} // namespace

struct CubqlBvhContext
{
    Triangle *d_tris = nullptr;
    box3f *d_boxes = nullptr;
    float *d_sdf = nullptr;
    WideBvh bvh{};
    int numTris = 0;
    int allocatedTris = 0;
    int allocatedCells = 0;
    bool bvhValid = false;
};

CubqlBvhContext *CubqlBvh_Create(void)
{
    int gpuCount = 0;
    if (cudaGetDeviceCount(&gpuCount) != cudaSuccess || gpuCount <= 0)
    {
        std::fprintf(stderr, "CubqlBvh: no CUDA device\n");
        return nullptr;
    }
    if (cudaSetDevice(0) != cudaSuccess)
    {
        std::fprintf(stderr, "CubqlBvh: cudaSetDevice(0) failed\n");
        return nullptr;
    }
    return new CubqlBvhContext();
}

void CubqlBvh_Destroy(CubqlBvhContext *ctx)
{
    if (!ctx)
        return;
    if (ctx->bvhValid)
    {
        cuBQL::cuda::free(ctx->bvh);
        ctx->bvhValid = false;
    }
    if (ctx->d_sdf)
        cudaFree(ctx->d_sdf);
    if (ctx->d_boxes)
        cudaFree(ctx->d_boxes);
    if (ctx->d_tris)
        cudaFree(ctx->d_tris);
    delete ctx;
}

int CubqlBvh_Build(CubqlBvhContext *ctx, const CubqlBvhTriangle *tris, int count, float *outBuildMs)
{
    if (!ctx || !tris || count <= 0)
        return 1;

    if (ctx->bvhValid)
    {
        cuBQL::cuda::free(ctx->bvh);
        ctx->bvh = {};
        ctx->bvhValid = false;
    }

    if (count > ctx->allocatedTris)
    {
        if (ctx->d_tris)
            cudaFree(ctx->d_tris);
        if (ctx->d_boxes)
            cudaFree(ctx->d_boxes);
        ctx->d_tris = deviceAlloc<Triangle>(size_t(count));
        ctx->d_boxes = deviceAlloc<box3f>(size_t(count));
        ctx->allocatedTris = count;
    }
    ctx->numTris = count;

    static_assert(sizeof(CubqlBvhTriangle) == sizeof(Triangle), "triangle layout mismatch");

    GpuTimer timer;
    timer.begin();
    CUBQL_CUDA_CALL(Memcpy(ctx->d_tris, tris, size_t(count) * sizeof(Triangle), cudaMemcpyHostToDevice));
    generateBoxes<<<divRoundUp(count, 256), 256>>>(ctx->d_boxes, ctx->d_tris, count);
    CUBQL_CUDA_SYNC_CHECK();

    cuBQL::BuildConfig cfg;
    cfg.makeLeafThreshold = 4;
    cfg.enableSAH();
    cuBQL::gpuBuilder(ctx->bvh, ctx->d_boxes, uint32_t(count), cfg);
    CUBQL_CUDA_SYNC_CHECK();
    const float buildMs = timer.endMs();
    ctx->bvhValid = true;

    if (outBuildMs)
        *outBuildMs = buildMs;
    return 0;
}

int CubqlBvh_FillVolume(CubqlBvhContext *ctx, float worldOriginX, float worldOriginY, float worldOriginZ, float cellSize,
                        int resolution, float *outHost, float *outFillMs)
{
    if (!ctx || !ctx->bvhValid || !outHost || resolution < 2 || cellSize <= 0.f)
        return 1;

    const vec3i dims(resolution, resolution, resolution);
    const int numCells = dims.x * dims.y * dims.z;
    if (numCells > ctx->allocatedCells)
    {
        if (ctx->d_sdf)
            cudaFree(ctx->d_sdf);
        ctx->d_sdf = deviceAlloc<float>(size_t(numCells));
        ctx->allocatedCells = numCells;
    }

    // Voxel centers: lowerCorner + (i + 0.5) * cellSize
    const vec3f origin = vec3f(worldOriginX, worldOriginY, worldOriginZ) + vec3f(0.5f * cellSize);
    const vec3f cell = vec3f(cellSize);

    constexpr int kThreads = 256;
    const int blocks = divRoundUp(numCells, kThreads);
    GpuTimer timer;
    timer.begin();
    computeSdfKernel<<<blocks, kThreads>>>(ctx->d_sdf, ctx->bvh, ctx->d_tris, dims, origin, cell, numCells);
    CUBQL_CUDA_SYNC_CHECK();
    const float fillMs = timer.endMs();

    CUBQL_CUDA_CALL(Memcpy(outHost, ctx->d_sdf, size_t(numCells) * sizeof(float), cudaMemcpyDeviceToHost));

    if (outFillMs)
        *outFillMs = fillMs;
    return 0;
}
