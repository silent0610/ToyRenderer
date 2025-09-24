// Copyright 2020 Google LLC

struct VSOutput
{
	float4 Pos : SV_POSITION;
[[vk::location(0)]] float4 WorldPos : POSITION0;
[[vk::location(1)]] float3 LightPos : POSITION1;
};

struct LightDesc
{
	float4x4 proj;
};

cbuffer CBuffer : register(b0) { LightDesc light; }

struct PushConsts
{
	float4x4 view;
    float4x4 model;
    float3 pos;
};
[[vk::push_constant]] PushConsts pushConsts;

VSOutput main([[vk::location(0)]] float3 Pos : POSITION0)
{
	VSOutput output = (VSOutput)0;

	output.Pos = mul(light.proj, mul(pushConsts.view, mul(pushConsts.model, float4(Pos, 1.0))));

	output.WorldPos = float4(Pos, 1.0);
	output.LightPos = pushConsts.pos.xyz;
	return output;
}