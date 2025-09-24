struct PointLight {
    float4 pos;
    float4 color;
    float radius;
    float intensity;
    uint castShadow;
    int index;
};
StructuredBuffer<PointLight> lights : register(t0);

struct UBO
{
    float4x4 view;
    float4x4 proj;
    float2 screenSize;
    int tileSize;
    int numTilesX;
    int numTilesY;
    int numLights;
};
cbuffer ubo : register(b1)
{
    UBO ubo;
}
[[vk::constant_id(0)]] const uint MAX_LIGHTS_PER_TILE = 0;
struct Tile
{
    int lightIndices[10];
    int lightCount;
};
RWStructuredBuffer<Tile> tiles : register(u2);

[numthreads(8, 8, 1)] void main(uint3 threadId : SV_DISPATCHTHREADID, uint groupIndex : SV_GroupIndex)
{
    uint2 tileID = threadId.xy;
    uint tileIndex = tileID.y * ubo.numTilesX + tileID.x;
    // 计算 tile 的最小和最大坐标
    float2 tileMin = float2(tileID * ubo.tileSize);
    float2 tileMax = tileMin + float2(ubo.tileSize, ubo.tileSize);

    uint lightCount = 0;

    for (uint i = 0; i < ubo.numLights; ++i)
    {
        float3 lightViewPos = mul(ubo.view, float4(lights[i].pos.xyz, 1.0)).xyz;

        // 近似：把光源投影到屏幕空间
        float4 clipPos = mul(ubo.proj, float4(lightViewPos, 1.0));
        float2 screenPos = ((clipPos.xy / clipPos.w) * 0.5 + 0.5) * ubo.screenSize;

        // 计算光源在屏幕空间的半径
        // 这里假设光源是一个球体，半径在 View 空间中是一个常数
        float radius = lights[i].radius;
        float3 offset = lightViewPos + float3(radius, 0, 0); // 在 View 空间 X 方向偏移
        float4 clipOffset = mul(ubo.proj, float4(offset, 1.0));
        float2 screenOffset = ((clipOffset.xy / clipOffset.w) * 0.5 + 0.5) * ubo.screenSize;
        float screenRadius = length(screenOffset - screenPos);

        float2 closestPoint = clamp(screenPos, tileMin, tileMax);
        float2 delta = closestPoint - screenPos;
        bool cross = dot(delta, delta) <= screenRadius * screenRadius;
        if (!cross)
        {
            continue;
        }
        else if (lightCount < MAX_LIGHTS_PER_TILE)
        {
            tiles[tileIndex].lightIndices[lightCount] = i;
            lightCount++;
        }
    }
    tiles[tileIndex].lightCount = lightCount;
}