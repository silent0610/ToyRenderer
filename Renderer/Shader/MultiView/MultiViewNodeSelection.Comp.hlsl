// MultviewNodeSelection.comp.hlsl
// Collect SOLID candidate nodes. Two modes via UseHierarchicalParentQuota:
//   0: legacy complexity gate (bias toward complex surface regions)
//   1: all SOLID nodes; store parentLinear in Complexity for Final quota

#define EMPTY 0
#define MIXED 2
#define SOLID 1
#define MAX_CANDIDATE_NODES 4096

struct SolidNode {
    float3 center;
    float size;
    uint level;
    uint Complexity;
    uint padding[2];
};

RWStructuredBuffer<uint> candidateCountBuffer : register(u0);
RWStructuredBuffer<SolidNode> candidateNodesBuffer : register(u1);

Texture3D<uint2> mipmapTexture0 : register(t2);
Texture3D<uint2> mipmapTexture1 : register(t3);
Texture3D<uint2> mipmapTexture2 : register(t4);
Texture3D<uint2> mipmapTexture3 : register(t5);
Texture3D<uint2> mipmapTexture4 : register(t6);
Texture3D<uint2> mipmapTexture5 : register(t7);
Texture3D<uint2> mipmapTexture6 : register(t8);
Texture3D<uint2> mipmapTexture7 : register(t9);
Texture3D<uint2> mipmapTexture8 : register(t10);
Texture3D<uint2> mipmapTexture9 : register(t11);
Texture3D<uint2> mipmapTexture10 : register(t12);

RWStructuredBuffer<uint> LevelCountBuffer : register(u13);

struct PushConstantDesc {
    uint BaseSize;
    uint CurrentLevel;
    uint UseHierarchicalParentQuota;
    uint MaxChildrenPerParent;
};

[[vk::push_constant]]
PushConstantDesc PushConstant;

float3 CalculateWorldCenter(uint3 coord, uint level) {
    float levelSize = float(PushConstant.BaseSize >> level);
    float3 normalizedCoord = (float3(coord) + 0.5f) / levelSize * 2.0f - 1.0f;
    return normalizedCoord;
}

float CalculateCubeSize(uint level) {
    float levelSize = float(PushConstant.BaseSize >> level);
    return 2.0f / levelSize;
}

SolidNode CreateNode(uint3 coord, uint level) {
    SolidNode node;
    node.center = CalculateWorldCenter(coord, level);
    node.size = CalculateCubeSize(level);
    node.level = level;
    node.Complexity = 0;
    node.padding[0] = 0;
    node.padding[1] = 0;
    return node;
}

uint2 ReadCurrentLevelTexture(uint3 coord) {
    switch (PushConstant.CurrentLevel) {
        case 0: return mipmapTexture0.Load(int4(coord, 0));
        case 1: return mipmapTexture1.Load(int4(coord, 0));
        case 2: return mipmapTexture2.Load(int4(coord, 0));
        case 3: return mipmapTexture3.Load(int4(coord, 0));
        case 4: return mipmapTexture4.Load(int4(coord, 0));
        case 5: return mipmapTexture5.Load(int4(coord, 0));
        case 6: return mipmapTexture6.Load(int4(coord, 0));
        case 7: return mipmapTexture7.Load(int4(coord, 0));
        case 8: return mipmapTexture8.Load(int4(coord, 0));
        case 9: return mipmapTexture9.Load(int4(coord, 0));
        case 10: return mipmapTexture10.Load(int4(coord, 0));
        default: return uint2(EMPTY, 0);
    }
}

uint ReadPrevLevelTexture(uint3 coord) {
    switch (PushConstant.CurrentLevel) {
        case 0: return mipmapTexture1.Load(int4(coord, 0)).y;
        case 1: return mipmapTexture2.Load(int4(coord, 0)).y;
        case 2: return mipmapTexture3.Load(int4(coord, 0)).y;
        case 3: return mipmapTexture4.Load(int4(coord, 0)).y;
        case 4: return mipmapTexture5.Load(int4(coord, 0)).y;
        case 5: return mipmapTexture6.Load(int4(coord, 0)).y;
        case 6: return mipmapTexture7.Load(int4(coord, 0)).y;
        case 7: return mipmapTexture8.Load(int4(coord, 0)).y;
        case 8: return mipmapTexture9.Load(int4(coord, 0)).y;
        case 9: return mipmapTexture10.Load(int4(coord, 0)).y;
        default: return 0;
    }
}

uint ParentLinearIndex(uint3 coord, uint levelSize) {
    uint3 parentCoord = coord / 2;
    uint parentSize = max(levelSize / 2, 1u);
    return parentCoord.x + parentCoord.y * parentSize + parentCoord.z * parentSize * parentSize;
}

[numthreads(4, 4, 4)]
void main(uint3 id : SV_DispatchThreadID) {
    uint3 coord = id;

    uint levelSize = PushConstant.BaseSize >> PushConstant.CurrentLevel;
    if (any(coord >= levelSize)) return;
    if (candidateCountBuffer[0] >= MAX_CANDIDATE_NODES) return;

    uint2 nodeValue = ReadCurrentLevelTexture(coord).xy;
    if (nodeValue.x != SOLID) return;

    SolidNode candidate = CreateNode(coord, PushConstant.CurrentLevel);
    candidate.center.y *= -1.0;
    candidate.center.z *= -1.0;

    if (PushConstant.UseHierarchicalParentQuota != 0) {
        // parentLinear 放 padding[0]，勿写入 Complexity（CountSort 会按 Complexity 排序且仅支持 0..255）
        candidate.Complexity = 0;
        candidate.padding[0] = ParentLinearIndex(coord, levelSize);
    } else {
        uint3 coordPrev = coord / 2;
        uint complexity = ReadPrevLevelTexture(coordPrev);

        float3 normalizedCoord = (float3(coord) + 0.5f) / float(levelSize);
        float3 centerOffset = abs(normalizedCoord - 0.5f);
        float distanceWeight = (centerOffset.x + centerOffset.y + centerOffset.z) / 1.5f;
        float w = lerp(0.95, 1.05, pow(distanceWeight, 1.5));
        uint finalComplexity = (uint)min(float(complexity + nodeValue.y) * w, 255);
        if (finalComplexity < 100) return;
        candidate.Complexity = finalComplexity;
        candidate.padding[0] = 0;
    }

    uint candidateIndex;
    InterlockedAdd(candidateCountBuffer[0], 1, candidateIndex);
    uint count;
    InterlockedAdd(LevelCountBuffer[PushConstant.CurrentLevel], 1, count);
    if (candidateIndex < MAX_CANDIDATE_NODES) {
        candidateNodesBuffer[candidateIndex] = candidate;
    }
}
