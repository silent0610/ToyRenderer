struct VSOutput
{
    float4 pos : SV_POSITION;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};

VSOutput main(uint VertexIndex : SV_VertexID)
{
    VSOutput output = (VSOutput)0;
    // 生成覆盖整个屏幕的三角形
    output.uv = float2((VertexIndex << 1) & 2, VertexIndex & 2);
    output.pos = float4(output.uv * 2.0f - 1.0f, 0.0f, 1.0f);
    return output;
}