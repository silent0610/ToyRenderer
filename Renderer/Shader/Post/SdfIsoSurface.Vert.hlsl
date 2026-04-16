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

struct SdfIsoSurfacePush
{
    float4 surfaceColor;
    float4 contourColor;
    float4 volumeMin;
    float4 volumeMax;
    float4 sliceParams; // x: axis(0=x,1=y,2=z), y: layer, z: slice index, w: contour width
    float4 styleParams;  // x: fill strength, y: background mix
    float4 gridParams;   // x: base SDF resolution
};

[[vk::push_constant]] SdfIsoSurfacePush pc;

struct VSOutput
{
    float4 Pos : SV_POSITION;
    [[vk::location(0)]] float3 WorldPos : TEXCOORD0;
    [[vk::location(1)]] float2 LocalUV : TEXCOORD1;
};

float3 BuildSliceWorldPos(float2 uv, uint axis, float sliceT)
{
    float3 mn = pc.volumeMin.xyz;
    float3 mx = pc.volumeMax.xyz;
    float u = uv.x;
    float v = 1.0 - uv.y;

    if (axis == 0)
    {
        return float3(lerp(mn.x, mx.x, sliceT), lerp(mn.y, mx.y, v), lerp(mn.z, mx.z, u));
    }
    if (axis == 1)
    {
        return float3(lerp(mn.x, mx.x, u), lerp(mn.y, mx.y, sliceT), lerp(mn.z, mx.z, v));
    }
    return float3(lerp(mn.x, mx.x, u), lerp(mn.y, mx.y, v), lerp(mn.z, mx.z, sliceT));
}

VSOutput main(uint VertexIndex : SV_VertexID)
{
    VSOutput output = (VSOutput)0;

    uint axis = (uint)clamp(round(pc.sliceParams.x), 0.0, 2.0);
    uint layer = (uint)max(round(pc.sliceParams.y), 0.0);
    uint sliceCount = max(1u, (uint)pc.gridParams.x >> min(layer, 30u));

    uint sliceIndex = (uint)max(round(pc.sliceParams.z), 0.0);
    sliceIndex = min(sliceIndex, sliceCount - 1u);
    float sliceT = (float(sliceIndex) + 0.5f) / float(sliceCount);

    float2 uv = float2(0.0, 0.0);
    if (VertexIndex == 0)
    {
        uv = float2(0.0, 0.0);
    }
    else if (VertexIndex == 1)
    {
        uv = float2(1.0, 0.0);
    }
    else if (VertexIndex == 2)
    {
        uv = float2(1.0, 1.0);
    }
    else if (VertexIndex == 3)
    {
        uv = float2(0.0, 0.0);
    }
    else if (VertexIndex == 4)
    {
        uv = float2(1.0, 1.0);
    }
    else
    {
        uv = float2(0.0, 1.0);
    }

    float3 worldPos = BuildSliceWorldPos(uv, axis, sliceT);
    output.WorldPos = worldPos;
    output.LocalUV = uv;
    output.Pos = mul(CBCamera.ProjView, float4(worldPos, 1.0f));
    return output;
}
