// GPU节点选择compute shader - 支持多种可扩展策略
// 编译命令: dxc -spirv -T cs_6_0 -E main NodeSelection.Comp.hlsl -Fo NodeSelection.Comp.spv

// 选择策略常量
#define STRATEGY_ALL_SOLID      0
#define STRATEGY_BOUNDARY_ONLY  1
#define STRATEGY_SIZE_BASED     2
#define STRATEGY_COMPLEXITY     3
#define STRATEGY_CUSTOM         4

// 节点状态
#define NODE_EMPTY 0
#define NODE_MIXED 1
#define NODE_SOLID 2

// 配置结构体（与C++对应）
struct SelectionConfig {
    uint minLevel;
    uint maxLevel; 
    uint maxNodes;
    float minNodeSize;
    
    uint strategy;
    uint reserved1, reserved2, reserved3;
    
    // 高级参数
    float complexityThreshold;
    float sizeWeight;
    float depthWeight;
    float coverageWeight;
    uint customFlags;
    uint reserved4, reserved5, reserved6;
};

// 选中节点结构体
struct SelectedNode {
    float3 center;
    float size;
    uint3 position;
    uint level;
    uint nodeId;
    float score;
    uint metadata;
    uint reserved;
};

// 统计信息结构体
struct SelectionStats {
    uint totalCandidates;
    uint selectedCount;
    uint boundaryNodes;
    uint interiorNodes;
    float executionTime;
    uint memoryUsage;
    uint reserved1, reserved2;
};

// 绑定资源
ConstantBuffer<SelectionConfig> config : register(b0);
Texture3D<uint> octreeTextures[6] : register(t1);      // 八叉树纹理数组 (6个mip levels)
RWStructuredBuffer<SelectedNode> selectedNodes : register(u2);     // 输出节点
RWStructuredBuffer<uint> atomicCounter : register(u3);             // 原子计数器
RWStructuredBuffer<SelectionStats> stats : register(u4);           // 统计信息

// 工具函数：计算节点信息
uint3 DecodeNodePosition(uint nodeId, uint level) {
    uint levelSize = 64u >> (level + 1);  // level 0 = 32, level 1 = 16...
    
    uint z = nodeId / (levelSize * levelSize);
    uint remainder = nodeId % (levelSize * levelSize);
    uint y = remainder / levelSize;
    uint x = remainder % levelSize;
    
    return uint3(x, y, z);
}

float3 CalculateNodeCenter(uint3 position, uint level) {
    float nodeSize = 1.0f / (1u << level);  // 节点大小 = 1/2^level (level越高越小)
    return float3(position) * nodeSize + float3(nodeSize * 0.5f, nodeSize * 0.5f, nodeSize * 0.5f);
}

float CalculateNodeSize(uint level) {
    return 1.0f / (1u << level);  // level 0 = 1.0, level 1 = 0.5, level 2 = 0.25
}

// 边界检测：检查节点是否在边界上
bool IsBoundaryNode(uint3 nodePos, uint level) {
    uint levelSize = 64u >> (level + 1);
    
    // 检查6个邻居方向
    int3 directions[6] = {
        int3(1, 0, 0), int3(-1, 0, 0),   // X方向
        int3(0, 1, 0), int3(0, -1, 0),   // Y方向
        int3(0, 0, 1), int3(0, 0, -1)    // Z方向
    };
    
    for (uint dir = 0; dir < 6; dir++) {
        int3 neighborPos = int3(nodePos) + directions[dir];
        
        // 边界检查
        if (any(neighborPos < 0) || any(neighborPos >= int(levelSize))) {
            return true;  // 超出边界，认为是边界节点
        }
        
        uint neighborState = octreeTextures[level][uint3(neighborPos)];
        if (neighborState != NODE_SOLID) {
            return true;  // 有非SOLID邻居，是边界节点
        }
    }
    
    return false;  // 所有邻居都是SOLID，不是边界节点
}

// 复杂度计算：基于邻域变化
float CalculateComplexity(uint3 nodePos, uint level) {
    uint levelSize = 64u >> (level + 1);
    float complexity = 0.0f;
    uint sampleCount = 0;
    
    // 3x3x3邻域采样
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            for (int dz = -1; dz <= 1; dz++) {
                int3 samplePos = int3(nodePos) + int3(dx, dy, dz);
                
                if (all(samplePos >= 0) && all(samplePos < int(levelSize))) {
                    uint sampleState = octreeTextures[level][uint3(samplePos)];
                    if (sampleState == NODE_MIXED) {
                        complexity += 1.0f;
                    }
                    sampleCount++;
                }
            }
        }
    }
    
    return sampleCount > 0 ? complexity / float(sampleCount) : 0.0f;
}

// 评分函数：为节点计算综合评分
float CalculateNodeScore(uint3 nodePos, uint level, float nodeSize) {
    float score = 0.0f;
    
    // 基础分数：根据策略
    switch (config.strategy) {
        case STRATEGY_ALL_SOLID:
            score = 1.0f;  // 所有SOLID节点评分相同
            break;
            
        case STRATEGY_BOUNDARY_ONLY:
            score = IsBoundaryNode(nodePos, level) ? 1.0f : 0.0f;
            break;
            
        case STRATEGY_SIZE_BASED:
            score = nodeSize * config.sizeWeight;
            break;
            
        case STRATEGY_COMPLEXITY:
            score = CalculateComplexity(nodePos, level);
            break;
            
        case STRATEGY_CUSTOM:
            // 自定义评分逻辑（可通过customFlags控制）
            float complexity = CalculateComplexity(nodePos, level);
            float sizeScore = nodeSize * config.sizeWeight;
            float depthScore = (float(config.maxLevel - level) / float(config.maxLevel)) * config.depthWeight;
            
            score = complexity * config.complexityThreshold + 
                    sizeScore + 
                    depthScore * config.depthWeight;
            break;
    }
    
    return score;
}

// 主计算着色器 - 重写为每线程处理一个节点
[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    uint threadId = id.x;
    
    // 计算总的节点偏移，遍历所有层级找到该线程对应的节点
    uint nodeOffset = 0;
    uint targetLevel = 0;
    uint targetIndex = 0;
    
    // 找到当前线程对应的层级和节点索引
    // 优化：从高层级到低层级处理，优先选择大节点
    for (uint level = config.maxLevel; level >= config.minLevel && level <= config.maxLevel; level--) {
        uint levelSize = 64u >> (level + 1);
        uint nodesThisLevel = levelSize * levelSize * levelSize;
        
        if (threadId < nodeOffset + nodesThisLevel) {
            targetLevel = level;
            targetIndex = threadId - nodeOffset;
            break;
        }
        nodeOffset += nodesThisLevel;
    }
    
    // 如果线程ID超出了所有节点范围，退出
    if (targetLevel < config.minLevel || targetLevel > config.maxLevel) {
        return;
    }
    
    // 计算节点位置
    uint3 nodePos = DecodeNodePosition(targetIndex, targetLevel);
    uint levelSize = 64u >> (targetLevel + 1);
    
    // 检查边界
    if (any(nodePos >= levelSize)) return;
    
    // 读取节点状态 - 使用对应的mip level
    uint nodeState = octreeTextures[targetLevel][nodePos];
    
    // 基础过滤：根据策略处理不同类型节点
    bool shouldProcess = false;
    
    if (config.strategy == STRATEGY_ALL_SOLID) {
        shouldProcess = (nodeState == NODE_SOLID);
    } else if (config.strategy == STRATEGY_BOUNDARY_ONLY) {
        shouldProcess = (nodeState == NODE_MIXED || nodeState == NODE_SOLID); // 边界更可能在MIXED区域
    } else {
        shouldProcess = (nodeState == NODE_SOLID || nodeState == NODE_MIXED); // 其他策略：处理有内容的节点
    }
    
    if (shouldProcess) {
        // 原子递增候选计数
        InterlockedAdd(stats[0].totalCandidates, 1);
        
        float nodeSize = CalculateNodeSize(targetLevel);
        
        // 大小过滤
        if (nodeSize >= config.minNodeSize) {
            // 计算节点评分
            float score = CalculateNodeScore(nodePos, targetLevel, nodeSize);
            
            // 评分过滤（阈值检查）
            if (score > 0.0f) {
                // 原子获取输出索引
                uint outputIndex;
                InterlockedAdd(atomicCounter[0], 1, outputIndex);
                
                // 检查是否超出最大节点数限制
                if (outputIndex < config.maxNodes) {
                    // 创建选中节点
                    SelectedNode node;
                    node.center = CalculateNodeCenter(nodePos, targetLevel);
                    node.size = nodeSize;
                    node.position = nodePos;
                    node.level = targetLevel;
                    node.nodeId = (targetLevel << 24) | targetIndex;  // 打包level和index
                    node.score = score;
                    node.metadata = 0;  // 可用于存储额外信息
                    node.reserved = 0;
                    
                    // 写入结果buffer
                    selectedNodes[outputIndex] = node;
                    
                    // 更新统计信息
                    InterlockedAdd(stats[0].selectedCount, 1);
                    
                    if (config.strategy == STRATEGY_BOUNDARY_ONLY && IsBoundaryNode(nodePos, targetLevel)) {
                        InterlockedAdd(stats[0].boundaryNodes, 1);
                    } else {
                        InterlockedAdd(stats[0].interiorNodes, 1);
                    }
                }
            }
        }
    }
}