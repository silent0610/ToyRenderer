// CameraMatrixPreparation.Comp.hlsl
// Stage 4C GPU Data Preparation: Camera Matrix Preparation
// Convert selected nodes from Stage 3B into camera positions

// Input: SolidNode structure (matching C++ SolidNodeSelection::SolidNode)
struct SolidNode {
    float3 center;          // 世界坐标中心 (12 bytes)
    float size;             // 立方体边长 (4 bytes)
    uint level;             // Mipmap层级 (4 bytes)
    uint3 padding;          // 对齐到16字节 (12 bytes)
}; // 总共32字节，满足16字节对齐

// Output: Camera matrix structure (simplified to position only)
struct CameraMatrix {
    float4 cameraPosition;  // Camera position (w component unused, maintains 16-byte alignment)
};

// Resource bindings
[[vk::binding(0, 0)]] StructuredBuffer<SolidNode> selectedNodes;        // Input from Stage 3B
[[vk::binding(1, 0)]] RWStructuredBuffer<CameraMatrix> cameraMatrices;  // Output camera positions
[[vk::binding(2, 0)]] RWStructuredBuffer<uint> activeCameraCount;       // Output active camera count
[[vk::binding(3, 0)]] StructuredBuffer<uint> actualNodeCount;           // Input from Stage 3B counter buffer

// Push constants - 添加统一坐标系变换参数
struct PushConstants {
    uint maxCameraCount;    // Maximum camera limit (10)
    float3 modelCenter;     // 模型中心，与voxelization一致
    float halfSizeWithMargin; // 包含边距的半尺寸，与voxelization一致
};
[[vk::push_constant]] PushConstants pushConsts;

[numthreads(256, 1, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    uint nodeIndex = id.x;
    
    // Read actual node count from Stage 3B output buffer
    uint actualCount = actualNodeCount[0];
    
    // Boundary checks
    if (nodeIndex >= actualCount) return;
    if (nodeIndex >= pushConsts.maxCameraCount) return;
    
    // Read selected node information
    uint revertNodeIndex = actualCount-1-nodeIndex;
    SolidNode node = selectedNodes[revertNodeIndex];
    
    // 将归一化相机位置映射到统一世界坐标系
    // node.center 是归一化坐标 [-1,1]
    float3 normalizedPos = node.center; 
    
    // 映射到 [-halfSizeWithMargin, +halfSizeWithMargin] 的统一立方体空间
    float3 worldCameraPos = (normalizedPos) * pushConsts.halfSizeWithMargin + pushConsts.modelCenter;
    // Generate camera matrix using transformed world position
    CameraMatrix camera;
    camera.cameraPosition = float4(worldCameraPos, 1.0);
    
    // Write to output buffer
    cameraMatrices[nodeIndex] = camera;
    
    // Update active camera count (use thread 0 to avoid race conditions)
    if (nodeIndex == 0) {
        activeCameraCount[0] = min(actualCount, pushConsts.maxCameraCount);
    }
}