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
#define AMBIENT_LIGHT 0.1
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
float4 main([[vk::location(0)]] float2 inUV : TEXCOORD0) : SV_TARGET
{
	// Get G-Buffer values
	float3 fragPos = textureposition.Sample(samplerposition, inUV).rgb;
	float3 normal = textureNormal.Sample(samplerNormal, inUV).rgb;
	float4 albedo = textureAlbedo.Sample(samplerAlbedo, inUV);

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

		float lightCosInnerAngle = cos(radians(15.0));
		float lightCosOuterAngle = cos(radians(25.0));
		float lightRange = 100.0;

		// Direction vector from source to target
		float3 dir = normalize(ubo.lights[i].position.xyz - ubo.lights[i].target.xyz);

		// Dual cone spot light with smooth transition between inner and outer angle
		float cosDir = dot(L, dir);
		float spotEffect = smoothstep(lightCosOuterAngle, lightCosInnerAngle, cosDir);
		float heightAttenuation = smoothstep(lightRange, 0.0f, dist);

		// Diffuse lighting
		float NdotL = max(0.0, dot(N, L));
		float3 diff = NdotL.xxx;

		// Specular lighting
		float3 R = reflect(-L, N);
		float NdotR = max(0.0, dot(R, V));
		float3 spec = (pow(NdotR, 16.0) * albedo.a * 2.5).xxx;
		fragcolor = float3((diff + spec) * spotEffect * heightAttenuation) * ubo.lights[i].color.rgb * albedo.rgb;
		if (ubo.useShadows > 0)
		{
			fragcolor = shadow1(fragcolor, fragPos,i);
		}
		color += fragcolor;
	}

	color +=evir;
	// Shadow calculations in a separate pass


	return float4(color, 1);
}