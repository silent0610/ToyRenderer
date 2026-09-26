// SolidNodeSelectionB_Final.Comp.hlsl
// Pass 4: inclusion check + final selection
// UseHierarchicalParentQuota == 0: legacy (check coarser candidates, complexity-era list)
// UseHierarchicalParentQuota == 1: check already-selected finals, enforce MaxChildrenPerParent

struct SolidNode {
    float3 center;
    float size;
    uint level;
    uint Complexity;
    uint padding[2];
};

RWStructuredBuffer<uint> candidateCountBuffer : register(u0);
RWStructuredBuffer<SolidNode> candidateNodesBuffer : register(u1);
RWStructuredBuffer<uint> finalCountBuffer : register(u2);
RWStructuredBuffer<SolidNode> finalNodesBuffer : register(u3);
StructuredBuffer<uint> LevelCountBuffer : register(t4);
RWStructuredBuffer<uint> parentCountsBuffer : register(u5);
StructuredBuffer<uint> selectionPrefixCountBuffer : register(t6);

struct PushConstantDesc {
    uint MaxSelectedNode;
    uint UseHierarchicalParentQuota;
    uint MaxChildrenPerParent;
    uint BaseSize;
    uint CoarsestLevel;
    uint CurrentLevel;
    uint MaxLevelIndex;
    uint Pad;
};

[[vk::push_constant]]
PushConstantDesc PC;

bool IsNodeIncludedBy(SolidNode child, SolidNode parent) {
    if (child.size > parent.size) return false;

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

bool TrySelect(SolidNode myNode) {
    uint previousCount;
    InterlockedAdd(finalCountBuffer[0], 1, previousCount);
    if (previousCount < PC.MaxSelectedNode) {
        finalNodesBuffer[previousCount] = myNode;
        return true;
    }
    InterlockedAdd(finalCountBuffer[0], -1);
    return false;
}

[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    uint myIndex = id.x;
    uint totalCandidates = candidateCountBuffer[0];
    if (myIndex >= totalCandidates) return;
    if (finalCountBuffer[0] >= PC.MaxSelectedNode) return;

    SolidNode myNode = candidateNodesBuffer[myIndex];

    if (PC.UseHierarchicalParentQuota != 0) {
        // Prefix is frozen before this dispatch (coarser levels only).
        uint selectedPrefix = selectionPrefixCountBuffer[0];
        bool isIncluded = false;
        for (uint i = 0; i < selectedPrefix; ++i) {
            SolidNode previousNode = finalNodesBuffer[i];
            if (IsNodeIncludedBy(myNode, previousNode)) {
                isIncluded = true;
                break;
            }
        }
        if (isIncluded) return;

        // Coarsest level: no sibling quota. Finer: at most MaxChildrenPerParent per parent.
        if (myNode.level != PC.CoarsestLevel) {
            uint parentLinear = myNode.padding[0];
            uint prev;
            InterlockedAdd(parentCountsBuffer[parentLinear], 1, prev);
            if (prev >= PC.MaxChildrenPerParent) return;
        }

        TrySelect(myNode);
        return;
    }

    // Legacy path: inclusion against coarser candidates in the collected list.
    bool isIncluded = false;
    uint maxCheckCount = 0;
    for (uint j = myNode.level + 1; j <= PC.MaxLevelIndex; ++j) {
        maxCheckCount += LevelCountBuffer[j];
    }

    for (uint i = 0; i < maxCheckCount; ++i) {
        SolidNode previousNode = candidateNodesBuffer[i];
        if (IsNodeIncludedBy(myNode, previousNode)) {
            isIncluded = true;
            break;
        }
    }

    if (!isIncluded) {
        TrySelect(myNode);
    }
}
