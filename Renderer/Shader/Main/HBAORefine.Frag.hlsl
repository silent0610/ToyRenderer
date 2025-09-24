Texture2D TexNormal : register(t2); // world-space normal
Texture2D TexDepth : register(t3); //  NDC depth
Texture2D TexNoise : register(t4); // Noise texture (e.g., 4x4 or 8x8 RGBA noise)
SamplerState SampNormal : register(s2); // Linear sampler, clamp UVs 应该是 nearest
SamplerState SampDepth : register(s3); // Point sampler, clamp UVs  应该是 nearest
SamplerState SampNoise : register(s4); // Noise sampler, repeat UVs 应该是 nearest
static const float PI = 3.1415926;
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
cbuffer CBCamera : register(b1)
{
    CameraInfos CBCamera;
}
struct HBAOConstantsDesc
{
    float Radius; // View-space radius for sampling
    float Raidus2;
    float NegInvR2;
    float Scale;
    int NumDirections; // Number of directions to sample (e.g., 8)
    int NumSteps;
    float MaxRadiusPixels;
    float TanBias;
    float Strength;
    float2 UVToViewA;
    float2 UVToViewB;
    float2 LinMAD; // Number of steps per direction (e.g., 6)
};
cbuffer CBHBAO : register(b0)
{
    HBAOConstantsDesc CBHBAO;
}
// 公式推导见笔记
float ViewSpaceZFromDepth(float d)
{
    // Get view space Z
    return -1.0 / (CBHBAO.LinMAD.x * d + CBHBAO.LinMAD.y);
}
float3 UVToViewSpace(float2 uv, float z)
{
    uv = CBHBAO.UVToViewA * uv + CBHBAO.UVToViewB;
    return float3(uv * z, z);
}

float3 GetViewPos(float2 uv)
{
    float Zview = ViewSpaceZFromDepth(TexDepth.Sample(SampDepth, uv).r);
    //float z = texture(texture0, uv).r;
    return UVToViewSpace(uv, Zview);
}

float3 ReconstructViewPos(float2 uv)
{
    float depthNDC = TexDepth.Sample(SampDepth, uv).r;

    float4 posNDC = float4(uv * 2.0f - 1.0f, depthNDC, 1.0f);
    float4 posViewH = mul(CBCamera.InvProj, posNDC);
    return posViewH.xyz / posViewH.w;
}

float3 ReconstructViewPosWithNDCDepth(float2 uv, float depthNDC)
{
    float4 posNDC = float4(uv * 2.0f - 1.0f, depthNDC, 1.0f);
    float4 posViewH = mul(CBCamera.InvProj, posNDC);
    return posViewH.xyz / posViewH.w;
}

float3 ReconstructWorldPos(float2 uv)
{
    float depthNDC = TexDepth.Sample(SampDepth, uv).r;

    float4 posNDC = float4(uv * 2.0f - 1.0f, depthNDC, 1.0f);
    float4 posViewH = mul(CBCamera.InvProj, posNDC);
    float3 posWorld = mul((float3x3) CBCamera.InvView, posViewH.xyz / posViewH.w);
    return posWorld;
}

float3 GetViewNormal(float2 uv)
{
    float3 normalWorld = TexNormal.Sample(SampNormal, uv).xyz;
    return mul((float3x3) CBCamera.View, normalWorld);
}

float2 GetRandomDir(float2 uv)
{
    float randomAngle = TexNoise.Sample(SampNoise, uv * CBHBAO.Scale).x * 2.0 * PI;
    return float2(cos(randomAngle), sin(randomAngle));
}

float GetProjectedRadius(float2 uv, float3 posView)
{
    float2 posScreen = uv * CBCamera.ScreenSize;

    float3 posOffset = posView + float3(CBHBAO.Radius, 0, 0);
    float4 posClip = mul(CBCamera.Proj, float4(posOffset, 1.0f));
    posClip.xy /= posClip.w;
    float2 posNDC = (posClip.xy) * 0.5f + 0.5;
    float2 offsetPosScreen = posNDC * CBCamera.ScreenSize;

    return length(offsetPosScreen - posScreen);
}
float Length2(float3 V)
{
    return dot(V, V);
}

float3 MinDiff(float3 P, float3 Pr, float3 Pl)
{
    float3 V1 = Pr - P;
    float3 V2 = P - Pl;
    return (Length2(V1) < Length2(V2)) ? V1 : V2;
}
float BiasedTangent(float3 V)
{
    return V.z * 1 / length(V.xy);
}
float TanToSin(float x)
{
    return x * 1 / sqrt(x * x + 1.0);
}
float Tangent(float3 P, float3 S)
{
    return (S.z - P.z) * 1 / length(S.xy - P.xy);
}

void ComputeSteps(inout float2 stepSizeUv, inout float numSteps, float rayRadiusPix, float rand)
{
    // Avoid oversampling if numSteps is greater than the kernel radius in pixels
    numSteps = min(CBHBAO.NumSteps, rayRadiusPix);

    // Divide by Ns+1 so that the farthest samples are not fully attenuated
    // 防止最远采样点被完全衰减
    float stepSizePix = rayRadiusPix / (numSteps + 1);

    // Clamp numSteps if it is greater than the max kernel footprint
    float maxNumSteps = CBHBAO.MaxRadiusPixels / stepSizePix;
    if (maxNumSteps < numSteps)
    {
        // Use dithering to avoid AO discontinuities
        numSteps = floor(maxNumSteps + rand);
        numSteps = max(numSteps, 1);
        stepSizePix = CBHBAO.MaxRadiusPixels / numSteps;
    }

    // Step size in uv space
    stepSizeUv = stepSizePix * CBCamera.InvScreenSize;
}
float2 RotateDirections(float2 Dir, float2 CosSin)
{
    return float2(Dir.x * CosSin.x - Dir.y * CosSin.y,
        Dir.x * CosSin.y + Dir.y * CosSin.x);
}
float2 SnapUVOffset(float2 uv)
{
    return round(uv * CBCamera.ScreenSize) * CBCamera.InvScreenSize;
}
float Falloff(float d2)
{
    return d2 * CBHBAO.NegInvR2 + 1.0f;
}
// 能不能对比一下原始实现
float HorizonOcclusion(float2 uv0,
    float2 deltaUV,
    float3 P,
    float3 dPdu,
    float3 dPdv,
    float randstep,
    float numSamples)
{
    float ao = 0;

    // Offset the first coord with some noise
    float2 uv = uv0 + SnapUVOffset(randstep * deltaUV);
    deltaUV = SnapUVOffset(deltaUV);

    // Calculate the tangent vector
    float3 T = deltaUV.x * dPdu + deltaUV.y * dPdv;

    // Get the angle of the tangent vector from the viewspace axis
    //float tanH = Tangent(float3(0,0,0),T);
    float tanH = BiasedTangent(T);
    float sinH = TanToSin(tanH); // 即sinT

    float tanS;
    float d2;
    float3 S;

    // Sample to find the maximum angle
    for (float s = 1; s <= numSamples; ++s)
    {
        uv += deltaUV;
        S = ReconstructViewPos(uv);
        tanS = Tangent(P, S);
        d2 = Length2(S - P);

        // Is the sample within the radius and the angle greater?
        if (d2 < CBHBAO.Raidus2 && tanS > tanH)
        {
            float sinS = TanToSin(tanS);
            // Apply falloff based on the distance
            ao += Falloff(d2) * (sinS - sinH); // 正确的。。。
            tanH = tanS;
            sinH = sinS;
        }
    }
    //ao = (sinH - sinT)*Fallof(maxd2);
    return ao;
}
// 相比于paper中的HBAO有几处不同
// 1. 为解决 阴影的不连续，使用的衰减函数是平方衰减
// 2. 积分是每个方向的每个采样点都计算
// 3. tangent 添加Angle Bias
// 4. 超出屏幕的采样点  clamping to edge
// 5. 滤波 这里没有，需要在外面找
float4 main(float2 uv : TEXCOORD0) : SV_TARGET0
{
    float3 P, Pr, Pl, Pt, Pb;

    // P = GetViewPos(uv);
    P = ReconstructViewPos(uv);
    Pr = ReconstructViewPos(uv + float2(CBCamera.InvScreenSize.x, 0));
    Pl = ReconstructViewPos(uv + float2(-CBCamera.InvScreenSize.x, 0));
    Pt = ReconstructViewPos(uv + float2(0, CBCamera.InvScreenSize.y));
    Pb = ReconstructViewPos(uv + float2(0, -CBCamera.InvScreenSize.y));

    // Calculate tangent basis vectors using the minimu difference
    float3 dPdu = MinDiff(P, Pr, Pl);
    float3 dPdv = MinDiff(P, Pt, Pb) * (CBCamera.ScreenSize.y * CBCamera.InvScreenSize.x);

    float2 random = TexNoise.Sample(SampNoise, uv * CBHBAO.Scale).xy;

    float rayRadiusU = CBHBAO.Radius * CBCamera.Proj[0][0] / -P.z;
    float rayRadiusPix = 0.5 * rayRadiusU.x * CBCamera.ScreenSize.x; // 这里的计算需要注意
    //float curRadius = GetProjectedRadius(uv, P); //(radius 对当前片元在屏幕空间的投影半径)

    float ao = 1.0f;
    if (rayRadiusPix > 1.0)
    {
        ao = 0.0;
        float numSteps;
        float2 stepSizeUV;

        // Compute the number of steps
        ComputeSteps(stepSizeUV, numSteps, rayRadiusPix, random.y);

        float alpha = 2.0 * PI / CBHBAO.NumDirections;
        for (float d = 0; d < CBHBAO.NumDirections; ++d)
        {
            float theta = alpha * d;
            float2 dir = RotateDirections(float2(cos(theta), sin(theta)), random.xy);
            //float theta = alpha*d + random.x; 
            // float2 dir =float2(cos(theta), sin(theta));
            float2 deltaUV = dir * stepSizeUV;
            
            ao += HorizonOcclusion(
                uv,
                deltaUV,
                P,
                dPdu,
                dPdv,
                random.y,
                numSteps);
        }

        ao = 1.0 - ao / CBHBAO.NumDirections * CBHBAO.Strength;
    }

    return float4(ao, P.z, 1, 0);
}