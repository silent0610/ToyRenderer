#include "OctreeNode.hlsl"

RWStructuredBuffer<MortonCodeEntry> InMortonCodes : register(u5);
RWStructuredBuffer<uint> GlobalHistogram : register(u6);

cbuffer RadixSortConstants : register(b4)
{
    uint numElements;
    uint bitOffset;
};

groupshared uint shared_histogram[16];

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID, uint3 groupID : SV_GroupID)
{
    uint i = dispatchThreadID.x;
    uint group_id = groupID.x;
    uint local_id = i % 256;

    // 初始化共享内存直方图
    if (local_id < 16)
    {
        shared_histogram[local_id] = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    // 计算每个元素的数位并更新本地直方图
    if (i < numElements)
    {
        uint digit = (InMortonCodes[i].morton_code >> bitOffset) & 0xF;
        InterlockedAdd(shared_histogram[digit], 1);
    }
    GroupMemoryBarrierWithGroupSync();

    // 将本地直方图加到全局直方图
    if (local_id < 16)
    {
        InterlockedAdd(GlobalHistogram[local_id * 256 + group_id], shared_histogram[local_id]);
    }
}