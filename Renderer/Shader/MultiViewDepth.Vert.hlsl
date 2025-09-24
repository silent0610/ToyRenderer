// MultiViewDepth.Vert.hlsl
// Stage 4C Multi-View Depth Rendering: Vertex Shader
// Renders model geometry from multiple cameras to cubemap array using Vulkan Multiview

// HLSL Multiview 支持

// 顶点着色器输入
struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;      // 可选，用于法线剔除优化
    float2 texCoord : TEXCOORD;  // 可选，暂时不用
};

// 顶点着色器输出 - 简化接口，只传递必要数据给片段着色器
struct VertexOutput {
    float4 position : SV_POSITION;
    uint renderTargetIndex : SV_RenderTargetArrayIndex; // 指定输出到哪个layer

    // Use explicit locations for all interpolated outputs
    [[vk::location(0)]] float3 worldPosition : WORLD_POS;
    [[vk::location(1)]] float3 cameraPosition : CAMERA_POS;
};

// 相机矩阵结构体 (与C++ GPUDataPreparation4C匹配)
struct CameraMatrix {
    float4 cameraPosition;  // 相机位置 (w分量未使用)
};

// 模型矩阵结构体
struct ModelMatrix {
    float4x4 matrix;        // 模型变换矩阵
};

// Vulkan右手坐标系的立方体贴图6个面的标准方向向量
static const float3 CUBE_DIRECTIONS[6] = {
    float3(-1,  0,  0),  // +X面：看向-X方向（左）
    float3(1,   0,  0),  // -X面：看向+X方向（右）
    float3(0,   1,  0),  // +Y面：看向+Y方向（下，Vulkan Y向下）  
    float3(0,  -1,  0),  // -Y面：看向-Y方向（上，Vulkan Y向下）
    float3(0,   0, -1),  // +Z面：看向-Z方向（前）
    float3(0,   0,  1)   // -Z面：看向+Z方向（后）
};

static const float3 CUBE_UP_VECTORS[6] = {
    float3(0, -1,  0),   // +X面：向右看时，上方向为-Y（Vulkan坐标系Y向下）
    float3(0, -1,  0),   // -X面：向左看时，上方向为-Y
    float3(0,  0, -1),   // +Y面：向上看时，上方向为-Z（前方）
    float3(0,  0,  1),   // -Y面：向下看时，上方向为+Z（后方）
    float3(0, -1,  0),   // +Z面：向前看时，上方向为-Y
    float3(0, -1,  0)    // -Z面：向后看时，上方向为-Y
};

// 资源绑定
[[vk::binding(0, 0)]] StructuredBuffer<CameraMatrix> cameraMatrices;
[[vk::binding(1, 0)]] StructuredBuffer<ModelMatrix> modelMatrices;

// Push Constants
struct PushConstants {
    float4x4 projectionMatrix;  // 统一的透视投影矩阵
    uint totalPartCount;        // 总子部件数量
    uint activeCameraCount;     // 活跃相机数量
    uint totalDrawCommands;     // 总绘制命令数 (partCount * cameraCount * 6)
    uint baseInstanceID;        // 基础实例ID用于解码
    uint2 _padding;             // 16字节对齐
};
[[vk::push_constant]] PushConstants pushConsts;

// 辅助函数：构建Vulkan右手坐标系LookAt矩阵
float4x4 BuildLookAtMatrix(float3 eye, float3 target, float3 up) {
    // 右手坐标系：相机看向-Z方向
    float3 f = normalize(target - eye);  // 前方向（从eye指向target）
    float3 s = normalize(cross(f, up));  // 右方向  
    float3 u = cross(s, f);              // 上方向

    // 右手坐标系视图矩阵：需要取反f，因为相机看向-Z方向
    return float4x4(
        s.x,  s.y,  s.z, -dot(s, eye),
        u.x,  u.y,  u.z, -dot(u, eye),
        -f.x, -f.y, -f.z, dot(f, eye),   // 正确：右手坐标系取反f
        0,    0,    0,    1);
}

VertexOutput main(VertexInput input, uint instanceID : SV_InstanceID) {
    VertexOutput output;

    // 1. 解码实例索引获取相机和面索引
    // New design: Each indirect command renders multiple instances (instanceCount = cameraCount * 6)
    // SV_InstanceID = firstInstance + instanceID
    // firstInstance = partIndex * (cameraCount * 6), instanceID = 0 to (cameraCount * 6 - 1)

    uint totalCameras = pushConsts.activeCameraCount;
    uint totalFaces = 6;
    uint camerasAndFaces = totalCameras * totalFaces;

    // Decode part index from firstInstance
    uint partIndex = instanceID / camerasAndFaces;

    // Decode camera and face from instanceID within this part
    uint instanceWithinPart = instanceID % camerasAndFaces;
    uint cameraIndex = instanceWithinPart / totalFaces;
    uint faceIndex = instanceWithinPart % totalFaces;

    // 边界检查
    if (partIndex >= pushConsts.totalPartCount || 
        cameraIndex >= totalCameras || 
        faceIndex >= totalFaces) {
        // 输出退化三角形，移到远平面外避免影响深度缓冲
        output.position = float4(0, 0, -1, 1);
        output.worldPosition = float3(0, 0, 0);
        output.cameraPosition = float3(0, 0, 0);
        output.renderTargetIndex = 0; // 默认输出到第一层
        return output;
    }

    // 2. 计算正确的输出layer：cameraIndex * 6 + faceIndex
    // 每个相机占用6个连续的layer (cubemap的6个面)
    output.renderTargetIndex = cameraIndex * 6 + faceIndex;

    // 3. 构建模型矩阵 (M) - Use identity since vertices are pre-transformed
    float4x4 modelMatrix = float4x4(
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1);

    // 4. 获取相机位置并构建视图矩阵 (V)
    // 每个相机从其位置看向cubemap的6个标准方向

    // 使用原来的相机位置
    float3 originalCameraPos = cameraMatrices[cameraIndex].cameraPosition.xyz;
    float3 cameraPos = originalCameraPos;
    float3 lookDir = CUBE_DIRECTIONS[faceIndex];
    float3 upDir = CUBE_UP_VECTORS[faceIndex];

    float4x4 viewMatrix = BuildLookAtMatrix(cameraPos, cameraPos + lookDir, upDir);

    // 5. 计算世界坐标
    float4 worldPos = mul(modelMatrix, float4(input.position, 1.0));
    output.worldPosition = worldPos.xyz;
    output.cameraPosition = cameraPos;

    // 6. 计算最终裁剪坐标 (P * V * M * vertex)
    // Apply Y-axis flip to projection matrix to correct coordinate system
    float4x4 projMatrix = pushConsts.projectionMatrix;
    //projMatrix[1][1] = -projMatrix[1][1]; // Flip Y axis

    float4x4 mvpMatrix = mul(projMatrix, mul(viewMatrix, modelMatrix));
    output.position = mul(mvpMatrix, float4(input.position, 1.0));

    // 7. Debug: Pass view matrix first row for verification  

    // 8. Output debug information

    return output;
}