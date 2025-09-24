#include "OctreeNode.hlsl"

// 输入：64x64x64的体素状态纹理
Texture3D<uint> FinalVoxelState : register(t3);

// 输出：莫顿码缓冲区
RWStructuredBuffer<MortonCodeEntry> VoxelMortonCodeBuffer : register(u3);

// 原子计数器
RWStructuredBuffer<uint> AtomicCountersBuffer : register(u4);

cbuffer VoxelConstants : register(b3)
{
    uint voxel_grid_size;  // 64
    uint max_voxel_count;  // 最大体素数量
};

// 将一个10位整数扩展为30位，通过在每位后插入2个零
uint expandBits(uint v)
{
    v = (v * 0x00010001u) & 0xFF0000FFu;
    v = (v * 0x00000101u) & 0x0F00F00Fu;
    v = (v * 0x00000011u) & 0xC30C30C3u;
    v = (v * 0x00000005u) & 0x49249249u;
    return v;
}

// 计算给定3D坐标的30位莫顿码
uint morton3D(uint3 coord)
{
    uint x = expandBits(coord.x);
    uint y = expandBits(coord.y);
    uint z = expandBits(coord.z);
    return x * 4 + y * 2 + z; // 交错位
}

[numthreads(4, 4, 4)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint3 voxel_coord = dispatchThreadID.xyz;
    
    // 边界检查
    if (any(voxel_coord >= voxel_grid_size))
        return;
    
    // 采样体素状态
    uint voxel_state = FinalVoxelState[voxel_coord];
    
    // 只处理内部体素（状态为1）
    if (voxel_state == 1)
    {
        // 生成莫顿码
        uint morton_code = morton3D(voxel_coord);
        
        // 原子操作获取写入索引
        uint write_index;
        InterlockedAdd(AtomicCountersBuffer[0], 1, write_index);
        
        // 检查是否超出缓冲区大小
        if (write_index < max_voxel_count)
        {
            // 写入莫顿码条目
            MortonCodeEntry entry;
            entry.morton_code = morton_code;
            entry.voxel_coord = voxel_coord;
            VoxelMortonCodeBuffer[write_index] = entry;
        }
    }
}