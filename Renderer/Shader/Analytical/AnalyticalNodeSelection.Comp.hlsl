// === Constants ===
#define EMPTY 0
#define MIXED 2
#define SOLID 1

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

// Mipmap textures - Bindings 2-7 level 0-5 若base为128,则(128,64,32,16,8,4)
Texture3D<uint> mipmapTexture0 : register(t2); // Level 0: 128
Texture3D<uint> mipmapTexture1 : register(t3); // Level 1: 64
Texture3D<uint> mipmapTexture2 : register(t4); // Level 2: 32
Texture3D<uint> mipmapTexture3 : register(t5); // Level 3: 16
Texture3D<uint> mipmapTexture4 : register(t6); // Level 4: 8
Texture3D<uint> mipmapTexture5 : register(t7); // Level 4: 4
// Push constants for coordinate transformation (matching voxelization)
struct PushConstants {  
    uint BaseSize;
    uint SampledLevel;
    uint2 padding;
    float3 modelCenter;        // 模型中心，与voxelization一致
    float halfSizeWithMargin;  // 包含边距的半尺寸，与voxelization一致
};
[[vk::push_constant]] PushConstants pushConsts;

// === Utility Functions ===

// Calculate world space center from grid coordinates and level
float3 CalculateWorldCenter(uint3 coord, uint level) {
    float levelSize = pushConsts.BaseSize >> level; // Size of grid at this level

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

uint SampleSdfLevel(int4 coord, uint level)
{
    switch (level)
    {
        case 0: return mipmapTexture0.Load(coord);
        case 1: return mipmapTexture1.Load(coord);
        case 2: return mipmapTexture2.Load(coord);
        case 3: return mipmapTexture3.Load(coord);
        case 4: return mipmapTexture4.Load(coord);
        case 5: return mipmapTexture5.Load(coord);
        default:
            // 超出范围时返回 0 或某种标记值
            return 0;
    }
}

// Calculate cube edge length in world space
float CalculateCubeSize(uint level) {
    float levelSize = pushConsts.BaseSize >> level; // Size of grid at this level
    return 2.0f * pushConsts.halfSizeWithMargin / levelSize; // Size of each cube in world space
}

// === Main Compute Shader ===
[numthreads(4, 4, 4)]  // Reduced thread count for more controlled sampling
void main(uint3 id : SV_DispatchThreadID) {
    uint3 globalID = id;
    uint levelSize = pushConsts.BaseSize >> pushConsts.SampledLevel;
    // 越界检查（因为 dispatch 组数可能不是整数倍）
    if (any(id >= uint3(levelSize, levelSize, levelSize)))
        return;
    uint3 currentCoord = uint3(globalID.xyz);

    // Read current node value
    uint currentValue = SampleSdfLevel(int4(currentCoord, 0),pushConsts.SampledLevel);

    // Select SOLID and MIXED nodes with Level 3→2→1→0 priority
    // MIXED nodes provide boundary information, SOLID for interior
    if(currentValue == EMPTY) return;

    // Select SOLID/MIXED nodes with strict priority for higher levels
    // Level 4 gets first priority, then 3, then 2, then 1
    uint nodeIndex;
    InterlockedAdd(counterBuffer[0], 1, nodeIndex);

    // Only store if within the 20 node limit
    if(nodeIndex < 512) {
        solidNodeBuffer[nodeIndex].center = CalculateWorldCenter(currentCoord, pushConsts.SampledLevel);
        solidNodeBuffer[nodeIndex].size = CalculateCubeSize(pushConsts.SampledLevel);
        solidNodeBuffer[nodeIndex].level = pushConsts.SampledLevel;
    }
}