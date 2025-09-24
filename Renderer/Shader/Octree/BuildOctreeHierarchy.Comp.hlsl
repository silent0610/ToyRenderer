#include "OctreeNode.hlsl"

// 输入：已排序的莫顿码缓冲区
StructuredBuffer<MortonCodeEntry> SortedMortonCodes : register(t5);

// 输出：八叉树节点池
RWStructuredBuffer<OctreeNode> OctreeNodePool : register(u10);

// 原子计数器用于节点分配
RWStructuredBuffer<uint> NodeCounters : register(u11);

cbuffer BuildConstants : register(b6)
{
    uint valid_voxel_count;     // 有效体素数量
    uint max_node_count;        // 最大节点数量
};

// 计算两个莫顿码的公共前缀长度
uint commonPrefixLength(uint code1, uint code2)
{
    if (code1 == code2)
        return 32; // 完全相同
    
    uint xor_result = code1 ^ code2;
    return 31 - firstbithigh(xor_result); // 计算前导零的数量
}

// 根据莫顿码和深度计算父节点的莫顿码
uint getParentMortonCode(uint morton_code, uint depth)
{
    // 每一层深度对应3位（x,y,z各1位）
    uint shift_amount = (MAX_OCTREE_DEPTH - depth) * 3;
    return morton_code >> shift_amount << shift_amount;
}

// 从莫顿码和深度计算子节点索引（0-7）
uint getChildIndex(uint morton_code, uint depth)
{
    if (depth >= MAX_OCTREE_DEPTH)
        return 0;
    
    uint shift_amount = (MAX_OCTREE_DEPTH - depth - 1) * 3;
    return (morton_code >> shift_amount) & 0x7;
}

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint i = dispatchThreadID.x;
    
    // 边界检查
    if (i >= valid_voxel_count)
        return;
    
    uint morton_code = SortedMortonCodes[i].morton_code;
    uint3 voxel_coord = SortedMortonCodes[i].voxel_coord;
    
    // 检查是否是唯一的叶子节点（与前一个不同）
    bool is_unique_leaf = (i == 0) || (SortedMortonCodes[i-1].morton_code != morton_code);
    
    if (is_unique_leaf)
    {
        // 为叶子节点分配索引
        uint leaf_node_index;
        InterlockedAdd(NodeCounters[0], 1, leaf_node_index);
        
        if (leaf_node_index < max_node_count)
        {
            // 创建叶子节点
            OctreeNode leaf_node;
            leaf_node.first_child_index = INVALID_NODE_INDEX;
            leaf_node.is_leaf_flag = 1;
            leaf_node.state = NODE_STATE_SOLID;
            leaf_node.parent_index = INVALID_NODE_INDEX; // 稍后设置
            
            OctreeNodePool[leaf_node_index] = leaf_node;
        }
        
        // 为每个深度层级创建必要的父节点
        for (uint depth = MAX_OCTREE_DEPTH - 1; depth > 0; depth--)
        {
            uint parent_morton = getParentMortonCode(morton_code, depth);
            
            // 检查是否需要创建新的父节点
            bool create_parent = true;
            
            // 检查前一个条目是否共享相同的父节点
            if (i > 0)
            {
                uint prev_morton = SortedMortonCodes[i-1].morton_code;
                uint prev_parent_morton = getParentMortonCode(prev_morton, depth);
                
                if (parent_morton == prev_parent_morton)
                    create_parent = false;
            }
            
            if (create_parent)
            {
                // 分配父节点索引
                uint parent_node_index;
                InterlockedAdd(NodeCounters[0], 1, parent_node_index);
                
                if (parent_node_index < max_node_count)
                {
                    // 创建父节点
                    OctreeNode parent_node;
                    parent_node.first_child_index = INVALID_NODE_INDEX; // 稍后设置
                    parent_node.is_leaf_flag = 0;
                    parent_node.state = NODE_STATE_MIXED; // 默认为混合状态
                    parent_node.parent_index = INVALID_NODE_INDEX; // 稍后设置
                    
                    OctreeNodePool[parent_node_index] = parent_node;
                }
            }
        }
        
        // 创建根节点（如果这是第一个条目）
        if (i == 0)
        {
            uint root_node_index;
            InterlockedAdd(NodeCounters[0], 1, root_node_index);
            
            if (root_node_index < max_node_count)
            {
                OctreeNode root_node;
                root_node.first_child_index = INVALID_NODE_INDEX; // 稍后设置
                root_node.is_leaf_flag = 0;
                root_node.state = NODE_STATE_MIXED;
                root_node.parent_index = INVALID_NODE_INDEX; // 根节点没有父节点
                
                OctreeNodePool[root_node_index] = root_node;
            }
        }
    }
}