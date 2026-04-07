// SDFFusion.Comp.hlsl
// Converts multi-view depth maps to 3D signed distance field

// SDF Grid Configuration

static const float SDF_WORLD_SIZE = 2.0f;       // 10 units world space

static const float3 SDF_MIN_BOUNDS = float3(-SDF_WORLD_SIZE * 0.5f, -SDF_WORLD_SIZE * 0.5f, -SDF_WORLD_SIZE * 0.5f);

// Camera matrix structure (matches C++ GPUDataPreparation4C)
struct CameraMatrix {
    float4 cameraPosition;  // Camera position (w component unused)
};

// Push constants for SDF parameters
struct PushConstants {
    uint maxCameraCount;     // Number of active cameras
    float maxDistance;          // Maximum SDF distance
    uint BaseSize;
    uint padding;            // 16-byte alignment
};
[[vk::push_constant]] PushConstants pushConsts;

// Resource bindings
SamplerState depthSampler : register(s0);                  // Binding 0: Sampler
TextureCubeArray depthCubemapArray : register(t1);         // Binding 1: Sampled Image (Cube Array) 
RWTexture3D<float> finalSDFTexture : register(u2);         // Binding 2: Storage Image (3D SDF output)
StructuredBuffer<CameraMatrix> cameraMatrices : register(t3); // Binding 3: Storage Buffer
StructuredBuffer<uint> activeCameraCount:register(t4);
[numthreads(4, 4, 4)]
void main(uint3 id : SV_DispatchThreadID) {

    
    // 1. Thread setup and bounds checking
    if (id.x >= pushConsts.BaseSize || id.y >= pushConsts.BaseSize || id.z >= pushConsts.BaseSize) {
        return; // Early exit if thread is outside SDF grid
    }
    float voxelSize = SDF_WORLD_SIZE / pushConsts.BaseSize;
    // 2. Calculate world position of current voxel
    // Convert thread ID to voxel position in SDF grid
    float3 voxelID = float3(id) + float3(0.5,0.5,0.5);
    float3 voxelPos = voxelID * voxelSize + SDF_MIN_BOUNDS;
    // Calculate world position at voxel center
    float3 worldPos = float3(voxelPos.x,-voxelPos.y,-voxelPos.z);
    // 3. Initialize minimum SDF value
    float minSdfValue = pushConsts.maxDistance; // Start with maximum distance

    // 4. Main loop: iterate through all active cameras
    for (uint cameraIndex = 0; cameraIndex < activeCameraCount[0];cameraIndex++) {
        // a. Get camera data
        const float3 cameraPos = cameraMatrices[cameraIndex].cameraPosition.xyz;

        // Debug: Check for invalid camera positions
        if (length(cameraPos) < 0.001f) {
            continue; // Skip invalid cameras
        }

        // b. Calculate projection vector from camera to voxel
        float3 vecToVoxel = worldPos-cameraPos;
        float currentDistance = length(vecToVoxel);

        // Skip if too far away (optimization)
        if (currentDistance > pushConsts.maxDistance) {
            continue;
        }

        // c. Use TextureCubeArray built-in sampling
        // Normalize direction vector for cubemap sampling
        float3 sampleDirection = normalize(vecToVoxel);
        sampleDirection.z *= -1;
        sampleDirection.x *= -1;
        // For TextureCubeArray: float4(direction.xyz, arrayIndex)
        // Hardware automatically determines face and UV coordinates
        float4 cubemapCoord = float4(sampleDirection, float(cameraIndex));

        // d. Sample depth map and convert to world distance
        float sampledDepth = depthCubemapArray.SampleLevel(depthSampler, cubemapCoord, 0).r;

        // Debug: Check for invalid depth values
        if (sampledDepth <= 0.0f || sampledDepth > 1000.0f) {
            continue; // Skip invalid depth samples
        }

        // e. Calculate SDF value for current view: distance from voxel to surface
        float sdfValue = currentDistance - sampledDepth;

        // f. Update minimum SDF value
        if(minSdfValue<0&&sdfValue<0) {
            minSdfValue = max(minSdfValue, sdfValue);
        }
        else {
            minSdfValue = min(minSdfValue, sdfValue);
        }
    }

    finalSDFTexture[id] = minSdfValue;
}