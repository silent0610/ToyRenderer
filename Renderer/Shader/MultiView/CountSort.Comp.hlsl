#define MAX_ELEMENT_NUM 256
#define MAX_VALUE 255

struct Node 
{
    float3 Center;      // World space center
    float Size;         // Cube edge length
    uint Level;         // Mipmap level
    uint Complexity;
    uint Padding[2];    // 16-byte alignment
};

[[vk::binding(0, 0)]] StructuredBuffer<Node> _InputNode;
[[vk::binding(1, 0)]] RWStructuredBuffer<Node> _OutputNode;
[[vk::binding(2, 0)]] RWStructuredBuffer<uint> _NodeNumBuffer;

groupshared uint sHistogram[MAX_VALUE + 1];
groupshared uint sPrefix[MAX_VALUE + 1];
[numthreads(256,1,1)]
void main(uint3 id:SV_DispatchThreadID)
{
    uint tID = id.x;
    uint num = _NodeNumBuffer[0];
    if (tID <= MAX_VALUE) 
    {
        sHistogram[tID] = 0;
    }
    GroupMemoryBarrierWithGroupSync();
    if(tID>=num)
    {
        return;
    }
    

    uint val = _InputNode[tID].Complexity;
    uint localOffset = 0;

    // 统计值 == val 的个数, 并记录当前局部序号
    InterlockedAdd(sHistogram[val],1,localOffset);
    GroupMemoryBarrierWithGroupSync();
    if (tID == 0)
    {
        uint prefix = 0;
        for (uint i = 0; i <= MAX_VALUE; ++i)
        {
            sPrefix[i] = prefix;
            prefix += sHistogram[i];
        }
    }
    GroupMemoryBarrierWithGroupSync();

    uint sortedIndex = sPrefix[val] + localOffset;
    _OutputNode[sortedIndex] = _InputNode[tID];
}
