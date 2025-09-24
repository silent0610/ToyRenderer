Texture2D texPos : register(t1);
SamplerState sampPos : register(s1);
Texture2D texNormal : register(t2);
SamplerState sampNormal : register(s2);
Texture2D texDepth : register(t3);
SamplerState sampDepth : register(s3);

Texture2D texNoise : register(t4);
SamplerState sampNoise : register(s4);

StructuredBuffer<float4> kernels : register(t5);

struct CBufferDesc {
    uint sampleNum;
    float radius;
    float scale;
};

cbuffer CBuffer : register(b0) {
    CBufferDesc CBuffer;
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
cbuffer CBCamera : register(b6) {
    CameraInfos CBCamera;
}
float3 GetWorldPos(float2 uv) {
    float3 ndc ;
    ndc.xy = 2.0 * uv.xy -1.0;
    ndc.z = texDepth.Sample(sampDepth,uv).r;

    float4 viewPosH = mul(CBCamera.InvProj, float4(ndc, 1.0));
    float3 viewPos = viewPosH.xyz / viewPosH.w;
    float4 worldPosH = mul(CBCamera.InvView, float4(viewPos, 1.0));
    return worldPosH.xyz;
}
float3 GetWorldPos(float2 uv,float depth) {
    float3 ndc ;
    ndc.xy = 2.0 * uv.xy -1.0;
    ndc.z = depth;

    float4 viewPosH = mul(CBCamera.InvProj, float4(ndc, 1.0));
    float3 viewPos = viewPosH.xyz / viewPosH.w;
    float4 worldPosH = mul(CBCamera.InvView, float4(viewPos, 1.0));
    return worldPosH.xyz;
}
float3 GetRandDir(float2 uv) {
    float3 dir = float3(texNoise.Sample(sampNoise,uv*CBuffer.scale).xy * 2.0f - 1.0f,1.0f);
    //dir.y =1.0f;

    return dir;
}
float3 GetSamplePos(int i) {
    float3 pos = kernels[i].xyz;
    return pos;
}
float4 main(float4 pos:SV_Position, [[vk::location(0)]] float2 uv : TEXCOORD0):SV_TARGET0 {
    // float4 worldPos = texPos.Sample(sampPos,uv);

    float3 normal = texNormal.Sample(sampNormal,uv).xyz;
    float depth = texDepth.Sample(sampDepth,uv).r;
    if(depth>=1) {
        return float4(1,1,1,1);
    }
    float3 worldPos = GetWorldPos(uv,depth);
    float3 viewPos = mul((float3x3)CBCamera.View,worldPos);

    float3 randDir = GetRandDir(uv);
    float occlusion = 0.0f;

    float3 N = normalize(normal);

    float3 T, B;
    const float3 up_vec = float3(0.0f, 1.0f, 0.0f);
    const float3 alt_vec = float3(1.0f, 0.0f, 0.0f);
    //float3 reference_vec = (abs(dot(N, up_vec)) > 0.999f) ? alt_vec : up_vec;
    float3 reference_vec = randDir;
    T = normalize(cross(reference_vec, N)); // 计算初始切线
    B = normalize(cross(N, T));             // 计算副切线
    float3x3 tbn = transpose(float3x3(T, N, B)); // 这里是按行构造矩阵！！！！！！！！！！！但我需要按列构造！！！，再者，TBN指 法线存在z分量上，我这里是存在y分量上
    //return mul(float3(0.5,0.6,0),tbn);
    for (uint i = 0; i < CBuffer.sampleNum; ++i) {
        float3 kernelPos = mul(tbn, GetSamplePos(i));
        //return kernelPos;
        // Calculate sample world space pos
        float3 samplePosW = worldPos + (kernelPos * 1);
        float sampleDepth = length(samplePosW - CBCamera.CameraPos.xyz);

        float4 samplePosProj = mul(CBCamera.Proj, mul(CBCamera.View,float4(samplePosW, 1.0f)));
        samplePosProj /= samplePosProj.w;

        float2 sampleUV = saturate(float2(samplePosProj.x, samplePosProj.y) * 0.5f + 0.5f);

        float sceneDepth = length(GetWorldPos(sampleUV).xyz - CBCamera.CameraPos.xyz)+0.01; //nmmmd草擦操哦曹操凹槽从，妈的，恶心我四五个个小时，，原来是直接采uv，所以都是子阴影，畜生啊

        float rangeCheck = step(abs(sampleDepth - sceneDepth), 1); 
        occlusion += step(sceneDepth, sampleDepth) * rangeCheck;
        // if(occlusion>0.005)
        // {
        //     return float4(kernelPos,sampleDepth-sceneDepth);
        // }
    }
    float factor = 1 - (occlusion / float(CBuffer.sampleNum));

    return float4(factor,viewPos.z,0,0);
}