Texture2D textureposition : register(t1);
SamplerState samplerposition : register(s1);
Texture2D textureNormal : register(t2);
SamplerState samplerNormal : register(s2);
Texture2D textureAlbedo : register(t3);
SamplerState samplerAlbedo : register(s3);

// Texture2DArray textureShadowMap : register(t5);
// SamplerState samplerShadowMap : register(s5);

TextureCube textureIrradiance : register(t6);
SamplerState samplerIrradiance : register(s6);
Texture2D textureBRDFLUT : register(t7);
SamplerState samplerBRDFLUT : register(s7);
TextureCube prefilteredMapTexture : register(t8);
SamplerState prefilteredMapSampler : register(s8);

Texture2D textureMRAO : register(t9);
SamplerState samplerMRAO : register(s9);

Texture2D TexAO : register(t13);
SamplerState SampAO : register(s13);

struct DirLight {
    float4 pos;
    float4 dir;
    float4 color;
    float intensity;
    uint castShadow;
    int index;
};
struct PointLight {
    float4 pos;
    float4 color;
    float radius;
    float intensity;
    uint castShadow;
    int index;
};
struct SpotLight {
    float4 pos;
    float4 target;
    float4 color;
    float range;
    float angle;
    float intensity;
    uint castShadow;
    int index;
};
StructuredBuffer<DirLight> DirLights : register(t15);
StructuredBuffer<PointLight> PointLights : register(t16);
StructuredBuffer<SpotLight> SpotLights : register(t17);
//StructuredBuffer<PointLight> lights : register(t12);

struct CascadeMatrices {
    float4x4 mvp[4];
};
cbuffer  CascadeMatricesBuffer:register(b18) {
    CascadeMatrices DirLightMatrices[4];
}
struct Tile {
    int lightIndices[10];
    int lightCount;
};
StructuredBuffer<Tile> tiles : register(t11);

struct Light {
    float4 position;
    float4 target;
    float4 color;
    float4x4 viewMatrix;
};

struct UBO {
    float4 SplitDepth;
    uint DirLightCount;
    uint ShadowDirCount;
    uint PointLightCount;
    uint ShadowPointCount;
    uint SpotLightCount;
    uint ShadowSpotCount;
    float metallicFactor;
    float roughnessFactor;
    uint useShadows;
    int displayDebugTarget;
};

cbuffer ubo : register(b4) {
    UBO ubo;
}

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
cbuffer CBCamera : register(b14) {
    CameraInfos CBCamera;
}

Texture2DArray DirShadowMaps[] : register(t19);
SamplerState ShadowSampler : register(s19);
TextureCube ShadowPoint[] : register(t20);
SamplerState ShadowPointSampler : register(s20);

[[vk::constant_id(0)]] uint USE_AO=0;
[[vk::constant_id(1)]] uint USE_IBL=0;
[[vk::constant_id(2)]] uint SHOW_DIR_LIGHT=0;
[[vk::constant_id(3)]] uint SHOW_DIR_LIGHT_SHADOW=0;
[[vk::constant_id(4)]] uint SHOW_POINT_LIGHT=0;
[[vk::constant_id(5)]] uint SHOW_POINT_LIGHT_SHADOW=0;
[[vk::constant_id(6)]] uint SHOW_SPOT_LIGHT=0;
[[vk::constant_id(7)]] uint USE_PCF=0;

// float textureProj(float4 P, float layer, float2 offset) {
//     float shadow = 1.0;
//     float4 shadowCoord = P / P.w;
//     shadowCoord.xy = shadowCoord.xy * 0.5 + 0.5;

//     if (shadowCoord.z > -1.0 && shadowCoord.z < 1.0) {
//         float dist = textureShadowMap.Sample(samplerShadowMap, float3(shadowCoord.xy + offset, layer)).r;
//         if (shadowCoord.w > 0.0 && dist < shadowCoord.z) {
//             shadow = SHADOW_FACTOR;
//         }
//     }
//     return shadow;
// }

// float3 shadow(float3 fragcolor, float3 fragPos) {
//     for (int i = 0; i < LIGHT_COUNT; ++i) {
//         float4 shadowClip = mul(ubo.lights[i].viewMatrix, float4(fragPos.xyz, 1.0));

//         float shadowFactor;
// #ifdef USE_PCF
//         shadowFactor = filterPCF(shadowClip, i);
// #else
//         shadowFactor = textureProj(shadowClip, i, float2(0.0, 0.0));
// #endif

//         fragcolor *= shadowFactor;
//     }
//     return fragcolor;
// }

static const float PI = 3.14159265359;
float3 materialcolor() {
    return float3(0.1, 0.1, 0.1);
}
// Normal Distribution function ------------------------------------
float D_GGX(float dotNH, float roughness) {
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
    return (alpha2) / (PI * denom * denom);
}

// Geometric Shadowing function --------------------------------------
float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float GL = dotNL / (dotNL * (1.0 - k) + k);
    float GV = dotNV / (dotNV * (1.0 - k) + k);
    return GL * GV;
}

// Fresnel function ----------------------------------------------------
float3 F_Schlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
float3 F_SchlickR(float cosTheta, float3 F0, float roughness) {
    return F0 + (max((1.0 - roughness).xxx, F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

// Specular BRDF composition --------------------------------------------
float3 BRDF(float3 L, float3 V, float3 N, float metallic, float roughness) {
    // Precalculate vectors and dot products
    float3 H = normalize(V + L);
    float dotNV = clamp(dot(N, V), 0.0, 1.0);
    float dotNL = clamp(dot(N, L), 0.0, 1.0);
    float dotLH = clamp(dot(L, H), 0.0, 1.0);
    float dotNH = clamp(dot(N, H), 0.0, 1.0);

    // Light color fixed
    float3 lightColor = float3(1.0, 1.0, 1.0);

    float3 color = float3(0.0, 0.0, 0.0);

    if (dotNL > 0.0) {
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
float3 prefilteredReflection(float3 R, float roughness) {
    const float MAX_REFLECTION_LOD = 9.0; // todo: param/const
    float lod = roughness * MAX_REFLECTION_LOD;
    float lodf = floor(lod);
    float lodc = ceil(lod);
    float3 a = prefilteredMapTexture.SampleLevel(prefilteredMapSampler, R, lodf).rgb;
    float3 b = prefilteredMapTexture.SampleLevel(prefilteredMapSampler, R, lodc).rgb;
    return lerp(a, b, lod - lodf);
}
float3 specularContribution(float3 L, float3 V, float3 N, float3 F0, float3 albedo, float metallic, float roughness,float3 lightColor,float3 intensity) {
    // Precalculate vectors and dot products
    float3 H = normalize(V + L);
    float dotNH = clamp(dot(N, H), 0.0, 1.0);
    float dotNV = clamp(dot(N, V), 0.0, 1.0);
    float dotNL = clamp(dot(N, L), 0.0, 1.0);

    // Light color fixed

    float3 color = float3(0.0, 0.0, 0.0);

    if (dotNL > 0.0) {
        // D = Normal distribution (Distribution of the microfacets)
        float D = D_GGX(dotNH, roughness);
        // G = Geometric shadowing term (Microfacets shadowing)
        float G = G_SchlicksmithGGX(dotNL, dotNV, roughness);
        // F = Fresnel factor (Reflectance depending on angle of incidence)
        float3 F = F_Schlick(dotNV, F0);
        float3 spec = D * F * G / (4.0 * dotNL * dotNV + 0.001);
        float3 kD = (float3(1.0, 1.0, 1.0) - F) * (1.0 - metallic);
        color += (kD * albedo / PI + spec) * dotNL*lightColor*intensity;
    }

    return color;
}

float3 AOMultiBounce(float3 BaseColor, float AO) {
    float3 a =  2.0404 * BaseColor - 0.3324;
    float3 b = -4.7951 * BaseColor + 0.6417;
    float3 c =  2.7552 * BaseColor + 0.6903;
    return max(AO, ((AO * a + b) * AO + c) * AO);
}
float textureProj(float4 shadowCoord, float2 offset, uint lightIndex, uint cascadeIndex) {
    float shadow = 1.0;
    float bias = 0.001;
    shadowCoord.xy = 0.5*(shadowCoord.xy+1);
    if (shadowCoord.z > 0 && shadowCoord.z < 1.0) {
        float dist = DirShadowMaps[lightIndex].Sample(ShadowSampler, float3(shadowCoord.xy + offset, cascadeIndex)).r;
        bias = 0.001 + 0.001 * (1.0 - shadowCoord.z) * (1.0 - shadowCoord.z);
        if (shadowCoord.w > 0 && dist < shadowCoord.z - bias) {
            shadow = 0;
        }
    }
    return shadow;
}

float filterPCF(float4 sc, uint lightIndex, uint cascadeIndex) {
    int3 texDim;
    DirShadowMaps[lightIndex].GetDimensions(texDim.x, texDim.y, texDim.z);
    float scale = 0.75;
    float dx = scale * 1.0 / float(texDim.x);
    float dy = scale * 1.0 / float(texDim.y);

    float shadowFactor = 0.0;
    int count = 0;
    int range = 1;

    for (int x = -range; x <= range; x++) {
        for (int y = -range; y <= range; y++) {
            shadowFactor += textureProj(sc, float2(dx*x, dy*y),lightIndex, cascadeIndex);
            count++;
        }
    }
    return shadowFactor / count;
}

float SampleDirShadow(float2 uv,uint lightIndex) {
    float3 worldPos = textureposition.Sample(samplerposition, uv).rgb;
    float4 viewPos = mul(CBCamera.View,float4(worldPos,1.0f));
    uint cascadeIndex = 0;
    for(uint i = 0; i < 4 - 1; ++i) {
        if(viewPos.z < ubo.SplitDepth[i]) {
            cascadeIndex = i + 1;
        }
    }
    float4 shadowCoord = mul(DirLightMatrices[lightIndex].mvp[cascadeIndex],float4(worldPos,1.0f));
    shadowCoord/=shadowCoord.w;
    // float2 shadowUV = 0.5*(shadowCoord.xy+1.0f);
    float shadow =0;

    if (USE_PCF == 1) {
        shadow = filterPCF(shadowCoord, lightIndex,cascadeIndex);
    } else {
        shadow = shadow = textureProj(shadowCoord, float2(0.0, 0.0),lightIndex, cascadeIndex);
    }
    return shadow;
}

float SamplePointShadow(float2 uv,uint lightIndex) {
    float3 worldPos = textureposition.Sample(samplerposition, uv).rgb;
    float3 lightVec = worldPos - PointLights[lightIndex].pos.xyz;
    float sampledDist = ShadowPoint[lightIndex].Sample(ShadowPointSampler, lightVec).r;
    float dist = length(lightVec);
    float shadow = (dist <= sampledDist + 0.15) ? 1.0 : 0;
    return shadow;
}
struct FSOutput {
    float4 lighting : SV_TARGET0;
};

FSOutput main([[vk::location(0)]] float2 uv : TEXCOORD0) {
    FSOutput output = (FSOutput)0;

    float3 fragPos = textureposition.Sample(samplerposition, uv).rgb;
    // float2 screenPos = uv * CBCamera.ScreenSize;
    // int2 pixelCoord = int2(screenPos+0.5);
    // int2 tileCoord = pixelCoord / 16;
    // int tileIndex = tileCoord.y * 80 + tileCoord.x;
    // int curTileLightCount = tiles[tileIndex].lightCount;

    // output.lighting+=float4(float(curTileLightCount)/10,0,0,0);

    if (length(fragPos) < 1e-5) {
        return output;
    }

    float ao = 1;
    if(USE_AO!=0) {
        ao = TexAO.Sample(SampAO,uv).r;
    }
    float4 albedo = textureAlbedo.Sample(samplerAlbedo, uv);
    float3 normal = textureNormal.Sample(samplerNormal, uv).rgb;

    float4 MRAO = textureMRAO.Sample(samplerMRAO, uv);
    float metallic = ubo.metallicFactor * MRAO.r;
    float roughness = ubo.roughnessFactor * MRAO.g;

    float3 fragcolor = 0;

    // Debug display
    if (ubo.displayDebugTarget > 0) {
        switch (ubo.displayDebugTarget) {
            case 1:
            // fragcolor.rgb = shadow(float3(1.0, 1.0, 1.0), fragPos);
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
            case 7:
            fragcolor.rgb = ao.rrr;
            break;
        }
        output.lighting = float4(fragcolor, 1.0f);
        return output;
    }

    float3 N = normalize(normal);
    float3 V = normalize(CBCamera.CameraPos.xyz - fragPos);
    float3 R = reflect(-V, N);

    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo.rgb, metallic);
    float3 Lo = float3(0.0, 0.0, 0.0);
    float shadow=1.0f;
    if(SHOW_DIR_LIGHT==1) {
        for (int i = 0; i < ubo.DirLightCount; ++i) {
            float3 L = normalize(-DirLights[i].dir).xyz;
            shadow =1.0f;
            if(SHOW_DIR_LIGHT_SHADOW==1) {
                if(DirLights[i].castShadow==1) {
                    int index = DirLights[i].index;
                    shadow = SampleDirShadow(uv,index);
                }
            }
            Lo += shadow*specularContribution(L, V, N, F0, albedo.rgb, metallic, roughness, DirLights[i].color.xyz,DirLights[i].intensity);
        };
    }

    float2 screenPos = uv * CBCamera.ScreenSize;
    int2 pixelCoord = int2(screenPos+0.5);
    int2 tileCoord = pixelCoord / 16;
    int tileIndex = tileCoord.y * 80 + tileCoord.x;
    int curTileLightCount = tiles[tileIndex].lightCount;
    if(SHOW_POINT_LIGHT) {
        for(int j=0;j<curTileLightCount;++j) {
            int lightIndex = tiles[tileIndex].lightIndices[j];
            float3 lightVec = PointLights[lightIndex].pos.xyz-fragPos;
            float length = dot(lightVec,lightVec);
            float fade = 0;
            if(length> PointLights[lightIndex].radius*PointLights[lightIndex].radius) {
                continue;
            }

            fade = length/(PointLights[lightIndex].radius*PointLights[lightIndex].radius);
            fade = fade*fade;
            fade = 1.0 - fade;
            fade = fade*fade;

            shadow =1.0f;
            if(SHOW_DIR_LIGHT_SHADOW) {
                if(PointLights[lightIndex].castShadow==1) {
                    int index = PointLights[lightIndex].index;
                    shadow = SamplePointShadow(uv,index);
                }
            }
            float3 L = normalize(lightVec).xyz;
            Lo += fade*shadow*specularContribution(L, V, N, F0, albedo.rgb, metallic, roughness, PointLights[lightIndex].color.xyz,PointLights[lightIndex].intensity);
        }
    }

    if(SHOW_SPOT_LIGHT) {
        for (int k = 0; k < ubo.SpotLightCount; ++k) {
            float3 L = normalize(SpotLights[k].pos.xyz-fragPos).xyz;
            Lo += specularContribution(L, V, N, F0, albedo.rgb, metallic, roughness,SpotLights[k].color.xyz,SpotLights[k].intensity);
        }
    }

    float3 ambient;

    if(USE_IBL) {
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
        ambient = ao*(kD * diffuse + specular);
    }
    else {
        ambient = ao * albedo.rgb * 0.1;
    }

    float3 color = ambient + Lo;

    output.lighting += float4(color, 1);

    return output;
}