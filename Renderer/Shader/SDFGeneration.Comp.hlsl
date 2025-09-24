// 简单高效的SDF生成 - 直接从立方体节点解析计算SDF
// 编译命令: dxc -spirv -T cs_6_0 -E main SDFGeneration.Comp.hlsl -Fo SDFGeneration.Comp.spv

// SDF生成配置
struct SDFConfig {
    uint outputResolution;       // SDF纹理分辨率 (256)
    uint nodeCount;              // 输入节点数量
    float worldScale;            // 世界坐标缩放 (1.0)
    float reserved;              // 预留字段
};

// 选中的节点结构 (与NodeSelection shader一致)
struct SelectedNode {
    float3 center;              // 节点中心位置
    float size;                 // 节点大小
    uint3 position;             // 八叉树位置
    uint level;                 // 层级
    uint nodeId;                // 节点ID  
    float score;                // 评分
    uint metadata;              // 元数据
    uint reserved;              // 预留
};

// 绑定资源
ConstantBuffer<SDFConfig> config : register(b0);
StructuredBuffer<SelectedNode> selectedNodes : register(t1);    // 输入：GPU选择的节点
RWByteAddressBuffer counterBuffer : register(u2);               // GPU计数器：实际节点数量
RWTexture3D<float> sdfTexture : register(u3);                  // 输出：SDF纹理

// 立方体SDF函数 - 解析计算
float CubeSDF(float3 pos, float3 center, float size) {
    float halfSize = size * 0.5f;
    float3 dist = abs(pos - center) - float3(halfSize, halfSize, halfSize);
    
    // 内部距离 + 外部距离
    return length(max(dist, 0.0)) + min(max(dist.x, max(dist.y, dist.z)), 0.0);
}

// 计算点到所有节点的最短距离 (Union操作)
float CalculateSDF(float3 worldPos) {
    float minDistance = 999999.0f;
    
    // 从GPU counter buffer读取实际节点数量
    uint actualNodeCount = counterBuffer.Load(0);
    actualNodeCount = min(actualNodeCount, config.nodeCount);  // 安全边界检查
    
    // 遍历实际选中的节点
    for (uint i = 0; i < actualNodeCount; i++) {
        SelectedNode node = selectedNodes[i];
        
        // 计算到当前立方体的距离
        float distance = CubeSDF(worldPos, node.center, node.size);
        
        // Union操作：取最小距离
        minDistance = min(minDistance, distance);
    }
    
    return minDistance;
}

// 主计算着色器
[numthreads(8, 8, 8)]
void main(uint3 id : SV_DispatchThreadID) {
    // 边界检查
    if (any(id >= config.outputResolution)) {
        return;
    }
    
    // 将纹理坐标转换为世界坐标
    float3 texCoord = float3(id) / float(config.outputResolution - 1);
    
    // 映射到世界坐标范围 (假设[-1,1]立方体)
    float3 worldPos = (texCoord * 2.0f - 1.0f) * config.worldScale;
    
    // 直接计算SDF距离 - 无需ray marching
    float sdfDistance = CalculateSDF(worldPos);
    
    // 写入SDF纹理
    sdfTexture[id] = sdfDistance;
}