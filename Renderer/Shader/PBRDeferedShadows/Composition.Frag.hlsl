// Copyright 2020 Google LLC

Texture2D textureposition : register(t1);
SamplerState samplerposition : register(s1);
Texture2D textureNormal : register(t2);
SamplerState samplerNormal : register(s2);
Texture2D textureAlbedo : register(t3);
SamplerState samplerAlbedo : register(s3);
// Depth from the light's point of view
// layout (binding = 5) uniform sampler2DShadow samplerShadowMap;
Texture2DArray textureShadowMap : register(t5);
SamplerState samplerShadowMap : register(s5);

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

float3 shadow1(float3 fragcolor, float3 fragPos,int index)
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
	return float3(0.1,0.1,0.1);
}
// Normal Distribution function --------------------------------------
float D_GGX(float dotNH, float roughness)
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
	return (alpha2)/(PI * denom*denom);
}

// Geometric Shadowing function --------------------------------------
float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;
	float GL = dotNL / (dotNL * (1.0 - k) + k);
	float GV = dotNV / (dotNV * (1.0 - k) + k);
	return GL * GV;
}

// Fresnel function ----------------------------------------------------
float3 F_Schlick(float cosTheta, float metallic)
{
	float3 F0 = lerp(float3(0.04, 0.04, 0.04), materialcolor(), metallic); // * material.specular
	float3 F = F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
	return F;
}

// Specular BRDF composition --------------------------------------------

float3 BRDF(float3 L, float3 V, float3 N, float metallic, float roughness)
{
	// Precalculate vectors and dot products
	float3 H = normalize (V + L);
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

float4 main([[vk::location(0)]] float2 inUV : TEXCOORD0) : SV_TARGET
{
	// Get G-Buffer values
	float3 fragPos = textureposition.Sample(samplerposition, inUV).rgb;
	float3 normal = textureNormal.Sample(samplerNormal, inUV).rgb;
	float4 albedo = textureAlbedo.Sample(samplerAlbedo, inUV);

	if (length(fragPos) < 1e-5)
	{
		return albedo;
	}
	float3 fragcolor =0;

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
		}
		return float4(fragcolor, 1.0);
	}

	// Ambient part
	float3 evir = albedo.rgb * AMBIENT_LIGHT;

	float3 color = 0;
	float3 N = normalize(normal);

	for (int i = 0; i < LIGHT_COUNT; ++i)
	{
		fragcolor =0;
		// Vector to light
		float3 L = ubo.lights[i].position.xyz - fragPos;
		// Distance from light to fragment position
		float dist = length(L);
		L = normalize(L);

		// Viewer to fragment
		float3 V = ubo.viewPos.xyz - fragPos;
		V = normalize(V);


		fragcolor = BRDF(L, V, N, 0.1f, 0.1f);
		if (ubo.useShadows > 0)
		{
			fragcolor = shadow1(fragcolor, fragPos,i);
		}
		color += fragcolor;
	}

	color += evir;
	// Shadow calculations in a separate pass
	color = pow(color, float3(0.4545, 0.4545, 0.4545));

	return float4(color, 1);
}