// IndirectCommandGeneration.Comp.hlsl
// Stage 4C GPU Data Preparation: Indirect Command Generation
// Generate VkDrawIndexedIndirectCommand for each model sub-part

// Input: Model part information structure (matching C++ ModelPartInfo)
struct ModelPartInfo {
    uint indexCount;        // Number of indices for this sub-part
    uint firstIndex;        // Starting index in global index buffer
    uint vertexOffset;      // Vertex offset for this sub-part
    uint materialIndex;     // Material index (reserved for future use)
};

// Output: Vulkan indirect draw command structure
struct VkDrawIndexedIndirectCommand {
    uint indexCount;        // Number of indices to draw
    uint instanceCount;     // Number of instances (camera count)
    uint firstIndex;        // Starting index
    uint vertexOffset;      // Vertex offset
    uint firstInstance;     // Starting instance ID
};

// Resource bindings
[[vk::binding(0, 0)]] StructuredBuffer<ModelPartInfo> modelParts;               // Input model part info
[[vk::binding(1, 0)]] StructuredBuffer<uint> activeCameraCount;                 // Input active camera count
[[vk::binding(2, 0)]] RWStructuredBuffer<VkDrawIndexedIndirectCommand> indirectCommands; // Output commands

// Push constants
struct PushConstants {
    uint totalPartCount;     // Total number of model sub-parts
};
[[vk::push_constant]] PushConstants pushConsts;

[numthreads(32, 1, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    uint partIndex = id.x;
    
    // Read active camera count from buffer
    uint cameraCount = activeCameraCount[0];
    
    // Boundary check - now we only generate one command per part
    if (partIndex >= pushConsts.totalPartCount) return;
    
    // Read model sub-part information
    ModelPartInfo partInfo = modelParts[partIndex];
    
    // Create indirect draw command for this part
    // Each command will render all camera-face combinations for this part
    VkDrawIndexedIndirectCommand cmd;
    cmd.indexCount = partInfo.indexCount;                    // This sub-part's index count
    cmd.instanceCount = cameraCount * 6;                     // All cameras × 6 faces
    cmd.firstIndex = partInfo.firstIndex;                    // This sub-part's starting index
    cmd.vertexOffset = partInfo.vertexOffset;                // Vertex offset
    cmd.firstInstance = partIndex * (cameraCount * 6);       // Base instance for this part
    
    // Write to output buffer
    indirectCommands[partIndex] = cmd;
}