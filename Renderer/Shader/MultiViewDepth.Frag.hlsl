// MultiViewDepth.Frag.hlsl
// Stage 4C Multi-View Depth Rendering: Fragment Shader
// Calculates linear depth values for SDF fusion

// 片段着色器输入 (从顶点着色器插值而来)
struct VertexOutput {
    float4 position : SV_POSITION;
    uint renderTargetIndex : SV_RenderTargetArrayIndex;
    // Use explicit locations matching vertex shader
    [[vk::location(0)]] float3 worldPosition : WORLD_POS;
    [[vk::location(1)]] float3 cameraPosition : CAMERA_POS;
};

// Push Constants (与顶点着色器共享)
struct PushConstants {
    float4x4 projectionMatrix;
    uint totalPartCount;
    uint activeCameraCount;
    uint _padding;
};
[[vk::push_constant]] PushConstants pushConsts;

float main(VertexOutput input) : SV_Target {
    // 计算当前片段到相机的线性距离（世界空间深度）
    float3 dir = input.worldPosition - input.cameraPosition;
    float depth = length(dir);  // 计算世界空间距离

    // 返回线性深度值
    return depth;
}