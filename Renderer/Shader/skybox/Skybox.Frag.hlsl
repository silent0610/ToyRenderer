Texture2D textureLighting : register(t1);
SamplerState samplerLighting : register(s1);
TextureCube textureEnv : register(t2);
SamplerState samplerEnv : register(s2);

struct UBO
{
	float2 resolution;
	float4x4 model;
	float4x4 projection;
};
cbuffer ubo : register(b0) { UBO ubo; }

struct FSOutput
{
	float4 lighting : SV_TARGET0;
	float4 highLight : SV_TARGET1;
};
struct VSOutput
{
	float4 pos : SV_POSITION;
	[[vk::location(0)]] float3 inUVW : POSITION0;
};
FSOutput main(VSOutput input)
{
	FSOutput output = (FSOutput)0;

	float2 uv = input.pos.rg;
	uv.r = uv.r / ubo.resolution.x;
	uv.g = uv.g / ubo.resolution.y;

	float4 lighting = textureLighting.Sample(samplerLighting, uv);
	float3 cube = textureEnv.Sample(samplerEnv, input.inUVW).rgb;

	if (length(lighting.rgb) < 1e-5)
	{
		output.lighting = float4(cube, 1);
	}
	else
	{
		output.lighting = lighting;
	}
	float luminance = dot(output.lighting.rgb, float3(0.2126, 0.7152, 0.0722));
	if (luminance > 0.3f)
	{
		output.highLight = output.lighting;
	}

	return output;
}
