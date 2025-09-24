// ScanFill.comp.hlsl
// Stage: Compute Shader

struct VoxelConstants {
    float4x4 model;
    float4x4 view;
    float4x4 projection;
    uint3 voxelGridSize;
};
ConstantBuffer<VoxelConstants> cb : register(b0);

// Input: The counter texture from the marking pass
Texture3D<int> VoxelCounter : register(t1);

// Output: The final solid voxel texture (R8_UINT format)
RWTexture3D<uint> FinalVoxelState : register(u2);

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID) {
    // Each thread handles one (x, y) column along the Z axis
    uint2 xy = dispatchThreadID.xy;

    // Check if we are outside the grid bounds to prevent unnecessary processing
    if (xy.x >= cb.voxelGridSize.x || xy.y >= cb.voxelGridSize.y) {
        return;
    }

    int counter = 0;
    for (int z = 0; z < cb.voxelGridSize.z; ++z) {
        // Accumulate the winding number along the Z-axis
        counter += VoxelCounter.Load(uint4(xy.x, xy.y, z, 0));

        // If counter > 0, the voxel is inside the mesh
        if (counter > 0) {
            FinalVoxelState[uint3(xy.x, xy.y, z)] = 1;
        }
        else {
            // This assumes the texture was cleared to 0 beforehand,
            // but writing 0 explicitly is safer.
            FinalVoxelState[uint3(xy.x, xy.y, z)] = 0;
        }
    }
}