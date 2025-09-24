RWTexture3D<uint> voxelGrid : register(u0);

cbuffer VoxelizationParams : register(b0) {
    uint3 voxelGridResolution;
    float voxelSize;
    matrix worldMatrix;
};

[numthreads(8, 8, 8)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (any(dispatchThreadID >= voxelGridResolution))
    {
        return;
    }

    // Check if the voxel is on the boundary of the grid
    if (dispatchThreadID.x == 0 || dispatchThreadID.x == voxelGridResolution.x - 1 ||
        dispatchThreadID.y == 0 || dispatchThreadID.y == voxelGridResolution.y - 1 ||
        dispatchThreadID.z == 0 || dispatchThreadID.z == voxelGridResolution.z - 1)
    {
        uint original_value;
        uint new_value = 2; // Mark as EXTERNAL

        // Only mark empty voxels (value 0) as external.
        // Do not overwrite surface voxels (value 1).
        InterlockedCompareExchange(voxelGrid[dispatchThreadID], 0, new_value, original_value);
    }
}
