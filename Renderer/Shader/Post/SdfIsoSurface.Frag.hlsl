// SdfIsoSurface.Frag.hlsl
// 2D slice visualization of the multiview SDF volume with iso-contour overlay.

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

Texture3D sdfTexture : register(t1);
SamplerState sdfSampler : register(s1);

struct SdfIsoSurfacePush
{
    float4 surfaceColor;
    float4 contourColor;
    float4 volumeMin;
    float4 volumeMax;
    float4 sliceParams; // x: axis(0=x,1=y,2=z), y: layer, z: slice index, w: contour width
    float4 styleParams; // x: fill strength, y: background mix
    float4 gridParams;  // x: base SDF resolution
};

[[vk::push_constant]] SdfIsoSurfacePush pc;

float3 WorldToSdfUV(float3 worldPos)
{
    return (worldPos - pc.volumeMin.xyz) / max(pc.volumeMax.xyz - pc.volumeMin.xyz, float3(1e-6, 1e-6, 1e-6));
}

float SampleQuantizedSdf(float3 worldPos, uint layer)
{
    uint width = 1;
    uint height = 1;
    uint depth = 1;
    sdfTexture.GetDimensions(width, height, depth);

    float3 uv = WorldToSdfUV(worldPos);
    if (any(uv < 0.0.xxx) || any(uv > 1.0.xxx))
    {
        return 1e6;
    }

    float3 dims = max(float3(width, height, depth), float3(1.0, 1.0, 1.0));
    float step = max(1.0, exp2((float)layer));

    float3 voxel = uv * dims;
    voxel = floor(voxel / step) * step + step * 0.5;
    voxel = clamp(voxel, float3(0.5, 0.5, 0.5), dims - float3(0.5, 0.5, 0.5));

    float3 quantizedUv = voxel / dims;
    return sdfTexture.SampleLevel(sdfSampler, quantizedUv, 0).r;
}

struct VSOutput
{
    float4 Pos : SV_POSITION;
    [[vk::location(0)]] float3 WorldPos : TEXCOORD0;
    [[vk::location(1)]] float2 LocalUV : TEXCOORD1;
};

float4 main(VSOutput input) : SV_Target
{
    uint axis = (uint)clamp(round(pc.sliceParams.x), 0.0, 2.0);
    uint layer = (uint)max(round(pc.sliceParams.y), 0.0);
    uint sliceIndex = (uint)max(round(pc.sliceParams.z), 0.0);
    float contourWidth = max(pc.sliceParams.w, 1e-4);

    uint sliceCount = max(1u, (uint)pc.gridParams.x >> min(layer, 30u));
    sliceIndex = min(sliceIndex, sliceCount - 1u);

    float sdf = SampleQuantizedSdf(input.WorldPos, layer);

    float inside = sdf < 0.0 ? 1.0 : 0.0;
    float contour = 1.0 - smoothstep(0.0, contourWidth, abs(sdf));
    float fill = saturate(1.0 - abs(sdf) / (contourWidth * 8.0));

    float3 bgOutside = float3(0.06, 0.07, 0.09);
    float3 bgInside = float3(0.03, 0.09, 0.12);
    float3 background = lerp(bgOutside, bgInside, inside);
    float gradient = saturate(0.5 + sdf * 4.0);
    background = lerp(background, float3(0.12, 0.14, 0.18), gradient * 0.35);
    background = lerp(float3(0.02, 0.02, 0.03), background, saturate(pc.styleParams.y));

    float edge = min(min(input.LocalUV.x, 1.0 - input.LocalUV.x), min(input.LocalUV.y, 1.0 - input.LocalUV.y));
    float border = 1.0 - smoothstep(0.0, 0.015, edge);
    background = lerp(background, float3(0.18, 0.20, 0.26), border * 0.65);

    float3 color = lerp(background, pc.surfaceColor.rgb, fill * saturate(pc.styleParams.x));
    color = lerp(color, pc.contourColor.rgb, contour);

    return float4(color, 1.0);
}
