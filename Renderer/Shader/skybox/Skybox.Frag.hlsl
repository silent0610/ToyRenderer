Texture2D textureLighting : register(t1);
SamplerState samplerLighting : register(s1);
TextureCube textureEnv : register(t2);
SamplerState samplerEnv : register(s2);

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
cbuffer CBCamera : register(b0) {
    CameraInfos CBCamera;
}
struct FSOutput {
    float4 lighting : SV_TARGET0;
    float4 highLight : SV_TARGET1;
};
struct VSOutput {
    float4 pos : SV_POSITION;
    [[vk::location(0)]] float3 inUVW : POSITION0;
};

[[vk::constant_id(0)]] uint USE_SKYBOX=0;
FSOutput main(VSOutput input) {
    FSOutput output = (FSOutput)0;

    float2 uv = input.pos.rg;
    uv*=CBCamera.InvScreenSize;

    float4 lighting = textureLighting.Sample(samplerLighting, uv);

    output.lighting = lighting;
    if(USE_SKYBOX==1) {
        float3 cube = textureEnv.Sample(samplerEnv, input.inUVW).rgb;
        if (length(lighting.rgb) < 1e-8) {
            output.lighting = float4(cube, 1);
        }
    }

    float luminance = dot(output.lighting.rgb, float3(0.2126, 0.7152, 0.0722));
    if (luminance > 0.3f) {
        output.highLight = output.lighting;
    }

    return output;
}
