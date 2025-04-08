TextureCube textureEnv : register(t1);
SamplerState samplerEnv : register(s1);

struct FSOutput
{
	float4 Position : SV_TARGET0;
	float4 Normal : SV_TARGET1;
	float4 Albedo : SV_TARGET2;
};
float3 Uncharted2Tonemap(float3 color)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	float W = 11.2;
	return ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
}

#define EXPOSURE 4.5
#define GAMMA 2.2
FSOutput main([[vk::location(0)]] float3 inUVW : POSITION0)
{
	FSOutput output = (FSOutput)0;
	float3 color = textureEnv.Sample(samplerEnv, inUVW).rgb;
	color = Uncharted2Tonemap(color * EXPOSURE);
	color = color * (1.0f / Uncharted2Tonemap((11.2f).xxx));

	// Gamma correction
	color = pow(color, (1.0f / GAMMA).xxx);
	output.Albedo = float4(color, 1.0f);

	return output;
}
