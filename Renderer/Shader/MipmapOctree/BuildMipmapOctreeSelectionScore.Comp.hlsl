// Computes the selection score S(n) for each octree node and stores it in a
// separate score pyramid. This pass is debug-only and does not affect the
// primary octree textures used by the main pipeline.

struct LevelInfo
{
    uint current_level;
    uint input_size;
    uint output_size;
    uint base_size;
};

ConstantBuffer<LevelInfo> cb : register(b0);

Texture3D<uint2> CurrentLevel : register(t1);
Texture3D<uint2> ParentLevel : register(t2);
RWTexture3D<uint> OutputLevel : register(u3);

#define NODE_EMPTY 0
#define NODE_SOLID 1

[numthreads(8, 8, 8)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id >= cb.output_size))
    {
        return;
    }

    // uint2 currentNode = CurrentLevel.Load(int4(id, 0));
    // if (currentNode.x != NODE_SOLID)
    // {
    //     OutputLevel[id] = 0;
    //     return;
    // }

    uint parentComplexity = 0;
    if (cb.output_size > 4)
    {
        uint3 parentCoord = id / 2;
        parentComplexity = ParentLevel.Load(int4(parentCoord, 0)).y;
    }

    float3 normalizedCoord = (float3(id) + 0.5f) / float(cb.output_size);
    float3 centerOffset = abs(normalizedCoord - 0.5f);
    float distanceWeight = (centerOffset.x + centerOffset.y + centerOffset.z) / 1.5f;
    float weight = lerp(0.8f, 1.2f, pow(distanceWeight, 1.5f));

    uint score = (uint)min(float(parentComplexity) * weight, 255.0f);
    OutputLevel[id] = score;
}
