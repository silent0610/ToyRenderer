// lighting.vert.hlsl

struct VSOutput
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
};

// 这是一个全屏三角形 Pass，不需要 Vertex Buffer
// 直接根据 VertexIndex 生成坐标
VSOutput main(uint VertexIndex : SV_VertexID)
{
    VSOutput output;
    
    // 魔法代码：生成全屏三角形
    // Vertex 0: (-1, -1), UV (0, 0)
    // Vertex 1: (-1,  3), UV (0, 2)
    // Vertex 2: ( 3, -1), UV (2, 0)
    float2 uv = float2((VertexIndex << 1) & 2, VertexIndex & 2);
    
    output.UV = uv;
    // Vulkan NDC: x[-1, 1], y[-1, 1], z[0, 1]
    output.Pos = float4(uv * 2.0f - 1.0f, 0.0f, 1.0f);
    
    return output;
}