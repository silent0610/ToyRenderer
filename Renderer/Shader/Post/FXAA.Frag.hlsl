// FXAA HLSL for Vulkan
Texture2D tex : register(t1);
SamplerState smp : register(s1);

struct UBO
{
    float2 rcpFrame;
    float2 sth;
};
cbuffer ubo : register(b0) { UBO ubo; }

#define FXAA_REDUCE_MIN (1.0 / 128.0)

#define FXAA_REDUCE_MUL (1.0 / 8.0)

#define FXAA_SPAN_MAX 8.0

float4 main(float2 uv : TEXCOORD) : SV_Target
{
    if (ubo.sth.g > 0.5f)
    {
        float3 rgbM = tex.Sample(smp, uv).rgb;
        float3 rgbNW = tex.Sample(smp, uv + float2(-ubo.rcpFrame.x, -ubo.rcpFrame.y)).rgb;
        float3 rgbNE = tex.Sample(smp, uv + float2(ubo.rcpFrame.x, -ubo.rcpFrame.y)).rgb;
        float3 rgbSW = tex.Sample(smp, uv + float2(-ubo.rcpFrame.x, ubo.rcpFrame.y)).rgb;
        float3 rgbSE = tex.Sample(smp, uv + float2(ubo.rcpFrame.x, ubo.rcpFrame.y)).rgb;

        float3 luma = float3(0.299, 0.587, 0.114);
        float lumaM = dot(rgbM, luma);
        float lumaNW = dot(rgbNW, luma);
        float lumaNE = dot(rgbNE, luma);
        float lumaSW = dot(rgbSW, luma);
        float lumaSE = dot(rgbSE, luma);

        float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
        float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

        float edgeThreshold = ubo.sth.x;

        // return float4((lumaMax - lumaMin).rrr, 1);
        if (lumaMax - lumaMin < edgeThreshold)
            return float4(rgbM, 1.0);

        float2 dir;
        dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
        dir.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));

        float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) *
                                  (0.25 * FXAA_REDUCE_MUL),
                              FXAA_REDUCE_MIN);

        float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
        dir = min(float2(FXAA_SPAN_MAX, FXAA_SPAN_MAX),
                  max(float2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
                      dir * rcpDirMin)) *
              ubo.rcpFrame;

        float3 rgbA = 0.5 * (tex.Sample(smp, uv + dir * (1.0 / 3.0 - 0.5)).rgb + tex.Sample(smp, uv + dir * (2.0 / 3.0 - 0.5)).rgb);

        float3 rgbB = rgbA * 0.5 + 0.25 * (tex.Sample(smp, uv + dir * -0.5).rgb + tex.Sample(smp, uv + dir * 0.5).rgb);

        float lumaB = dot(rgbB, luma);
        float4 color;
        if ((lumaB < lumaMin) || (lumaB > lumaMax))
            color = float4(rgbA, 1);
        else
            color = float4(rgbB, 1);
        return color;
    }
    else
    {
        return float4(tex.Sample(smp, uv).rgb, 1.0f);
    }
}