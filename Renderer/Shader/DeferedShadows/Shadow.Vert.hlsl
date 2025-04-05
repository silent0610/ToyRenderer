// Copyright 2020 Google LLC

struct VSOutput
{
	float4 Pos : SV_POSITION;
};

VSOutput main([[vk::location(0)]] float4 Pos : POSITION0)
{
	VSOutput output = (VSOutput)0;
	output.Pos = Pos;
	return output;
}