// === Constants ===
#define GRID_SIZE 32  // Base grid size for Level 0
#define MAX_SOLID_NODES 1024
#define EMPTY 0
#define MIXED 1
#define SOLID 2

// === Solid Node Structure ===
struct SolidNode {
    float3 center;       // World space center
    float size;          // Cube edge length
    uint level;          // Mipmap level
    uint3 padding;       // Align to 16 bytes
};

// === Input/Output Resources ===
RWStructuredBuffer<uint> counterBuffer : register(u0);     // nodeCount - Binding 0
RWStructuredBuffer<SolidNode> solidNodeBuffer : register(u1); // Solid nodes - Binding 1

// Mipmap textures - Bindings 2-6 (levels 0-4: 64x64x64 to 4x4x4)
Texture3D<uint> mipmapTexture0 : register(t2); // Level 0: 64x64x64
Texture3D<uint> mipmapTexture1 : register(t3); // Level 1: 32x32x32
Texture3D<uint> mipmapTexture2 : register(t4); // Level 2: 16x16x16
Texture3D<uint> mipmapTexture3 : register(t5); // Level 3: 8x8x8
Texture3D<uint> mipmapTexture4 : register(t6); // Level 4: 4x4x4

// Push constants for coordinate transformation (matching voxelization)
struct PushConstants {
    float3 modelCenter;        // 模型中心，与voxelization一致
    float halfSizeWithMargin;  // 包含边距的半尺寸，与voxelization一致
};
[[vk::push_constant]] PushConstants pushConsts;

// === Utility Functions ===

// Calculate world space center from grid coordinates and level
float3 CalculateWorldCenter(uint3 coord, uint level) {
    float levelSize = GRID_SIZE >> level; // Size of grid at this level

    // Convert grid coordinates to normalized [0, 1] space
    float3 normalizedCoord = (float3(coord) + 0.5f) / levelSize;

    // Map to world coordinate space using the same transform as voxelization
    // This creates coordinates in the range that matches the model's bounding box
    float3 worldPos = (normalizedCoord - 0.5) * 2.0 * pushConsts.halfSizeWithMargin + pushConsts.modelCenter;

    // Apply Y/Z axis flips to match coordinate system convention
    worldPos.y = -worldPos.y;  // Y轴翻转：匹配世界坐标系向上
    worldPos.z = -worldPos.z;  // Z轴翻转：匹配世界坐标系向前

    return worldPos;
}

// Calculate cube edge length in world space
float CalculateCubeSize(uint level) {
    float levelSize = GRID_SIZE >> level; // Size of grid at this level
    return 2.0f * pushConsts.halfSizeWithMargin / levelSize; // Size of each cube in world space
}

// === Main Compute Shader ===
[numthreads(4, 4, 1)]  // Reduced thread count for more controlled sampling
void main(uint3 id : SV_DispatchThreadID) {
    uint3 globalID = id;

    uint level =2;
    uint levelSize = 8;

    // Skip if coordinates exceed level dimensions  
    //if(globalID.x >= levelSize || globalID.y >= levelSize) continue;

    // Test: Force complete Z traversal to see if positive Z data exists
    for(uint z = 0; z < levelSize; ++z) {
        uint3 currentCoord = uint3(globalID.xy, z);

        // Read current node value
        uint currentValue;

        currentValue = mipmapTexture2.Load(int4(currentCoord, 0));

        // Select SOLID and MIXED nodes with Level 3→2→1→0 priority
        // MIXED nodes provide boundary information, SOLID for interior
        if(currentValue == EMPTY) continue;

        // Select SOLID/MIXED nodes with strict priority for higher levels
        // Level 4 gets first priority, then 3, then 2, then 1
        uint nodeIndex;
        InterlockedAdd(counterBuffer[0], 1, nodeIndex);

        // Only store if within the 20 node limit
        if(nodeIndex < 512) {
            solidNodeBuffer[nodeIndex].center = CalculateWorldCenter(currentCoord, uint(level));
            solidNodeBuffer[nodeIndex].size = CalculateCubeSize(uint(level));
            solidNodeBuffer[nodeIndex].level = uint(level);
        }

        // Continue processing to allow mix of different levels if high levels are sparse
    }
}