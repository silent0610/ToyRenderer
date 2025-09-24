
struct Vertex {
    float3 pos;
    float3 normal;
    float2 uv;
    float4 color;
    float4 joint0;
    float4 weight0;
    float4 tangent;
};

StructuredBuffer<Vertex> InVertices : register(t0);
StructuredBuffer<uint> InIndices : register(t1);

RWStructuredBuffer<uint2> OutMortonCodes : register(u0);

// Scene bounding box, should be passed via a constant buffer
cbuffer SceneUniforms : register(b0)
{
    float3 minAABB;
    float padding;
    float3 maxAABB;
    float padding2;
};

// Expands a 10-bit integer into 30 bits by inserting 2 zeros after each bit.
// This is a key part of Morton code generation.
uint expandBits(uint v)
{
    v = (v * 0x00010001u) & 0xFF0000FFu;
    v = (v * 0x00000101u) & 0x0F00F00Fu;
    v = (v * 0x00000011u) & 0xC30C30C3u;
    v = (v * 0x00000005u) & 0x49249249u;
    return v;
}

// Calculates a 30-bit Morton code for the given 3D point.
uint morton3D(float3 p)
{
    // Quantize the point to a 10-bit integer grid (0-1023)
    float3 quantized = clamp(p, 0.0, 1.0) * 1023.0;
    uint x = expandBits((uint)quantized.x);
    uint y = expandBits((uint)quantized.y);
    uint z = expandBits((uint)quantized.z);
    return x * 4 + y * 2 + z; // Interleave bits
}

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint triangleIndex = dispatchThreadID.x;

    // Assuming there's a mechanism to know the total triangle count to avoid out-of-bounds access.
    // This should be passed in a constant buffer.
    // For now, this check is implicit.

    uint i0 = InIndices[triangleIndex * 3 + 0];
    uint i1 = InIndices[triangleIndex * 3 + 1];
    uint i2 = InIndices[triangleIndex * 3 + 2];

    float3 v0 = InVertices[i0].pos;
    float3 v1 = InVertices[i1].pos;
    float3 v2 = InVertices[i2].pos;

    // Calculate the center of the triangle
    float3 center = (v0 + v1 + v2) / 3.0;

    // Normalize the center position to a [0, 1] cube based on the scene's AABB
    float3 normalized_pos = (center - minAABB) / (maxAABB - minAABB);

    uint mortonCode = morton3D(normalized_pos);

    // Output the Morton code and the triangle index
    OutMortonCodes[triangleIndex] = uint2(mortonCode, triangleIndex);
}
