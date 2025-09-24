// Multi-View Depth Cube Vertex Shader (Pure Vertex Shader Solution)
// Stage 4B: Multi-view depth rendering for SDF generation without geometry shader

struct VSInput {
    [[vk::location(0)]] float3 position : POSITION0;
};

struct VSOutput {
    float4 position : SV_POSITION;
    [[vk::location(0)]] float3 worldPos : POSITION0;
    [[vk::location(1)]] float3 cameraPos : POSITION1;
    uint renderTargetIndex : SV_RenderTargetArrayIndex;  // Which cubemap face
};

// Camera matrices buffer - contains view matrices for all cameras
struct CameraMatrix {
    float4x4 viewMatrix;
    float3 cameraPosition;
    float padding;
};

// GPU准备的相机矩阵数据 (使用storage buffer而不是constant buffer)
[[vk::binding(0, 0)]] StructuredBuffer<CameraMatrix> cameras : register(t0);

// Push constants for model transformation
struct PushConstants {
    float4x4 modelMatrix;
    float4x4 projMatrix;  // Perspective projection for cubemap rendering
};
[[vk::push_constant]] PushConstants pushConsts;

// Cube face view matrices (relative to camera center)
static const float4x4 cubeViewMatrices[6] = {
    // +X face (right)
    float4x4( 0, 0, -1, 0,
              0, -1, 0, 0,
             -1, 0, 0, 0,
              0, 0, 0, 1),
    // -X face (left)
    float4x4( 0, 0, 1, 0,
              0, -1, 0, 0,
              1, 0, 0, 0,
              0, 0, 0, 1),
    // +Y face (top)
    float4x4( 1, 0, 0, 0,
              0, 0, 1, 0,
              0, -1, 0, 0,
              0, 0, 0, 1),
    // -Y face (bottom)
    float4x4( 1, 0, 0, 0,
              0, 0, -1, 0,
              0, 1, 0, 0,
              0, 0, 0, 1),
    // +Z face (front)
    float4x4( 1, 0, 0, 0,
              0, -1, 0, 0,
              0, 0, -1, 0,
              0, 0, 0, 1),
    // -Z face (back)
    float4x4(-1, 0, 0, 0,
              0, -1, 0, 0,
              0, 0, 1, 0,
              0, 0, 0, 1)
};

VSOutput main(VSInput input, uint instanceId : SV_InstanceID) {
    VSOutput output = (VSOutput)0;
    
    // Decode instance ID: camera * 6 + face
    uint cameraIndex = instanceId / 6;
    uint faceIndex = instanceId % 6;
    
    // Calculate world position
    float4 worldPos = mul(pushConsts.modelMatrix, float4(input.position, 1.0));
    output.worldPos = worldPos.xyz;
    
    // Get camera position
    float3 cameraPos = cameras[cameraIndex].cameraPosition;
    output.cameraPos = cameraPos;
    
    // Create view matrix for this cube face centered at camera position
    float4x4 faceViewMatrix = cubeViewMatrices[faceIndex];
    
    // Translate to camera position
    float4x4 translatedViewMatrix = faceViewMatrix;
    translatedViewMatrix[0][3] = -dot(faceViewMatrix[0].xyz, cameraPos);
    translatedViewMatrix[1][3] = -dot(faceViewMatrix[1].xyz, cameraPos);
    translatedViewMatrix[2][3] = -dot(faceViewMatrix[2].xyz, cameraPos);
    
    // Transform to view space
    float4 viewPos = mul(translatedViewMatrix, worldPos);
    
    // Apply projection
    output.position = mul(pushConsts.projMatrix, viewPos);
    
    // Set render target index: cameraIndex * 6 + face
    output.renderTargetIndex = instanceId;
    
    return output;
}