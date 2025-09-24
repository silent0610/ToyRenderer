// === Constants ===
#define GRID_SIZE 32  // Base grid size for Level 0 (32x32x32作为level0输入)
#define MAX_SELECTED_NODES 10
#define EMPTY 0
#define MIXED 1
#define SOLID 2

// === Data Structures ===
struct SolidNode {
    float3 center;      // World space center
    float size;         // Cube edge length
    uint level;         // Mipmap level
    uint padding[3];    // 16-byte alignment
};

// === Resource Bindings ===
RWStructuredBuffer<uint> selectedCountBuffer : register(u0);        // Selected node count
RWStructuredBuffer<SolidNode> selectedNodesBuffer : register(u1);   // Selected nodes buffer

// Mipmap octree textures (read-only) - 对应版本A的Level 0-3 
Texture3D<uint> mipmapTexture0 : register(t2); // Level 0: 64x64x64
Texture3D<uint> mipmapTexture1 : register(t3); // Level 1: 32x32x32
Texture3D<uint> mipmapTexture2 : register(t4); // Level 2: 16x16x16
Texture3D<uint> mipmapTexture3 : register(t5); // Level 3: 8x8x8

// Push constants structure
struct PushConstants {
    uint currentLevel;  // Current level being processed (3->2->1->0)
};

// Push constant declaration
[[vk::push_constant]]
PushConstants pushConstants;

// === Utility Functions ===

// Calculate world space center from grid coordinates and level
float3 CalculateWorldCenter(uint3 coord, uint level) {
    float levelSize = float(GRID_SIZE >> level); // Size of grid at this level

    // Convert grid coordinates to normalized [-1, 1] space
    float3 normalizedCoord = (float3(coord) + 0.5f) / levelSize * 2.0f - 1.0f;
    return normalizedCoord;
}

// Calculate cube edge length in world space
float CalculateCubeSize(uint level) {
    float levelSize = float(GRID_SIZE >> level); // Size of grid at this level
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
    node.padding[2] = 0;
    return node;
}

// Read mipmap texture based on current level
uint ReadCurrentLevelTexture(uint3 coord) {
    switch(pushConstants.currentLevel) {
        case 0: return mipmapTexture0.Load(int4(coord, 0));
        case 1: return mipmapTexture1.Load(int4(coord, 0));
        case 2: return mipmapTexture2.Load(int4(coord, 0));
        case 3: return mipmapTexture3.Load(int4(coord, 0));
        default: return EMPTY;
    }
}

// Check if child node is completely included within parent node
bool IsNodeIncludedBy(SolidNode child, SolidNode parent) {
    // Quick size check: child cannot be larger than parent
    if(child.size > parent.size) return false;

    // Bounding box inclusion check
    float3 parentHalfSize = float3(parent.size * 0.5, parent.size * 0.5, parent.size * 0.5);
    float3 childHalfSize = float3(child.size * 0.5, child.size * 0.5, child.size * 0.5);

    float3 parentMin = parent.center - parentHalfSize;
    float3 parentMax = parent.center + parentHalfSize;
    float3 childMin = child.center - childHalfSize;
    float3 childMax = child.center + childHalfSize;

    return (childMin.x >= parentMin.x && childMax.x <= parentMax.x &&
        childMin.y >= parentMin.y && childMax.y <= parentMax.y &&
        childMin.z >= parentMin.z && childMax.z <= parentMax.z);
}

// === Main Compute Shader ===
[numthreads(4, 4, 4)]
void main(uint3 id : SV_DispatchThreadID) {
    uint3 coord = id;

    // Step 1: Boundary check
    uint levelSize = GRID_SIZE >> pushConstants.currentLevel;
    if(any(coord >= levelSize)) return;

    // Step 2: Read current position node value
    uint nodeValue = ReadCurrentLevelTexture(coord);
    if(nodeValue != SOLID) return;  // Only select SOLID nodes

    // Step 3: Create candidate node
    SolidNode candidate = CreateNode(coord, pushConstants.currentLevel);

    // Step 4: Check if included by any already selected node
    bool isIncluded = false;
    uint selectedCount = selectedCountBuffer[0];

    for(uint i = 0; i < selectedCount; ++i) {
        SolidNode existing = selectedNodesBuffer[i];
        if(IsNodeIncludedBy(candidate, existing)) {
            isIncluded = true;
            break;
        }
    }

    // Step 5: If not included and there's space, add to selection
    if(!isIncluded) {
        // Use atomic operation to reserve a slot only if under limit
        uint previousCount;
        InterlockedAdd(selectedCountBuffer[0], 1, previousCount);
        
        // Only write if we successfully reserved a valid slot
        if(previousCount < MAX_SELECTED_NODES) {
            selectedNodesBuffer[previousCount] = candidate;
        } else {
            // We exceeded the limit, decrement the counter back
            InterlockedAdd(selectedCountBuffer[0], -1);
        }
    }
}