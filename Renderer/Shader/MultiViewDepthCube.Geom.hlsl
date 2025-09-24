// Multi-View Depth Cube Geometry Shader
// Stage 4B: Generates 6 cubemap faces for each camera

struct VSOutput {
    float4 position : SV_POSITION;
    [[vk::location(0)]] float3 worldPos : POSITION0;
    [[vk::location(1)]] uint cameraIndex : TEXCOORD0;
};

struct GSOutput {
    float4 position : SV_POSITION;
    [[vk::location(0)]] float3 worldPos : POSITION0;
    [[vk::location(1)]] float3 cameraPos : POSITION1;
    uint renderTargetIndex : SV_RenderTargetArrayIndex;  // Which cubemap face
};

// Camera data
struct CameraMatrix {
    float4x4 viewMatrix;
    float3 cameraPosition;
    float padding;
};

// GPU准备的相机矩阵数据 (使用storage buffer而不是constant buffer)
[[vk::binding(0, 0)]] StructuredBuffer<CameraMatrix> cameras : register(t0);

// Push constants
struct PushConstants {
    float4x4 modelMatrix;
    float4x4 projMatrix;
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

[maxvertexcount(18)]  // 6 faces * 3 vertices per triangle
void main(triangle VSOutput input[3], inout TriangleStream<GSOutput> outputStream) {
    uint cameraIndex = input[0].cameraIndex;
    
    // For each cube face
    for (uint face = 0; face < 6; face++) {
        // Calculate render target index: cameraIndex * 6 + face
        uint rtIndex = cameraIndex * 6 + face;
        
        // Get camera position
        float3 cameraPos = cameras[cameraIndex].cameraPosition;
        
        // Create view matrix for this cube face centered at camera position
        float4x4 faceViewMatrix = cubeViewMatrices[face];
        
        // Translate to camera position
        float4x4 translatedViewMatrix = faceViewMatrix;
        translatedViewMatrix[0][3] = -dot(faceViewMatrix[0].xyz, cameraPos);
        translatedViewMatrix[1][3] = -dot(faceViewMatrix[1].xyz, cameraPos);
        translatedViewMatrix[2][3] = -dot(faceViewMatrix[2].xyz, cameraPos);
        
        // Output triangle for this face
        for (uint i = 0; i < 3; i++) {
            GSOutput output = (GSOutput)0;
            
            // Transform world position to this cube face's view space
            float4 worldPos = float4(input[i].worldPos, 1.0);
            float4 viewPos = mul(translatedViewMatrix, worldPos);
            
            // Apply projection
            output.position = mul(pushConsts.projMatrix, viewPos);
            output.worldPos = input[i].worldPos;
            output.cameraPos = cameraPos;
            output.renderTargetIndex = rtIndex;
            
            outputStream.Append(output);
        }
        
        outputStream.RestartStrip();
    }
}