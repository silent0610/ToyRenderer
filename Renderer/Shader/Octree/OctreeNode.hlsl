#ifndef OCTREENODE_HLSL
#define OCTREENODE_HLSL

#define NODE_STATE_EMPTY 0
#define NODE_STATE_SOLID 1
#define NODE_STATE_MIXED 2

#define INVALID_NODE_INDEX 0xFFFFFFFF
#define MAX_OCTREE_DEPTH 6

struct OctreeNode {
    uint first_child_index; // 指向节点池中第一个子节点的索引，如果是叶子节点则为INVALID_NODE_INDEX
    uint is_leaf_flag;      // 标记是否是叶子节点 (1代表是)
    uint state;             // 节点状态：EMPTY, SOLID, MIXED
    uint parent_index;      // 父节点索引，便于向上遍历
};

// 用于存储莫顿码和对应的体素信息
struct MortonCodeEntry {
    uint morton_code;       // 莫顿码
    uint3 voxel_coord;      // 体素坐标
};

#endif // OCTREENODE_HLSL