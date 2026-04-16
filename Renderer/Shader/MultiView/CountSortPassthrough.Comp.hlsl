#define MAX_ELEMENT_NUM 256

struct Node
{
    float3 Center;
    float Size;
    uint Level;
    uint Complexity;
    uint Padding[2];
};

[[vk::binding(0, 0)]] StructuredBuffer<Node> _InputNode;
[[vk::binding(1, 0)]] RWStructuredBuffer<Node> _OutputNode;
[[vk::binding(2, 0)]] RWStructuredBuffer<uint> _NodeNumBuffer;

[numthreads(256, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint tID = id.x;
    uint num = _NodeNumBuffer[0];
    if (tID >= num)
    {
        return;
    }

    _OutputNode[tID] = _InputNode[tID];
}
