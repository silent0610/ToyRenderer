TextureCube TexCube : register(t0);
SamplerState SampCube : register(s0);

RWTexture3D<float> Result;

const int resolution = 64; // Resolution of the cubemap
const float3 CameraPos = float3(0.0, 0.0, 0.0); // Camera position in world space
const int size = 10;

[numthreads(4, 4, 4)]
void main(uint3 id : SV_DispatchThreadID) {
    float3 uvw = id / (float3) (resolution - 1);
    float3 worldPos = (uvw - 0.5) * size;
    float3 dir = normalize(worldPos - CameraPos);
    float depth = TexCube.SampleLevel(SampCube, dir, 0).r;

    //float depth = SampleCubemap(dir);
    float dist = distance(worldPos, CameraPos);
    float sdf = dist - depth;

    Result[id] = sdf;
}
