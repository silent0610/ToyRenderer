// Simple model fragment shader for glTF rendering
struct PSInput
{
    float4 Pos : SV_POSITION;
    [[vk::location(0)]] float3 Normal : NORMAL0;
    [[vk::location(1)]] float2 UV : TEXCOORD0;
    [[vk::location(2)]] float4 Color : COLOR0;
    [[vk::location(3)]] float3 WorldPos : POSITION0;
};

struct PSOutput
{
    float4 Color : SV_TARGET0;
};

PSOutput main(PSInput input)
{
    PSOutput output = (PSOutput)0;
    
    // Simple lighting calculation
    float3 normal = normalize(input.Normal);
    float3 lightDir = normalize(float3(1.0, 1.0, 1.0)); // Simple directional light
    float3 lightColor = float3(1.0, 1.0, 1.0);
    
    // Ambient lighting
    float3 ambient = 0.1 * lightColor;
    
    // Diffuse lighting
    float diffuse = max(dot(normal, lightDir), 0.0);
    float3 diffuseColor = diffuse * lightColor;
    
    // Combine with vertex color
    float3 finalColor = (ambient + diffuseColor) * input.Color.rgb;
    
    output.Color = float4(finalColor, input.Color.a);
    return output;
}