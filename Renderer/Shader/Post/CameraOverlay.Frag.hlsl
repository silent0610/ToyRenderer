struct CameraInfos
{
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

cbuffer CBCamera : register(b0)
{
    CameraInfos CBCamera;
}

Texture2D sceneColor : register(t1);
SamplerState sceneSampler : register(s1);

StructuredBuffer<float4> cameraMatrices : register(t2);
StructuredBuffer<uint> activeCameraCount : register(t3);

struct CameraOverlayPush
{
    float4 markerColor;
    float4 highlightColor;
    float4 params;
};

[[vk::push_constant]] CameraOverlayPush pushConsts;

float RectSDF(float2 p, float2 b)
{
    float2 d = abs(p) - b;
    return length(max(d, float2(0.0f, 0.0f))) + min(max(d.x, d.y), 0.0f);
}

float CircleMask(float2 p, float radius, float softness)
{
    float dist = length(p) - radius;
    return saturate(1.0 - smoothstep(0.0, softness, dist));
}

float4 main(float2 uv : TEXCOORD0) : SV_Target
{
    float3 color = sceneColor.Sample(sceneSampler, uv).rgb;
    uint cameraCount = activeCameraCount[0];
    float2 screenSize = max(CBCamera.ScreenSize, float2(1.0f, 1.0f));
    float2 fragPx = uv * screenSize;

    const float3 lightDir = normalize(float3(-0.45f, 0.55f, 0.72f));
    const float3 viewDir = float3(0.0f, 0.0f, 1.0f);
    const float3 halfDir = normalize(lightDir + viewDir);

    [loop]
    for (uint i = 0; i < cameraCount; ++i)
    {
        float3 worldPos = cameraMatrices[i].xyz;
        float4 clip = mul(CBCamera.ProjView, float4(worldPos, 1.0f));
        if (clip.w <= 0.0001f)
        {
            continue;
        }

        float2 centerNdc = clip.xy / clip.w;
        float2 centerUv = centerNdc * 0.5f + 0.5f;
        if (any(centerUv < float2(0.0f, 0.0f)) || any(centerUv > float2(1.0f, 1.0f)))
        {
            continue;
        }

        float2 centerPx = centerUv * screenSize;
        float2 localPx = fragPx - centerPx;

        float markerRadius = max(6.0f, pushConsts.params.x / max(clip.w, 0.25f));
        float invRadius = 1.0f / markerRadius;
        float2 local = localPx * invRadius;

        float3 markerBase = pushConsts.markerColor.rgb;
        float3 markerHighlight = pushConsts.highlightColor.rgb;
        float shapeMode = pushConsts.params.w;

        if (shapeMode < 0.5f)
        {
            float r2 = dot(local, local);
            if (r2 >= 1.0f)
            {
                continue;
            }

            float z = sqrt(max(1.0f - r2, 0.0f));
            float3 normal = normalize(float3(local.x, -local.y, z));
            float diff = saturate(dot(normal, lightDir));
            float spec = pow(saturate(dot(normal, halfDir)), pushConsts.params.z);

            float edge = saturate(1.0f - smoothstep(0.78f, 1.0f, r2));
            float rim = saturate((1.0f - z) * 0.9f);

            float3 sphereColor = markerBase * (0.25f + 0.85f * diff + 0.15f * rim);
            sphereColor += markerHighlight * spec * pushConsts.params.y;
            color = lerp(color, sphereColor, edge);
        }
        else
        {
            float body = 1.0f - smoothstep(0.0f, 0.06f, abs(RectSDF(local, float2(0.78f, 0.50f))));
            float lens = CircleMask(local - float2(0.18f, 0.0f), 0.26f, 0.10f);
            float viewfinder = 1.0f - smoothstep(0.0f, 0.06f, abs(RectSDF(local - float2(-0.62f, 0.0f), float2(0.18f, 0.26f))));
            float icon = saturate(max(body, max(lens, viewfinder)));
            if (icon <= 0.0f)
            {
                continue;
            }

            float iconDiffuse = 0.4f + 0.6f * saturate(dot(float3(local.x, -local.y, 1.0f), lightDir));
            float iconSpec = pow(saturate(dot(normalize(float3(local.x, -local.y, 1.0f)), halfDir)), max(pushConsts.params.z * 0.75f, 1.0f));
            float3 iconColor = markerBase * iconDiffuse + markerHighlight * iconSpec;
            color = lerp(color, iconColor, icon);
        }
    }

    return float4(color, 1.0f);
}
