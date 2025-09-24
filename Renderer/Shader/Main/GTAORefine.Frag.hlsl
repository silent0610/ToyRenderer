Texture2D TexNormal : register(t2); // world-space normal
Texture2D TexDepth : register(t3); //  NDC depth
Texture2D TexNoise : register(t4); // Noise texture (e.g., 4x4 or 8x8 RGBA noise)
SamplerState SampNormal : register(s2); // Linear sampler, clamp UVs 应该是 nearest
SamplerState SampDepth : register(s3); // Point sampler, clamp UVs  应该是 nearest
SamplerState SampNoise : register(s4); // Noise sampler, repeat UVs 应该是 nearest

static const float PI = 3.1415926;

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
cbuffer CBCamera : register(b1) {
    CameraInfos CBCamera;
}
struct GTAOConstantsDesc {
    float Radius; // View-space radius for sampling
    float Scale;
    int NumDirections; // Number of directions to sample (e.g., 8)
    int NumSteps ; // Number of steps per direction (e.g., 6)
    float MaxRadiusPixels;
    float Strength;
};
cbuffer CBGTAO : register(b0) {
    GTAOConstantsDesc CBGTAO;
}

float3 ReconstructViewPos(float2 uv) {
    float depthNDC = TexDepth.Sample(SampDepth, uv).r;

    float4 posNDC = float4(uv * 2.0f - 1.0f, depthNDC, 1.0f);
    float4 posViewH = mul(CBCamera.InvProj, posNDC);
    return posViewH.xyz / posViewH.w;
}

float3 ReconstructViewPosWithNDCDepth(float2 uv, float depthNDC) {
    float4 posNDC = float4(uv * 2.0f - 1.0f, depthNDC, 1.0f);
    float4 posViewH = mul(CBCamera.InvProj, posNDC);
    return posViewH.xyz / posViewH.w;
}

float3 GetViewNormal(float2 uv) {
    float3 normalWorld = TexNormal.Sample(SampNormal, uv).xyz;
    return mul((float3x3) CBCamera.View, normalWorld);
}

float2 GetRandomDir(float2 uv) {
    float randomAngle = TexNoise.Sample(SampNoise, uv * CBGTAO.Scale).x * 2.0 * PI;
    return float2(cos(randomAngle), sin(randomAngle));
}

void ComputeSteps(inout float2 stepSizeUv, inout float numSteps, float rayRadiusPix, float rand) {
    // Avoid oversampling if numSteps is greater than the kernel radius in pixels
    numSteps = min(CBGTAO.NumSteps, rayRadiusPix);

    // Divide by Ns+1 so that the farthest samples are not fully attenuated
    // 防止最远采样点被完全衰减
    float stepSizePix = rayRadiusPix / (numSteps + 1);

    // Clamp numSteps if it is greater than the max kernel footprint
    float maxNumSteps = CBGTAO.MaxRadiusPixels / stepSizePix;
    if (maxNumSteps < numSteps) {
        // Use dithering to avoid AO discontinuities
        numSteps = floor(maxNumSteps + rand);
        numSteps = max(numSteps, 1);
        stepSizePix = CBGTAO.MaxRadiusPixels / numSteps;
    }

    // Step size in uv space
    stepSizeUv = stepSizePix * CBCamera.InvScreenSize;
}
float2 RotateDirections(float2 Dir, float2 CosSin) {
    return float2(Dir.x*CosSin.x - Dir.y*CosSin.y,
        Dir.x*CosSin.y + Dir.y*CosSin.x);
}

float IntegrateArc_UniformWeight(float2 h) {
    float2 Arc = 1 - cos(h);
    return Arc.x + Arc.y;
}

float IntegrateArc_CosWeight(float2 h, float n) {
    float2 Arc = -cos(2 * h - n) + cos(n) + 2 * h * sin(n);
    return 0.25 * (Arc.x + Arc.y);
}
///当前采样原点离相机越远,thickness越小
float ComputeThickness(float z, float znear, float zfar, float minThickness) {
    float fade = saturate((z - znear) / (zfar - znear));
    return lerp(1.0, minThickness, fade);
}

float2 SnapUVOffset(float2 uv) {
    return round(uv *CBCamera.ScreenSize) * CBCamera.InvScreenSize;
}

float4 main(float2 uv:TEXCOORD0):SV_TARGET0 {
    float depth = TexDepth.Sample(SampDepth,uv).r;
    if(depth>=1) {
        return float4(1,1,1,1);
    }
    float3 viewPos = ReconstructViewPosWithNDCDepth(uv,depth);
    float3 viewNormal = GetViewNormal(uv);
    float3 viewDir = normalize(-viewPos);

    float rayRadiusU = CBGTAO.Radius*CBCamera.Proj[0][0]/-viewPos.z;
    float rayRadiusPix = 0.5*rayRadiusU.x * CBCamera.ScreenSize.x; // 这里的计算需要注意
    float thickness = ComputeThickness(-viewPos.z,CBCamera.ZNear,CBCamera.ZFar,0.3);

    float3 random = TexNoise.Sample(SampNoise,uv*CBGTAO.Scale).xyz;
    // 噪音,先不管
    // half noiseOffset = frac(GTAO_Offsets(uv) + _SSAO_TemporalOffsets);
    // half noiseDirection = GTAO_Noise(uv * _SSAO_TexelSize.zw) + _SSAO_TemporalDirections;

    float occlusion=0.0f;
    if(rayRadiusPix> 1.0) {
        float numSteps;
        float2 stepSizeUV;

        ComputeSteps(stepSizeUV, numSteps, rayRadiusPix, random.x);  // 得到每一步的uv偏移和当前方向的总步数
        stepSizeUV.y = -stepSizeUV.y; ///!!! 因为vulkan的y是向下的 那HBAO里有没有这个问题。
        float alpha =  PI / CBGTAO.NumDirections;
        for(int i=0;i<CBGTAO.NumDirections;++i) {
            float theta = alpha * (i+random.z);
            // float3 sliceDir = float3(RotateDirections(float2(cos(theta), sin(theta)), random.xy),0); //采样方向
            float3 sliceDir = float3(float2(cos(theta), sin(theta)),0);
            //sliceDir = float3(0,1,0);
            float3 planeNormal = normalize(cross(sliceDir, viewDir));
            float3 planeTangent = cross(viewDir, planeNormal);
            float3 sliceNormal = viewNormal - planeNormal * dot(viewNormal, planeNormal);
            float sliceLength = length(sliceNormal); //  slice normal 代表的是将视空间法线 viewNormal 投影到当前切片
            //（由 viewDir 和 sliceDir 组成的平面）上后的长度，sliceLength越小,指投影法线越小,则视空间法线和当前切片越垂直,则从h1,h2中间投射过来的光线越不可能进入眼睛.,即贡献越小

            float cos_n = clamp(dot(normalize(sliceNormal), viewDir), -1, 1);
            // 认为n 在 view 左半为正
            float n = -sign(dot(sliceNormal, planeTangent)) * acos(cos_n);
            float2 h = float2(-1,-1);

            float2 deltaUV = sliceDir.xy * stepSizeUV; // 当前方向每一步的uv偏移

            for (int j = 1; j <= CBGTAO.NumSteps; ++j) {
                //计算uv offset, 第j次采样的uv偏移
                float2 uvOffset = deltaUV*(j+random.x); // 使步长偏移0-1
                // 两个方向采样点的uv
                float4 uvSlice = uv.xyxy + float4(uvOffset.xy, -uvOffset.xy);
                uvSlice.xy = SnapUVOffset(uvSlice.xy);
                uvSlice.zw = SnapUVOffset(uvSlice.zw);
                // 两个采样点方向向量 viewspcae
                // h1在负半边,h2在正半边,见 // n = -sign(dot(sliceNormal, planeTangent)) * acos(cos_n);
                float3 h1 =  ReconstructViewPos(uvSlice.xy) - viewPos; // h1和slice dir 方向相同，
                float3 h2 =  ReconstructViewPos(uvSlice.zw) - viewPos;

                // h1,h2向量长度平方
                float2 h1h2 = float2(dot(h1, h1), dot(h2, h2));
                // 向量长度的倒数
                float2 h1h2Length = rsqrt(h1h2);

                // 没看懂这个衰减函数
                float2 falloff = saturate(h1h2 * (2 / CBGTAO.Radius*CBGTAO.Radius)); // 越远,系数越大
                // H为h1,H2的余弦,余弦越大,夹角越小. 取最小角,即最大余弦 这里得到的夹角不区分方向,但有正负
                float2 H = float2(dot(h1, viewDir), dot(h2, viewDir)) * h1h2Length;
                //if(H.y>h.y) return float4(H.y,uvOffset,0);
                // h.xy = (H.xy > h.xy) ? lerp(H, h, falloff) : lerp(H.xy, h.xy, thickness);
                h.x = (H.x > h.x) ? lerp(H.x, h.x, falloff.x) : lerp(H.x, h.x, thickness); // 若H.x>h.x,H离中心越远,系数越大,越倾向于不变,维持h.x,缓解远距离的闪烁
                h.y = (H.y > h.y) ? lerp(H.y, h.y, falloff.y) : lerp(H.y, h.y, thickness); // 若H.y<h.y,当前采样原点离相机越远,thickness越小,越倾向H.y(即认为H.y是正确的,认为之前的h是异常变大),越倾向小?1-thickness越大,越倾向h.y
                // h.x = max(H.x,h.x);
                // h.y = max(H.y,h.y);
            }

            float2 hh1 = acos(clamp(h, -1, 1)); //得到0-PI
            float2 hh2 =float2(0,0);
            hh2.x = n + max(-hh1.x - n, -PI/2);
            hh2.y = n + min(hh1.y - n, PI/2);//
            occlusion +=  sliceLength*IntegrateArc_CosWeight(hh2,n);  // n是
            //return float4(occlusion,hh2*180/PI,n*180/PI);
            //occlusion += sliceLength * IntegrateArc_UniformWeight(h);
        }
        occlusion = saturate(pow(occlusion / float(CBGTAO.NumDirections),CBGTAO.Strength));
    }

    return float4(occlusion,viewPos.z,viewPos.z,1);
}