module VoxelOctreeMod;
import std;
import <cmath>;
import <memory>;
import <vector>;
import GlmMod;
using vec3 = glm::vec3;

// 新增自顶向下无条件细分构建完整树骨架函数
void BuildSkeleton(std::shared_ptr<VoxelOctreeNode> node, int depth, int maxDepth, int x, int y, int z, int size, const std::vector<uint8_t> &voxelStates, int gridSize)
{
    if (depth == maxDepth)
    {
        // 叶子节点，读取体素状态
        int index = x + y * gridSize + z * gridSize * gridSize;
        VoxelState leafState = (voxelStates[index] == 1) ? VoxelState::SOLID : VoxelState::EMPTY;
        node->state = leafState;
        return;
    }

    node->state = VoxelState::MIXED;
    float childHalfSize = node->halfSize / 2.0f;

    for (int i = 0; i < 8; ++i)
    {
        int offsetX = (i & 1) ? size / 2 : 0;
        int offsetY = (i & 2) ? size / 2 : 0;
        int offsetZ = (i & 4) ? size / 2 : 0;

        vec3 childCenter = node->center + vec3(
                                              (offsetX ? childHalfSize : -childHalfSize),
                                              (offsetY ? childHalfSize : -childHalfSize),
                                              (offsetZ ? childHalfSize : -childHalfSize));

        node->children[i] = std::make_shared<VoxelOctreeNode>(childCenter, childHalfSize);

        BuildSkeleton(node->children[i], depth + 1, maxDepth, x + offsetX, y + offsetY, z + offsetZ, size / 2, voxelStates, gridSize);
    }
}

// 新增自底向上状态聚合函数
VoxelState PropagateState(std::shared_ptr<VoxelOctreeNode> node)
{
    if (node->IsLeaf())
    {
        return node->state;
    }

    bool hasSolid = false;
    bool hasEmpty = false;

    for (const auto &child : node->children)
    {
        VoxelState childState = PropagateState(child);
        if (childState == VoxelState::MIXED)
        {
            node->state = VoxelState::MIXED;
            return node->state;
        }
        else if (childState == VoxelState::SOLID)
        {
            hasSolid = true;
        }
        else if (childState == VoxelState::EMPTY)
        {
            hasEmpty = true;
        }
    }

    if (hasSolid && hasEmpty)
    {
        node->state = VoxelState::MIXED;
    }
    else if (hasSolid)
    {
        node->state = VoxelState::SOLID;
    }
    else
    {
        node->state = VoxelState::EMPTY;
    }

    return node->state;
}

// 修改VoxelOctree构造函数，调用BuildSkeleton和PropagateState
VoxelOctree::VoxelOctree(const std::vector<uint8_t> &voxelStates, int gridSize)
    : voxelStates(voxelStates), gridSize(gridSize)
{
    maxDepth = 0;
    int size = gridSize;
    while (size > 1)
    {
        size >>= 1;
        maxDepth++;
    }

    float halfSize = gridSize / 2.0f;
    vec3 center(halfSize, halfSize, halfSize);
    root = std::make_shared<VoxelOctreeNode>(center, halfSize);

    // 阶段一：构建完整树骨架
    BuildSkeleton(root, 0, maxDepth, 0, 0, 0, gridSize, voxelStates, gridSize);

    // 阶段二：自底向上聚合状态
    PropagateState(root);
}