// MultiViewNodeSelectionFinal.Comp.hlsl
// Legacy: parallel inclusion check (atomic fill)
// Hierarchical unstable (PC.Pad==0): parallel inclusion + atomic parent quota (pre-stability)
// Hierarchical stable   (PC.Pad!=0): workgroup bitonic sort + serial quota + temporal prefer-prev

#define MAX_CANDIDATES 4096
#define MAX_PREV 256
#define GROUP_SIZE 256

struct SolidNode {
    float3 center;
    float size;
    uint level;
    uint Complexity;
    uint padding[2]; // [0]=parentLinear, [1]=stableKey
};

RWStructuredBuffer<uint> candidateCountBuffer : register(u0);
RWStructuredBuffer<SolidNode> candidateNodesBuffer : register(u1);
RWStructuredBuffer<uint> finalCountBuffer : register(u2);
RWStructuredBuffer<SolidNode> finalNodesBuffer : register(u3);
StructuredBuffer<uint> LevelCountBuffer : register(t4);
RWStructuredBuffer<uint> parentCountsBuffer : register(u5);
StructuredBuffer<uint> selectionPrefixCountBuffer : register(t6);
StructuredBuffer<uint> prevSelectedCountBuffer : register(t7);
StructuredBuffer<SolidNode> prevSelectedNodesBuffer : register(t8);

struct PushConstantDesc {
    uint MaxSelectedNode;
    uint UseHierarchicalParentQuota;
    uint MaxChildrenPerParent;
    uint BaseSize;
    uint CoarsestLevel;
    uint CurrentLevel;
    uint MaxLevelIndex;
    uint UseStableSelection; // was Pad; 1=deterministic+temporal, 0=old parallel atomics
};

[[vk::push_constant]]
PushConstantDesc PC;

groupshared uint gs_prevPacked[MAX_PREV];
groupshared uint gs_prevCount;
groupshared uint gs_sortKeys[MAX_CANDIDATES];
groupshared uint gs_sortIdx[MAX_CANDIDATES];
groupshared uint gs_candidateCount;

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

uint PackLevelKey(uint level, uint stableKey) {
    return (level << 16) | (stableKey & 0xffffu);
}

uint MakeSortKey(uint stableKey, bool preferPrev) {
    uint key = stableKey & 0x7fffffffu;
    return preferPrev ? key : (0x80000000u | key);
}

bool PrevContains(uint packed) {
    uint count = gs_prevCount;
    for (uint i = 0; i < count; ++i) {
        if (gs_prevPacked[i] == packed) return true;
    }
    return false;
}

void BitonicSwap(uint i, uint j, bool ascending) {
    uint keyI = gs_sortKeys[i];
    uint keyJ = gs_sortKeys[j];
    if ((keyI > keyJ) == ascending) {
        gs_sortKeys[i] = keyJ;
        gs_sortKeys[j] = keyI;
        uint tmp = gs_sortIdx[i];
        gs_sortIdx[i] = gs_sortIdx[j];
        gs_sortIdx[j] = tmp;
    }
}

void HierarchicalStableSelect(uint lid) {
    if (lid == 0) {
        uint n = candidateCountBuffer[0];
        if (n > MAX_CANDIDATES) n = MAX_CANDIDATES;
        gs_candidateCount = n;

        uint prevCount = prevSelectedCountBuffer[0];
        if (prevCount > MAX_PREV) prevCount = MAX_PREV;
        gs_prevCount = prevCount;
    }
    GroupMemoryBarrierWithGroupSync();

    uint n = gs_candidateCount;
    if (n == 0) return;

    uint prevCount = gs_prevCount;
    for (uint p = lid; p < prevCount; p += GROUP_SIZE) {
        SolidNode prev = prevSelectedNodesBuffer[p];
        gs_prevPacked[p] = PackLevelKey(prev.level, prev.padding[1]);
    }
    GroupMemoryBarrierWithGroupSync();

    for (uint i = lid; i < MAX_CANDIDATES; i += GROUP_SIZE) {
        if (i < n) {
            SolidNode node = candidateNodesBuffer[i];
            bool preferPrev = PrevContains(PackLevelKey(node.level, node.padding[1]));
            gs_sortKeys[i] = MakeSortKey(node.padding[1], preferPrev);
            gs_sortIdx[i] = i;
        } else {
            gs_sortKeys[i] = 0xffffffffu;
            gs_sortIdx[i] = i;
        }
    }
    GroupMemoryBarrierWithGroupSync();

    for (uint k = 2u; k <= MAX_CANDIDATES; k <<= 1) {
        for (uint j = k >> 1; j > 0u; j >>= 1) {
            for (uint i = lid; i < MAX_CANDIDATES; i += GROUP_SIZE) {
                uint ixj = i ^ j;
                if (ixj > i) {
                    bool ascending = ((i & k) == 0u);
                    BitonicSwap(i, ixj, ascending);
                }
            }
            GroupMemoryBarrierWithGroupSync();
        }
    }

    if (lid == 0) {
        uint selectedPrefix = selectionPrefixCountBuffer[0];
        uint finalCount = selectedPrefix;

        for (uint s = 0; s < n; ++s) {
            if (finalCount >= PC.MaxSelectedNode) break;

            SolidNode myNode = candidateNodesBuffer[gs_sortIdx[s]];

            bool isIncluded = false;
            for (uint p = 0; p < selectedPrefix; ++p) {
                if (IsNodeIncludedBy(myNode, finalNodesBuffer[p])) {
                    isIncluded = true;
                    break;
                }
            }
            if (isIncluded) continue;

            if (myNode.level != PC.CoarsestLevel) {
                uint parentLinear = myNode.padding[0];
                if (parentCountsBuffer[parentLinear] >= PC.MaxChildrenPerParent) continue;
                parentCountsBuffer[parentLinear] += 1;
            }

            finalNodesBuffer[finalCount] = myNode;
            finalCount += 1;
        }

        finalCountBuffer[0] = finalCount;
    }
}

void HierarchicalUnstableSelect(uint myIndex) {
    uint totalCandidates = candidateCountBuffer[0];
    if (myIndex >= totalCandidates) return;
    if (finalCountBuffer[0] >= PC.MaxSelectedNode) return;

    SolidNode myNode = candidateNodesBuffer[myIndex];

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

    if (myNode.level != PC.CoarsestLevel) {
        uint parentLinear = myNode.padding[0];
        uint prev;
        InterlockedAdd(parentCountsBuffer[parentLinear], 1, prev);
        if (prev >= PC.MaxChildrenPerParent) return;
    }

    TrySelect(myNode);
}

[numthreads(GROUP_SIZE, 1, 1)]
void main(uint3 id : SV_DispatchThreadID, uint lid : SV_GroupIndex) {
    if (PC.UseHierarchicalParentQuota != 0) {
        if (PC.UseStableSelection != 0) {
            HierarchicalStableSelect(lid);
        } else {
            HierarchicalUnstableSelect(id.x);
        }
        return;
    }

    uint myIndex = id.x;
    uint totalCandidates = candidateCountBuffer[0];
    if (myIndex >= totalCandidates) return;
    if (finalCountBuffer[0] >= PC.MaxSelectedNode) return;

    SolidNode myNode = candidateNodesBuffer[myIndex];

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
