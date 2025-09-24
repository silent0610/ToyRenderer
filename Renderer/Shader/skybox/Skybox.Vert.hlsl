// Copyright 2020 Google LLC

struct VSInput {
    [[vk::location(0)]] float3 Pos : POSITION0;
};

struct CameraInfos {
    float4x4 View;
    float4x4 Proj;
    float4x4 ProjView;
    float4x4 InvView;
    float4x4 InvProj;
    float4x4 InvProjView;
    float4 CameraPos;
    float ZNear;
    float ZFar;
    float2 ScreenSize;
    float2 InvScreenSize;
};
cbuffer CBCamera : register(b0) {
    CameraInfos CBCamera;
}

struct VSOutput {
    float4 Pos : SV_POSITION;
    [[vk::location(0)]] float3 UVW : TEXCOORD0;
};

VSOutput main(VSInput input) {
    VSOutput output = (VSOutput)0;
    output.UVW = input.Pos;
    output.UVW.y = -output.UVW.y;
    output.Pos = mul(CBCamera.Proj, float4(mul((float3x3)CBCamera.View, input.Pos.xyz),1.0f));
    return output;
}
