struct Vertex {
    float3 pos;
};

StructuredBuffer<Vertex> vertices : register(t0);
StructuredBuffer<uint> indices : register(t1);

RWTexture3D<uint> voxelGrid : register(u0);

cbuffer VoxelizationParams : register(b0) {
    uint3 voxelGridResolution;
    float voxelSize;
    matrix worldMatrix;
};

// Helper function to project a triangle onto an axis
void project_triangle(float3 axis, float3 v0, float3 v1, float3 v2, out float min_proj, out float max_proj)
{
    float p0 = dot(axis, v0);
    float p1 = dot(axis, v1);
    float p2 = dot(axis, v2);
    min_proj = min(p0, min(p1, p2));
    max_proj = max(p0, max(p1, p2));
}

// SAT (Separating Axis Theorem) test for AABB-Triangle overlap
bool SATTriangleOverlap(float3 boxCenter, float3 boxHalfSize, float3 v0, float3 v1, float3 v2)
{
    // Translate triangle vertices to box's local space
    float3 tv0 = v0 - boxCenter;
    float3 tv1 = v1 - boxCenter;
    float3 tv2 = v2 - boxCenter;

    // Triangle edge vectors
    float3 e0 = tv1 - tv0;
    float3 e1 = tv2 - tv1;
    float3 e2 = tv0 - tv2;

    // AABB face normals (world axes)
    float3 box_axes[3] = {
        float3(1, 0, 0),
        float3(0, 1, 0),
        float3(0, 0, 1)
    };

    // --- Test AABB face normals ---
    for (int i = 0; i < 3; i++) {
        float r = boxHalfSize[i];
        float p0 = tv0[i];
        float p1 = tv1[i];
        float p2 = tv2[i];
        if (max(max(p0, p1), p2) < -r || min(min(p0, p1), p2) > r) {
            return false; // Separating axis found
        }
    }

    // --- Test triangle normal ---
    float3 tri_normal = cross(e0, e1);
    if (dot(tri_normal, tri_normal) > 0.00001f) { // Check for degenerate triangle
        float min_tri, max_tri;
        project_triangle(tri_normal, tv0, tv1, tv2, min_tri, max_tri);
        float r = dot(boxHalfSize, abs(tri_normal));
        if (max_tri < -r || min_tri > r) {
            return false; // Separating axis found
        }
    }

    // --- Test cross products of triangle edges and AABB axes ---
    float3 tri_edges[3] = { e0, e1, e2 };
    for (int i = 0; i < 3; i++) { // tri_edges
        for (int j = 0; j < 3; j++) { // box_axes
            float3 axis = cross(tri_edges[i], box_axes[j]);
            if (dot(axis, axis) > 0.00001f) { // Check for parallel vectors
                float min_tri, max_tri;
                project_triangle(axis, tv0, tv1, tv2, min_tri, max_tri);
                float r = dot(boxHalfSize, abs(axis));
                if (max_tri < -r || min_tri > r) {
                    return false; // Separating axis found
                }
            }
        }
    }

    return true; // No separating axis found, they overlap
}

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint triangleIndex = dispatchThreadID.x;
    
    uint i0 = indices[triangleIndex * 3 + 0];
    uint i1 = indices[triangleIndex * 3 + 1];
    uint i2 = indices[triangleIndex * 3 + 2];

    float4 v0_local = float4(vertices[i0].pos, 1.0f);
    float4 v1_local = float4(vertices[i1].pos, 1.0f);
    float4 v2_local = float4(vertices[i2].pos, 1.0f);

    // Transform vertices to world space
    float3 v0 = mul(v0_local, worldMatrix).xyz;
    float3 v1 = mul(v1_local, worldMatrix).xyz;
    float3 v2 = mul(v2_local, worldMatrix).xyz;

    // Transform to voxel grid space
    float3 g0 = v0 / voxelSize;
    float3 g1 = v1 / voxelSize;
    float3 g2 = v2 / voxelSize;

    // Calculate triangle AABB in grid space
    uint3 minGrid = max(uint3(0, 0, 0), uint3(floor(min(min(g0, g1), g2))));
    uint3 maxGrid = min(voxelGridResolution - 1, uint3(ceil(max(max(g0, g1), g2))));

    float3 voxelHalfSize = float3(0.5, 0.5, 0.5);

    for (uint z = minGrid.z; z <= maxGrid.z; ++z) {
        for (uint y = minGrid.y; y <= maxGrid.y; ++y) {
            for (uint x = minGrid.x; x <= maxGrid.x; ++x) {
                uint3 voxelCoord = uint3(x, y, z);
                float3 voxelCenter = float3(voxelCoord) + voxelHalfSize;

                if (SATTriangleOverlap(voxelCenter, voxelHalfSize, g0, g1, g2)) {
                    InterlockedOr(voxelGrid[voxelCoord], 1, voxelGrid[voxelCoord]);
                }
            }
        }
    }
}
