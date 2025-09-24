// === Constants ===
#define SDF_RESOLUTION 64
#define MAX_CUBES 512

// === Solid Node Structure ===
struct SolidNode {
    float3 center;       // World space center
    float size;          // Cube edge length
    uint level;          // Mipmap level
    uint3 padding;       // Align to 16 bytes
};

// === Input/Output Resources ===
StructuredBuffer<uint> counterBuffer : register(t0);        // nodeCount - Binding 0
StructuredBuffer<SolidNode> solidNodeBuffer : register(t1); // Solid nodes - Binding 1
RWTexture3D<float> sdfTexture : register(u2);               // SDF texture - Binding 2

// === Main Compute Shader ===
[numthreads(4, 4, 4)]
void main(uint3 id : SV_DispatchThreadID) {
    uint3 coord = id;

    // Skip threads outside texture bounds
    if (any(coord >= SDF_RESOLUTION))
    return;

    // Convert texture coordinate to world position
    // Convert from [0, SDF_RESOLUTION] to [-2.5, 2.5] world space
    float3 normalizedCoord = (float3(coord) + 0.5f) / float(SDF_RESOLUTION);
    float3 worldPos = normalizedCoord * 5.0f - 2.5f;
    
    // 应用与其他着色器一致的Y/Z轴翻转
    // 匹配体素化和相机坐标系的约定
    worldPos.y = -worldPos.y;  // Y轴翻转：匹配世界坐标系向上
    worldPos.z = -worldPos.z;  // Z轴翻转：匹配世界坐标系向前

    // Initialize minimum distance to a large value
    float minDistance = 1000.0f;

    // Get actual number of solid nodes (clamped to MAX_CUBES for performance)
    uint actualNodeCount = min(counterBuffer[0], MAX_CUBES);

    // Iterate through all solid nodes and find minimum distance
    for (uint i = 0; i < actualNodeCount; ++i) {
        float3 cubeCenter = solidNodeBuffer[i].center;
        float cubeSize = solidNodeBuffer[i].size;

        // Calculate signed distance from point to axis-aligned box (cube)
        float3 halfSize = float3(cubeSize * 0.5f, cubeSize * 0.5f, cubeSize * 0.5f);
        float3 d = abs(worldPos - cubeCenter) - halfSize;

        // Distance to box exterior + distance to box interior (negative inside)
        float exteriorDistance = length(max(d, 0.0f));
        float interiorDistance = min(max(d.x, max(d.y, d.z)), 0.0f);

        float distance = exteriorDistance + interiorDistance;

        // Union operation: keep minimum distance (closest surface)
        minDistance = min(minDistance, distance);
    }

    // Handle case where no solid nodes exist
    if (actualNodeCount == 0) {
        minDistance = 1000.0f; // Large positive value (far from any surface)
    }

    // Write final SDF value to 3D texture
    sdfTexture[coord] = minDistance;
}