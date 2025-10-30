// MultviewNodeSelection.comp.hlsl
// 只收集SOLID节点，不做包含检查

// === Constants ===

#define EMPTY 0
#define MIXED 2
#define SOLID 1
#define MAX_CANDIDATE_NODES 1000  // 候选节点缓冲区容量

// === Data Structures ===
struct SolidNode {
    float3 center;      // World space center
    float size;         // Cube edge length
    uint level;         // Mipmap level
    uint Complexity;
    uint padding[2];    // 16-byte alignment
};

// === Resource Bindings ===
RWStructuredBuffer<uint> candidateCountBuffer : register(u0);        // Candidate node count
RWStructuredBuffer<SolidNode> candidateNodesBuffer : register(u1);   // Candidate nodes buffer

// Mipmap octree textures (read-only)
Texture3D<uint2> mipmapTexture0 : register(t2); // Level 0: 128
Texture3D<uint2> mipmapTexture1 : register(t3); // Level 1: 64
Texture3D<uint2> mipmapTexture2 : register(t4); // Level 2: 32
Texture3D<uint2> mipmapTexture3 : register(t5); // Level 3: 16
Texture3D<uint2> mipmapTexture4 : register(t6); // Level 3: 8
Texture3D<uint2> mipmapTexture5 : register(t7); // Level 3: 4

RWStructuredBuffer<uint> LevelCountBuffer : register(u8);
// Push constants structure
struct PushConstantDesc {
    uint BaseSize;
    uint CurrentLevel;  // Current level being processed (3,2,1 only)
};

// Push constant declaration
[[vk::push_constant]]
PushConstantDesc PushConstant;

// === Utility Functions ===

// Calculate world space center from grid coordinates and level
float3 CalculateWorldCenter(uint3 coord, uint level) {
    float levelSize = float(PushConstant.BaseSize >> level); // Size of grid at this level

    // Convert grid coordinates to normalized [-1, 1] space
    float3 normalizedCoord = (float3(coord) + 0.5f) / levelSize * 2.0f - 1.0f;
    return normalizedCoord;
}

// Calculate cube edge length in world space
float CalculateCubeSize(uint level) {
    float levelSize = float(PushConstant.BaseSize >> level); // Size of grid at this level
    return 2.0f / levelSize;              // Size of each cube in world space
}

// Create a solid node from grid coordinates
SolidNode CreateNode(uint3 coord, uint level) {
    SolidNode node;
    node.center = CalculateWorldCenter(coord, level);
    node.size = CalculateCubeSize(level);
    node.level = level;
    node.padding[0] = 0;
    node.padding[1] = 0;
    return node;
}

// Read mipmap texture based on current level
uint2 ReadCurrentLevelTexture(uint3 coord) {
    switch(PushConstant.CurrentLevel) {
        case 0: return mipmapTexture0.Load(int4(coord, 0));
        case 1: return mipmapTexture1.Load(int4(coord, 0));
        case 2: return mipmapTexture2.Load(int4(coord, 0));
        case 3: return mipmapTexture3.Load(int4(coord, 0));
        case 4: return mipmapTexture4.Load(int4(coord, 0));
        case 5: return mipmapTexture5.Load(int4(coord, 0));
        default: return EMPTY;
    }
}
uint ReadPrevLevelTexture(uint3 coord) {
    switch(PushConstant.CurrentLevel) {
        case 0: return mipmapTexture1.Load(int4(coord, 0)).y;
        case 1: return mipmapTexture2.Load(int4(coord, 0)).y;
        case 2: return mipmapTexture3.Load(int4(coord, 0)).y;
        case 3: return mipmapTexture4.Load(int4(coord, 0)).y;
        case 4: return mipmapTexture5.Load(int4(coord, 0)).y;
        default: return 0;
    }
}
uint RandomInt(uint3 coord) {
    uint hash = coord.x + coord.y * 5 + coord.z * 9;  // 基于线程ID创建种子
    hash = (hash ^ 61) ^ (hash >> 16);  // 位操作混合
    hash = hash + (hash << 3);  // 扩展哈希值
    hash = hash ^ (hash >> 4);  // 进一步混合
    hash = hash * 0x27d4eb2f;  // 常数乘法扰动
    hash = hash ^ (hash >> 15);  // 最后的扰动

    // 映射到 [0, 255] 范围
    return hash % 256;  // 取模256，得到0到255之间的整数
}
// === Main Compute Shader ===
[numthreads(4, 4, 4)]
void main(uint3 id : SV_DispatchThreadID) {
    uint3 coord = id;

    // Step 1: Boundary check
    uint levelSize = PushConstant.BaseSize >> PushConstant.CurrentLevel;
    if(any(coord >= levelSize)) return;

    if(candidateCountBuffer[0]>=MAX_CANDIDATE_NODES)
    {
        return;
    }
    // Step 2: Read current position node value
    uint nodeValue = ReadCurrentLevelTexture(coord).x;
    if(nodeValue != SOLID) return;  // Only collect SOLID nodes

    // 根据权重进行选择
    uint3 coordPrev = coord/2;
    uint complexity = ReadPrevLevelTexture(coordPrev);

    // 计算距离中心的权重 (远离中心权重更高)
    float3 normalizedCoord = (float3(coord) + 0.5f) / float(levelSize); // 归一化到[0,1]
    float3 centerOffset = abs(normalizedCoord - 0.5f); // 距离中心的偏移量 [0, 0.5]
    float distanceWeight = (centerOffset.x + centerOffset.y + centerOffset.z) / 1.5f; // 归一化到[0,1]

    // // 结合复杂度和距离权重
    float w = lerp(0.8, 1.2, pow(distanceWeight, 1.5));  // 边缘更强
    uint finalComplexity = (uint)min(float(complexity) * w, 255);


    // // 复杂度选择
    // if(RandomInt(coord) > 2*finalComplexity) {
    //     return;
    // }
    // Step 3: Create candidate node
    SolidNode candidate = CreateNode(coord, PushConstant.CurrentLevel);
    candidate.Complexity = finalComplexity;
    candidate.center.y *= -1.0; // Invert Y to match world coordinate system
    candidate.center.z *= -1.0; // Invert Z to match world coordinate system
    // Step 4: Add to candidate buffer (simple append with atomic counter)
    uint candidateIndex;
    InterlockedAdd(candidateCountBuffer[0], 1, candidateIndex);
    uint count;
    InterlockedAdd(LevelCountBuffer[PushConstant.CurrentLevel], 1, count);
    // Only write if within buffer capacity
    if(candidateIndex < MAX_CANDIDATE_NODES) {
        GroupMemoryBarrierWithGroupSync();
        candidateNodesBuffer[candidateIndex] = candidate;
        AllMemoryBarrierWithGroupSync();
    }
}