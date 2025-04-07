TextureCube textureEnv : register(t1);
SamplerState samplerEnv : register(s1);


struct FSOutput
{
	float4 Position : SV_TARGET0;
	float4 Normal : SV_TARGET1;
	float4 Albedo : SV_TARGET2;
};

FSOutput main([[vk::location(0)]] float3 inUVW : POSITION0) 
{
	FSOutput output = (FSOutput)0;
	float3 color = textureEnv.Sample(samplerEnv, inUVW).rgb;

	output.Albedo = float4(color ,1.0f);
	return output;
}