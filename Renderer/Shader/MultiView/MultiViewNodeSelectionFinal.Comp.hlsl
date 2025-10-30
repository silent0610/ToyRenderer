// SolidNodeSelectionB_Final.Comp.hlsl
// Pass 4: 包含检查和最终选择
// 每个线程检查自己的候选节点是否被之前的节点包含


// === Data Structures ===
struct SolidNode {
    float3 center;      // World space center
    float size;         // Cube edge length
    uint level;         // Mipmap level
    uint Complexity;
    uint padding[2];    // 16-byte alignment
};

// === Resource Bindings ===
// Input: Candidate nodes from collection passes
RWStructuredBuffer<uint> candidateCountBuffer : register(u0);         // Read-only candidate count
RWStructuredBuffer<SolidNode> candidateNodesBuffer : register(u1);    // Read-only candidate nodes

// Output: Final selected nodes
RWStructuredBuffer<uint> finalCountBuffer : register(u2);             // Final selected count
RWStructuredBuffer<SolidNode> finalNodesBuffer : register(u3);        // Final selected nodes
StructuredBuffer<uint> LevelCountBuffer : register(t4);

struct PushConstantDesc
{
    uint MaxSelectedNode;
};
[[vk::push_constant]]
PushConstantDesc PC;
// === Utility Functions ===

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
[numthreads(64, 1, 1)]  // 1D dispatch for processing candidate list
void main(uint3 id : SV_DispatchThreadID) {
    uint myIndex = id.x;

    // Read candidate count (safe since it's read-only at this stage)
    uint totalCandidates = candidateCountBuffer[0];

    // 个体线程边界检查
    if(myIndex >= totalCandidates) return;
    if(finalCountBuffer[0]>=PC.MaxSelectedNode)
    {
        return;
    }
    // Get my candidate node
    SolidNode myNode = candidateNodesBuffer[myIndex];

    // Step 1: Check if included by any previous node (natural priority order)
    bool isIncluded = false;

    uint checkLow = LevelCountBuffer[myNode.level+1];

    uint maxCheckCount = 0;
    for(uint j = myNode.level + 1;j<=5;++j)
    {
        maxCheckCount+=LevelCountBuffer[j];
    }

    for(uint i = 0; i < maxCheckCount; ++i) {
        SolidNode previousNode = candidateNodesBuffer[i];
        if(IsNodeIncludedBy(myNode, previousNode)) {
            isIncluded = true;
            break; // 早期退出
        }
    }

    // Step 2: If not included and there's space, add to final selection
    if(!isIncluded) {
        // Use atomic operation to reserve a slot
        uint previousCount;
        InterlockedAdd(finalCountBuffer[0], 1, previousCount);

        // Only write if we successfully reserved a valid slot
        if(previousCount < PC.MaxSelectedNode) {
            finalNodesBuffer[previousCount] = myNode;
        } else {
            // We exceeded the limit, decrement the counter back
            InterlockedAdd(finalCountBuffer[0], -1);
        }
    }
}