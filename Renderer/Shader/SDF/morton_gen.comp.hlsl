struct MortonPrimitive
{
    uint morton_code;
    uint primitive_index;
};

[[vk::binding(0, 0)]] StructuredBuffer<float3> Positions;
[[vk::binding(1, 0)]] ByteAddressBuffer Indices;
[[vk::binding(2, 0)]] RWStructuredBuffer<MortonPrimitive> MortonPrimitives;

// Expands a 10-bit integer into 30 bits by inserting 2 zeros after each bit.
uint expand_bits(uint v)
{
    v = (v * 0x00010001u) & 0xFF0000FFu;
    v = (v * 0x00000101u) & 0x0F00F00Fu;
    v = (v * 0x00000011u) & 0xC30C30C3u;
    v = (v * 0x00000005u) & 0x49249249u;
    return v;
}

// Calculates a 30-bit Morton code for the given 3D point.
// The point is assumed to be in a [0, 1023] range.
uint morton_code(uint3 p)
{
    uint x = expand_bits(p.x);
    uint y = expand_bits(p.y);
    uint z = expand_bits(p.z);
    return (z << 2) | (y << 1) | x;
}

[numthreads(64, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint primitive_idx = DTid.x;

    uint index_0 = Indices.Load((primitive_idx * 3) * 4);
    uint index_1 = Indices.Load((primitive_idx * 3 + 1) * 4);
    uint index_2 = Indices.Load((primitive_idx * 3 + 2) * 4);

    float3 v0 = Positions[index_0];
    float3 v1 = Positions[index_1];
    float3 v2 = Positions[index_2];

    // For simplicity, we use the center of the primitive's AABB.
    float3 center = (min(v0, min(v1, v2)) + max(v0, max(v1, v2))) * 0.5;

    // TODO: You need to pass the scene AABB to normalize the center point to [0, 1]
    // For now, assuming a hardcoded scene size.
    float3 normalized_center = center * 1023.0f; // Assuming scene is roughly unit-sized

    uint morton = morton_code((uint3)normalized_center);

    MortonPrimitive prim;
    prim.morton_code = morton;
    prim.primitive_index = primitive_idx;

    MortonPrimitives[primitive_idx] = prim;
}
