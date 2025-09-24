// Copyright 2020 Google LLC
[[vk::constant_id(0)]] uint LIGHT_COUNT = 0;
#define MAX_LIGHT_COUNT 10

struct UBO {
    float4x4 mvp[MAX_LIGHT_COUNT];
};

cbuffer ubo : register(b0) {
    UBO ubo;
}

struct VSOutput {
    float4 Pos : SV_POSITION;
};

struct GSOutput {
    float4 Pos : SV_POSITION;
    int Layer : SV_RenderTargetArrayIndex;
};

[maxvertexcount(3*LIGHT_COUNT)] // 3 layers × 3 vertices !!!
void
main(triangle VSOutput input[3], inout TriangleStream<GSOutput> outStream) {
    for (int layer = 0; layer < LIGHT_COUNT; layer++) {
        for (int i = 0; i < 3; i++) {
            GSOutput output = (GSOutput)0;
            output.Pos = mul(ubo.mvp[layer], input[i].Pos);
            output.Layer = layer;
            outStream.Append(output);
        }
        outStream.RestartStrip();
    }
}