// Copyright 2020 Google LLC

Texture2D textureposition : register(t1);
SamplerState samplerposition : register(s1);
Texture2D textureNormal : register(t2);
SamplerState samplerNormal : register(s2);
Texture2D textureAlbedo : register(t3);
SamplerState samplerAlbedo : register(s3);

Texture2DArray textureShadowMap : register(t5);
SamplerState samplerShadowMap : register(s5);

TextureCube textureIrradiance : register(t6);
SamplerState samplerIrradiance : register(s6);
Texture2D textureBRDFLUT : register(t7);
SamplerState samplerBRDFLUT : register(s7);
TextureCube prefilteredMapTexture : register(t8);
SamplerState prefilteredMapSampler : register(s8);

Texture2D textureMRAO : register(t9);
SamplerState samplerMRAO : register(s9);

// TextureCube textureCube : register(t10);
// SamplerState samplerCube : register(s10);
struct PushConsts
{
	[[vk::offset(0)]] float metallicFactor;
	[[vk::offset(4)]] float roughnessFactor;
};

[[vk::push_constant]] PushConsts materialFactor;
#define LIGHT_COUNT 3
#define SHADOW_FACTOR 0
#define AMBIENT_LIGHT 0.02
#define USE_PCF

struct Light
{
	float4 position;
	float4 target;
	float4 color;
	float4x4 viewMatrix;
};

struct UBO
{
	float4 viewPos;
	Light lights[LIGHT_COUNT];
	int useShadows;
	int displayDebugTarget;
};

cbuffer ubo : register(b4) { UBO ubo; }

float textureProj(float4 P, float layer, float2 offset)
{
	float shadow = 1.0;
	float4 shadowCoord = P / P.w;
	shadowCoord.xy = shadowCoord.xy * 0.5 + 0.5;

	if (shadowCoord.z > -1.0 && shadowCoord.z < 1.0)
	{
		float dist = textureShadowMap.Sample(samplerShadowMap, float3(shadowCoord.xy + offset, layer)).r;
		if (shadowCoord.w > 0.0 && dist < shadowCoord.z)
		{
			shadow = SHADOW_FACTOR;
		}
	}
	return shadow;
}

float filterPCF(float4 sc, float layer)
{
	int2 texDim;
	int elements;
	int levels;
	textureShadowMap.GetDimensions(0, texDim.x, texDim.y, elements, levels);
	float scale = 1.5;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 1;

	for (int x = -range; x <= range; x++)
	{
		for (int y = -range; y <= range; y++)
		{
			shadowFactor += textureProj(sc, layer, float2(dx * x, dy * y));
			count++;
		}
	}
	return shadowFactor / count;
}

float3 shadow(float3 fragcolor, float3 fragPos)
{
	for (int i = 0; i < LIGHT_COUNT; ++i)
	{
		float4 shadowClip = mul(ubo.lights[i].viewMatrix, float4(fragPos.xyz, 1.0));

		float shadowFactor;
#ifdef USE_PCF
		shadowFactor = filterPCF(shadowClip, i);
#else
		shadowFactor = textureProj(shadowClip, i, float2(0.0, 0.0));
#endif

		fragcolor *= shadowFactor;
	}
	return fragcolor;
}

float3 shadow1(float3 fragcolor, float3 fragPos, int index)
{

	float4 shadowClip = mul(ubo.lights[index].viewMatrix, float4(fragPos.xyz, 1.0));

	float shadowFactor;
#ifdef USE_PCF
	shadowFactor = filterPCF(shadowClip, index);
#else
	shadowFactor = textureProj(shadowClip, index, float2(0.0, 0.0));
#endif

	fragcolor *= shadowFactor;

	return fragcolor;
}
static const float PI = 3.14159265359;
float3 materialcolor()
{
	return float3(0.1, 0.1, 0.1);
}
// Normal Distribution function --------------------------------------
float D_GGX(float dotNH, float roughness)
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
	return (alpha2) / (PI * denom * denom);
}

// Geometric Shadowing function --------------------------------------
float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;
	float GL = dotNL / (dotNL * (1.0 - k) + k);
	float GV = dotNV / (dotNV * (1.0 - k) + k);
	return GL * GV;
}

// Fresnel function ----------------------------------------------------
float3 F_Schlick(float cosTheta, float3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
float3 F_SchlickR(float cosTheta, float3 F0, float roughness)
{
	return F0 + (max((1.0 - roughness).xxx, F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

// Specular BRDF composition --------------------------------------------
float3 BRDF(float3 L, float3 V, float3 N, float metallic, float roughness)
{
	// Precalculate vectors and dot products
	float3 H = normalize(V + L);
	float dotNV = clamp(dot(N, V), 0.0, 1.0);
	float dotNL = clamp(dot(N, L), 0.0, 1.0);
	float dotLH = clamp(dot(L, H), 0.0, 1.0);
	float dotNH = clamp(dot(N, H), 0.0, 1.0);

	// Light color fixed
	float3 lightColor = float3(1.0, 1.0, 1.0);

	float3 color = float3(0.0, 0.0, 0.0);

	if (dotNL > 0.0)
	{
		float rroughness = max(0.05, roughness);
		// D = Normal distribution (Distribution of the microfacets)
		float D = D_GGX(dotNH, roughness);
		// G = Geometric shadowing term (Microfacets shadowing)
		float G = G_SchlicksmithGGX(dotNL, dotNV, rroughness);
		// F = Fresnel factor (Reflectance depending on angle of incidence)
		float3 F = F_Schlick(dotNV, metallic);

		float3 spec = D * F * G / (4.0 * dotNL * dotNV);

		color += spec * dotNL * lightColor;
	}

	return color;
}
float3 prefilteredReflection(float3 R, float roughness)
{
	const float MAX_REFLECTION_LOD = 9.0; // todo: param/const
	float lod = roughness * MAX_REFLECTION_LOD;
	float lodf = floor(lod);
	float lodc = ceil(lod);
	float3 a = prefilteredMapTexture.SampleLevel(prefilteredMapSampler, R, lodf).rgb;
	float3 b = prefilteredMapTexture.SampleLevel(prefilteredMapSampler, R, lodc).rgb;
	return lerp(a, b, lod - lodf);
}
float3 specularContribution(float3 L, float3 V, float3 N, float3 F0, float3 albedo, float metallic, float roughness)
{
	// Precalculate vectors and dot products
	float3 H = normalize(V + L);
	float dotNH = clamp(dot(N, H), 0.0, 1.0);
	float dotNV = clamp(dot(N, V), 0.0, 1.0);
	float dotNL = clamp(dot(N, L), 0.0, 1.0);

	// Light color fixed
	float3 lightColor = float3(1.0, 1.0, 1.0);

	float3 color = float3(0.0, 0.0, 0.0);

	if (dotNL > 0.0)
	{
		// D = Normal distribution (Distribution of the microfacets)
		float D = D_GGX(dotNH, roughness);
		// G = Geometric shadowing term (Microfacets shadowing)
		float G = G_SchlicksmithGGX(dotNL, dotNV, roughness);
		// F = Fresnel factor (Reflectance depending on angle of incidence)
		float3 F = F_Schlick(dotNV, F0);
		float3 spec = D * F * G / (4.0 * dotNL * dotNV + 0.001);
		float3 kD = (float3(1.0, 1.0, 1.0) - F) * (1.0 - metallic);
		color += (kD * albedo / PI + spec) * dotNL;
	}

	return color;
}
float3 Uncharted2Tonemap(float3 x)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

struct FSOutput
{
	float4 lighting : SV_TARGET0;
};

FSOutput main([[vk::location(0)]] float2 inUV : TEXCOORD0)
{
	FSOutput output = (FSOutput)0;

	// float3 sttt = textureCube.Sample(samplerCube, float3(0, 0, 0)).rgb;
	// output.lighting = float4(sttt, 1);
	// return output;

	float3 fragPos = textureposition.Sample(samplerposition, inUV).rgb;

	if (length(fragPos) < 1e-5)
	{
		return output;
	}
	float4 albedo = textureAlbedo.Sample(samplerAlbedo, inUV);
	float3 normal = textureNormal.Sample(samplerNormal, inUV).rgb;
	float2 uv = inUV.rg;
	float4 MRAO = textureMRAO.Sample(samplerMRAO, uv);
	float metallic = materialFactor.metallicFactor * MRAO.r;
	float roughness = materialFactor.roughnessFactor * MRAO.g;

	float3 fragcolor = 0;

	// Debug display
	if (ubo.displayDebugTarget > 0)
	{
		switch (ubo.displayDebugTarget)
		{
		case 1:
			fragcolor.rgb = shadow(float3(1.0, 1.0, 1.0), fragPos);
			break;
		case 2:
			fragcolor.rgb = fragPos;
			break;
		case 3:
			fragcolor.rgb = normal;
			break;
		case 4:
			fragcolor.rgb = albedo.rgb;
			break;
		case 5:
			fragcolor.rgb = albedo.aaa;
			break;
		case 6:
			fragcolor.rgb = MRAO.rgb;
			break;
		}
		output.lighting = float4(fragcolor, 1.0f);
		return output;
	}

	// Ambient part
	float3 evir = albedo.rgb * AMBIENT_LIGHT;

	float3 N = normalize(normal);
	float3 V = normalize(ubo.viewPos.xyz - fragPos);
	float3 R = reflect(-V, N);

	float3 F0 = float3(0.04, 0.04, 0.04);
	F0 = lerp(F0, albedo.rgb, metallic);
	float3 Lo = float3(0.0, 0.0, 0.0);

	for (int i = 0; i < LIGHT_COUNT; ++i)
	{
		float3 L = normalize(ubo.lights[i].position.xyz - fragPos);
		Lo += specularContribution(L, V, N, F0, albedo.rgb, metallic, roughness);
	}

	float2 brdf = textureBRDFLUT.Sample(samplerBRDFLUT, float2(max(dot(N, V), 0.0), roughness)).rg;
	float3 reflection = prefilteredReflection(R, roughness).rgb;
	float3 irradiance = textureIrradiance.Sample(samplerIrradiance, N).rgb;

	// Diffuse based on irradiance
	float3 diffuse = irradiance * albedo.rgb;

	float3 F = F_SchlickR(max(dot(N, V), 0.0), F0, roughness);
	// Specular reflectance
	float3 specular = reflection * (F * brdf.x + brdf.y);
	// Ambient part
	float3 kD = 1.0 - F;
	kD *= 1.0 - metallic;
	float3 ambient = (kD * diffuse + specular);

	float3 color = ambient + Lo;

	output.lighting = float4(color, 1);

	return output;
}