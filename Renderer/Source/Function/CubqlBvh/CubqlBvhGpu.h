#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CubqlBvhTriangle
{
    float a[3];
    float b[3];
    float c[3];
} CubqlBvhTriangle;

typedef struct CubqlBvhContext CubqlBvhContext;

CubqlBvhContext *CubqlBvh_Create(void);
void CubqlBvh_Destroy(CubqlBvhContext *ctx);

/* Upload triangles and build WideBVH(8) with SAH. Returns 0 on success. */
int CubqlBvh_Build(CubqlBvhContext *ctx, const CubqlBvhTriangle *tris, int count, float *outBuildMs);

/*
 * Fill an N^3 unsigned distance volume on the fixed world grid.
 * Sample centers: origin + (ix+0.5, iy+0.5, iz+0.5) * cellSize when origin is the
 * lower corner of the world box and cellSize = worldSize / N on each axis.
 * outHost must hold N*N*N floats. Returns 0 on success.
 */
int CubqlBvh_FillVolume(CubqlBvhContext *ctx, float worldOriginX, float worldOriginY, float worldOriginZ, float cellSize,
                        int resolution, float *outHost, float *outFillMs);

#ifdef __cplusplus
}
#endif
