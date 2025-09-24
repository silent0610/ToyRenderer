struct VSInput {
    [[vk::location(0)]] float3 Pos : POSITION0;
};
// todo: pass via specialization constant
#define SHADOW_MAP_CASCADE_COUNT 4

struct PushConsts {
    int lightIndex;
    uint cascadeIndex;
};
[[vk::push_constant]] PushConsts pushConsts;

struct LightCascadeData {
    float4x4 cascadeViewProjMat[SHADOW_MAP_CASCADE_COUNT];
};
cbuffer ubo : register(b0) {
    LightCascadeData ubo[4];
}

struct VSOutput {
    float4 Pos : SV_POSITION;
};

VSOutput main(VSInput input) {
    VSOutput output = (VSOutput)0;
    float3 pos = input.Pos;
    output.Pos = mul(ubo[pushConsts.lightIndex].cascadeViewProjMat[pushConsts.cascadeIndex], float4(input.Pos, 1.0));
    return output;
}