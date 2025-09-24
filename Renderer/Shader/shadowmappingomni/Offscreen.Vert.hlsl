// Copyright 2020 Google LLC

struct VSOutput
{
	float4 Pos : SV_POSITION;
[[vk::location(0)]] float4 WorldPos : POSITION1;
[[vk::location(1)]] float3 LightPos : POSITION2;
};

struct LightDesc
{
	float4x4 proj;
	float4x4 view;
	float4x4 model;
	float4 lightPos;
};

cbuffer CBuffer : register(b0) { LightDesc light[100]; }

struct PushConsts
{
	float4x4 view;
	uint index;
	uint pad1;
	uint pad2;
	uint pad3;
};
[[vk::push_constant]] PushConsts pushConsts;

VSOutput main([[vk::location(0)]] float3 Pos : POSITION0)
{
	VSOutput output = (VSOutput)0;
	uint index = pushConsts.index;
	output.Pos = mul(light[index].proj, mul(pushConsts.view, mul(light[index].model, float4(Pos, 1.0))));

	output.WorldPos = float4(Pos, 1.0);
	output.LightPos = light[index].lightPos.xyz;
	return output;
}