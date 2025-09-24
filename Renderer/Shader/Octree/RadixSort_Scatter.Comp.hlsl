#include "OctreeNode.hlsl"

RWStructuredBuffer<MortonCodeEntry> InMortonCodes : register(u8);
RWStructuredBuffer<MortonCodeEntry> OutMortonCodes : register(u9);
StructuredBuffer<uint> GlobalPrefixSum : register(t4);

cbuffer RadixSortConstants : register(b5)
{
    uint numElements;
    uint bitOffset;
};

groupshared uint g_local_histogram[16];

[numthreads(256, 1, 1)]
void main(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
    uint i = DTid.x;
    uint group_id = Gid.x;
    uint local_id = GTid.x;

    // 1. 加载全局前缀和到共享内存
    if (local_id < 16)
    {
        g_local_histogram[local_id] = GlobalPrefixSum[local_id * 256 + group_id];
    }
    GroupMemoryBarrierWithGroupSync();

    // 2. 计算本地前缀和
    uint digit = (i < numElements) ? (InMortonCodes[i].morton_code >> bitOffset) & 0xF : 0xF;
    uint local_prefix_sum = 0;
    
    // 计算在当前工作组中，在当前元素之前有多少个相同数位的元素
    for (uint j = 0; j < local_id; ++j)
    {
        uint base_index = group_id * 256 + j;
        if (base_index < numElements)
        {
            if (((InMortonCodes[base_index].morton_code >> bitOffset) & 0xF) == digit)
            {
                local_prefix_sum++;
            }
        }
    }

    // 3. 计算最终的散布地址并写入
    if (i < numElements)
    {
        uint scatter_address = g_local_histogram[digit] + local_prefix_sum;
        OutMortonCodes[scatter_address] = InMortonCodes[i];
    }
}