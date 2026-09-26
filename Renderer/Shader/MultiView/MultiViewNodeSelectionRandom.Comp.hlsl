// MultviewNodeSelectionRandom.comp.hlsl
// Random collection of SOLID nodes. Hierarchical mode skips the random gate
// and stores parentLinear in padding[0] (Complexity slot) for Final quota.

#define EMPTY 0
#define MIXED 2
#define SOLID 1
#define MAX_CANDIDATE_NODES 4096

struct SolidNode {
    float3 center;
    float size;
    uint level;
    uint padding[3];
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
    node.padding[0] = 0;
    node.padding[1] = 0;
    node.padding[2] = 0;
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

uint ParentLinearIndex(uint3 coord, uint levelSize) {
    uint3 parentCoord = coord / 2;
    uint parentSize = max(levelSize / 2, 1u);
    return parentCoord.x + parentCoord.y * parentSize + parentCoord.z * parentSize * parentSize;
}

uint RandomInt(uint3 coord) {
    uint hash = coord.x + coord.y * 5 + coord.z * 9;
    hash = (hash ^ 61) ^ (hash >> 16);
    hash = hash + (hash << 3);
    hash = hash ^ (hash >> 4);
    hash = hash * 0x27d4eb2f;
    hash = hash ^ (hash >> 15);
    return hash % 100;
}

[numthreads(4, 4, 4)]
void main(uint3 id : SV_DispatchThreadID) {
    uint3 coord = id;

    uint levelSize = PushConstant.BaseSize >> PushConstant.CurrentLevel;
    if (any(coord >= levelSize)) return;
    if (candidateCountBuffer[0] >= MAX_CANDIDATE_NODES) return;

    uint nodeValue = ReadCurrentLevelTexture(coord).x;
    if (nodeValue != SOLID) return;

    if (PushConstant.UseHierarchicalParentQuota == 0) {
		if (RandomInt(coord) > 100) {
			return;
		}
    }

    SolidNode candidate = CreateNode(coord, PushConstant.CurrentLevel);
    candidate.center.y *= -1.0;
    candidate.center.z *= -1.0;
    if (PushConstant.UseHierarchicalParentQuota != 0) {
        // 布局对齐 Main/Final：padding[0]=Complexity, [1]=parentLinear, [2]=stableKey
        candidate.padding[0] = 0;
        candidate.padding[1] = ParentLinearIndex(coord, levelSize);
        candidate.padding[2] = coord.x + coord.y * levelSize + coord.z * levelSize * levelSize;
    }

    uint candidateIndex;
    InterlockedAdd(candidateCountBuffer[0], 1, candidateIndex);
    uint count;
    InterlockedAdd(LevelCountBuffer[PushConstant.CurrentLevel], 1, count);
    if (candidateIndex < MAX_CANDIDATE_NODES) {
        candidateNodesBuffer[candidateIndex] = candidate;
    }
}
