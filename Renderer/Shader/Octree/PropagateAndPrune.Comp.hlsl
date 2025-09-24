#include "OctreeNode.hlsl"

// 八叉树节点池
RWStructuredBuffer<OctreeNode> OctreeNodePool : register(u12);

// 当前处理的层级节点列表
StructuredBuffer<uint> CurrentLevelNodes : register(t6);

cbuffer PropagateConstants : register(b7)
{
    uint current_level;         // 当前处理的层级
    uint node_count_at_level;   // 当前层级的节点数量
};

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint i = dispatchThreadID.x;
    
    // 边界检查
    if (i >= node_count_at_level)
        return;
    
    uint node_index = CurrentLevelNodes[i];
    OctreeNode current_node = OctreeNodePool[node_index];
    
    // 只处理非叶子节点
    if (current_node.is_leaf_flag == 1)
        return;
    
    // 检查所有8个子节点的状态
    uint solid_count = 0;
    uint empty_count = 0;
    uint mixed_count = 0;
    uint valid_child_count = 0;
    
    for (uint child_idx = 0; child_idx < 8; child_idx++)
    {
        uint child_node_index = current_node.first_child_index + child_idx;
        
        // 检查子节点是否有效
        if (child_node_index != INVALID_NODE_INDEX)
        {
            OctreeNode child_node = OctreeNodePool[child_node_index];
            valid_child_count++;
            
            switch (child_node.state)
            {
                case NODE_STATE_EMPTY:
                    empty_count++;
                    break;
                case NODE_STATE_SOLID:
                    solid_count++;
                    break;
                case NODE_STATE_MIXED:
                    mixed_count++;
                    break;
            }
        }
        else
        {
            // 无效子节点视为空
            empty_count++;
        }
    }
    
    // 决定父节点的状态并执行剪枝
    uint new_state = NODE_STATE_MIXED;
    bool should_prune = false;
    
    if (mixed_count > 0)
    {
        // 如果有任何子节点是混合状态，父节点必须是混合状态
        new_state = NODE_STATE_MIXED;
    }
    else if (solid_count > 0 && empty_count > 0)
    {
        // 如果既有实体又有空的子节点，父节点是混合状态
        new_state = NODE_STATE_MIXED;
    }
    else if (solid_count == 8)
    {
        // 如果所有8个子节点都是实体，父节点是实体，可以剪枝
        new_state = NODE_STATE_SOLID;
        should_prune = true;
    }
    else if (empty_count == 8)
    {
        // 如果所有8个子节点都是空的，父节点是空的，可以剪枝
        new_state = NODE_STATE_EMPTY;
        should_prune = true;
    }
    
    // 更新父节点状态
    OctreeNodePool[node_index].state = new_state;
    
    // 执行剪枝：如果所有子节点状态相同，将父节点标记为叶子节点
    if (should_prune)
    {
        OctreeNodePool[node_index].is_leaf_flag = 1;
        OctreeNodePool[node_index].first_child_index = INVALID_NODE_INDEX;
        
        // 标记子节点为无效（实际的内存回收可以在后续pass中执行）
        for (uint child_idx = 0; child_idx < 8; child_idx++)
        {
            uint child_node_index = current_node.first_child_index + child_idx;
            if (child_node_index != INVALID_NODE_INDEX)
            {
                // 将子节点标记为无效（可以使用特殊状态值）
                OctreeNodePool[child_node_index].state = 0xFFFFFFFF; // 标记为已删除
            }
        }
    }
}