// 顶点着色器输出结构
struct VSOutput {
    float4 Pos   : SV_POSITION;
    float4 Color : COLOR0;
    float2 UV    : TEXCOORD0; 
};
[[vk::binding(0, 0)]] Texture2D myTexture;
[[vk::binding(0, 0)]] SamplerState mySampler;
float4 main(VSOutput input) : SV_Target {
     
    float4 texColor = myTexture.Sample(mySampler, input.UV);
    
    // 混合 PushConstant 颜色
    return texColor ;
}