// SDFAO.Frag.hlsl - SDF环境光遮蔽片段着色器
// 利用SDF距离场信息进行更精确的环境光遮蔽计算

// SDFAO参数
struct SdfAODesc {
    uint sampleCount;      // 采样数量 (8-16)
    float sampleRadius;    // 采样半径 (世界空间)
    float aoStrength;      // AO强度系数
    float biasDistance;    // 偏移距离避免自遮蔽
    float maxDistance;     // 最大检测距离
    float falloffPower;    // 衰减幂次
    float voxelSize; // 每个体素的世界空间大小
    uint textureSize; // SDF纹理分辨率
    float4 minBounds;
    float4 maxBounds;
    float2 noiseScale; // 噪声缩放
};

cbuffer CBuffer : register(b0) {
    SdfAODesc sdfao;
}

// G-Buffer输入纹理
Texture2D texPos : register(t1);
SamplerState sampPos : register(s1);
Texture2D texNormal : register(t2);
SamplerState sampNormal : register(s2);
Texture2D texDepth : register(t3);
SamplerState sampDepth : register(s3);

// SDF距离场纹理 (3D texture)
Texture3D sdfTexture : register(t4);
SamplerState sdfSampler : register(s4);

// 噪声纹理用于随机化
Texture2D texNoise : register(t5);
SamplerState sampNoise : register(s5);

// 相机信息
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

// 从深度重建世界坐标
float3 GetWorldPos(float2 uv, float depth) {
    float3 ndc;
    ndc.xy = 2.0 * uv.xy - 1.0;
    ndc.z = depth;

    float4 viewPosH = mul(CBCamera.InvProj, float4(ndc, 1.0));
    float3 viewPos = viewPosH.xyz / viewPosH.w;
    float4 worldPosH = mul(CBCamera.InvView, float4(viewPos, 1.0));
    return worldPosH.xyz;
}

// 世界坐标转SDF纹理坐标 [0,1]
float3 WorldToSdfUV(float3 worldPos) {
    return (worldPos - sdfao.minBounds.xyz) / (sdfao.maxBounds - sdfao.minBounds).xyz;
}

// SDF距离场查询 (三线性插值)
float SampleSDF(float3 worldPos) {
    float3 sdfUV = WorldToSdfUV(worldPos);

    // 边界检查
    if (any(sdfUV < 0.0) || any(sdfUV > 1.0)) {
        return sdfao.maxDistance; // 超出边界返回最大距离
    }

    // 采样SDF距离场 (负值表示内部，正值表示外部)
    float sdfDistance = sdfTexture.SampleLevel(sdfSampler, sdfUV, 0).r;
    return abs(sdfDistance);
}

// cosine-weighted 半球采样（切线空间） -> 转换到世界空间（使用传入的 tangent/bitangent/normal）
// 优点：对 AO 更有效，减少噪点
float3 GetHemisphereSampleDirection_CosineWeighted(
    float3 normal, float3 worldPos, uint sampleIndex,
    float3 tangent, float3 bitangent) {
    // 1. 使用世界坐标和法线作为随机种子，消除相机视角依赖
    // 混合世界坐标的多个分量以打破规律性
    float seed = dot(worldPos, float3(12.9898, 78.233, 37.719)) +
                 dot(normal, float3(26.1359, 74.4875, 91.3248)) +
                 float(sampleIndex) * 43.7585;

    // 使用伪随机函数而不是纹理采样
    float2 xi;
    xi.x = frac(sin(seed) * 43758.5453);
    xi.y = frac(sin(seed + 1.61803) * 43758.5453);

    // 2. cosine-weighted sampling
    // r = sqrt(xi.x), phi = 2pi * xi.y
    float r = sqrt(max(0.0, xi.x));
    float phi = 2.0 * 3.14159265359 * xi.y;
    float x = r * cos(phi);
    float y = r * sin(phi);
    float z = sqrt(max(0.0, 1.0 - x*x - y*y)); // z >= 0, 朝向法线的半球

    // hemiDir in tangent-space where z is the normal axis
    float3 hemiDir = float3(x, y, z);

    // 3. transform to world space (inline combination: faster than mul with matrix)
    float3 worldDir = normalize(tangent * hemiDir.x + bitangent * hemiDir.y + normal * hemiDir.z);
    return worldDir;
}

// 计算SDF环境光遮蔽（优化版：TBN 在外面只构造一次；使用 cosine-weighted 半球采样）
float3 CalculateSdfAO(float3 worldPos, float3 normal, float2 screenUV) {
    float totalOcclusion = 0.0;

    // 构造切线空间基向量（一次）
    float3 up = abs(normal.y) < 0.999 ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = cross(normal, tangent);

    // 采样步数（分段采样以加速早停）
    const uint stepCount = 4;

    for (uint i = 0; i < sdfao.sampleCount; i++) {
        // 使用 cosine-weighted 半球采样（避免重复构造 TBN）
        float3 sampleDir = GetHemisphereSampleDirection_CosineWeighted(normal, worldPos, i, tangent, bitangent);

        // 逐步沿该方向采样 SDF
        float sampleOcclusion = 0.0;

        for (uint step = 1; step <= stepCount; step++) {
            float stepDistance = (float(step) / float(stepCount)) * sdfao.sampleRadius;
            float3 samplePos = worldPos + sampleDir * stepDistance + normal * sdfao.biasDistance;

            // 查询SDF距离
            float sdfDist = SampleSDF(samplePos);
            // return sdfDist.xxx;
            // 如果采样点在表面内部 (sdfDist < 0)，则产生遮蔽
            if (sdfDist < 0.02) {
                // 命中遮挡，记为1（也可按厚度调整）
                float occlusionStrength = 1.0;

                // 距离衰减（近距离更强）
                float distanceFalloff = 1.0 - pow(stepDistance / sdfao.sampleRadius, sdfao.falloffPower);

                sampleOcclusion += occlusionStrength * distanceFalloff;
                break; // 遇到遮挡物就停止该方向的采样
            } else {
                // 如果距离过远，减少影响并早停
                float farFalloff = saturate(sdfDist / sdfao.maxDistance);
                if (farFalloff > 0.8) break;
            }
        }

        totalOcclusion += sampleOcclusion;
    }

    // return float3(1.0f,1.0f,1.0f);
    // 归一化并应用强度
    float ao = totalOcclusion / max(1u, sdfao.sampleCount);
    ao = saturate(ao * sdfao.aoStrength);

    return (1.0 - ao).xxx; // 返回光照因子 (1=无遮蔽, 0=完全遮蔽)
}

// 主函数
float4 main(float4 pos : SV_Position, [[vk::location(0)]] float2 uv : TEXCOORD0) : SV_TARGET0 {
    // 获取G-Buffer信息
    float3 normal = normalize(texNormal.Sample(sampNormal, uv).xyz);
    float depth = texDepth.Sample(sampDepth, uv).r;

    // 天空盒处理
    if (depth >= 1.0) {
        return float4(1, 1, 1, 1); // 天空不产生AO
    }

    // 重建世界坐标
    float3 worldPos = GetWorldPos(uv, depth);

    // 计算SDFAO
    float3 aoFactor = CalculateSdfAO(worldPos, normal, uv);

    // 计算视空间深度 (修复：包含位移)
    float3 viewPosH = mul((float3x3)CBCamera.View, worldPos);
    float3 viewPos = viewPosH.xyz;

    return float4(aoFactor.x, viewPos.z,0.0f,0.0f);
}
