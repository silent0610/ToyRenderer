// Multi-View Depth Cube Fragment Shader (Pure Vertex Shader Solution)
// Stage 4B: Output distance from camera center for SDF generation without geometry shader

struct FSInput {
    float4 position : SV_POSITION;
    [[vk::location(0)]] float3 worldPos : POSITION0;
    [[vk::location(1)]] float3 cameraPos : POSITION1;
};

float main(FSInput input) : SV_TARGET {
    // Calculate distance from world position to camera center
    float3 lightVec = input.worldPos - input.cameraPos;
    float distance = length(lightVec);
    
    // Output distance as depth value
    return distance;
}