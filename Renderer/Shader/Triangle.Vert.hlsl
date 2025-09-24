// Simple hardcoded triangle vertex shader
struct VSOutput
{
    float4 Pos : SV_POSITION;
    [[vk::location(0)]] float3 Color : COLOR0;
};

VSOutput main(uint vertexId : SV_VertexID)
{
    VSOutput output = (VSOutput)0;
    
    // Hardcoded triangle vertices
    float2 positions[3] = {
        float2(0.0, -0.5),   // bottom center
        float2(0.5, 0.5),    // top right
        float2(-0.5, 0.5)    // top left
    };
    
    float3 colors[3] = {
        float3(1.0, 0.0, 0.0),  // red
        float3(0.0, 1.0, 0.0),  // green
        float3(0.0, 0.0, 1.0)   // blue
    };
    
    output.Pos = float4(positions[vertexId], 0.0, 1.0);
    output.Color = colors[vertexId];
    return output;
}