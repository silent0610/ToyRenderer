Texture2D TexAO:register(t1);
SamplerState SampAO:register(s1);

struct CBBlurDesc {
    float2 Res;
    float2 InvRes;
    float KernelRadius;
    float ZFactor;
};

cbuffer CBBlur:register(b0) {
    CBBlurDesc CBBlur;
};
float2 SampleAOZ(float2 offset, float2 baseUV) {
    return TexAO.Sample(SampAO, baseUV + offset * CBBlur.InvRes).rg;
}

float CrossBilateralWeight(float r, float z, float z0) {
    float BlurSigma = (CBBlur.KernelRadius + 1.0f) * 0.5f;
    float BlurFalloff = 1.0f / (2.0f * BlurSigma * BlurSigma);
    float dz = z0 - z;
    return exp2(-r * r * BlurFalloff - dz * dz);
}
float CrossBilateralWeight(float falloff,float r, float z, float z0) {
    float dz = CBBlur.ZFactor*(z0 - z);
    return exp2(-r * r * falloff - dz * dz);
}
float2 main(float2 uv:TEXCOORD0):SV_Target0 {
    float blurSigma = (CBBlur.KernelRadius + 1.0f) * 0.5f;
    float blurFalloff = 1.0f / (2.0f * blurSigma * blurSigma);

    float2 aoz = SampleAOZ(float2(0, 0), uv);
    float center_z = aoz.y;

    float total_ao = aoz.x;
    float total_weight = 1.0;

    float w= 1.0f;
    float i = 1.0;

    for (; i <= CBBlur.KernelRadius / 2.0; i += 1.0) {
        aoz = SampleAOZ(float2(i, 0), uv);
        w = CrossBilateralWeight(blurFalloff,i, aoz.y, center_z);
        total_ao += aoz.x * w;
        total_weight += w;

        aoz = SampleAOZ(float2(-i, 0), uv);
        w = CrossBilateralWeight(blurFalloff,i, aoz.y, center_z);
        total_ao += aoz.x * w;
        total_weight += w;
    }

    for (; i <= CBBlur.KernelRadius; i += 2.0) {
        aoz = SampleAOZ(float2(0.5 + i, 0), uv);
        w = CrossBilateralWeight(blurFalloff,i, aoz.y, center_z);
        total_ao += aoz.x * w;
        total_weight += w;

        aoz = SampleAOZ(float2(-0.5 - i, 0), uv);
        w = CrossBilateralWeight(blurFalloff,i, aoz.y, center_z);
        total_ao += aoz.x * w;
        total_weight += w;
    }

    float ao = total_ao / total_weight;
    return float2(ao,center_z);
}