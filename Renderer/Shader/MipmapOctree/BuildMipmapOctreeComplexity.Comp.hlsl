// GPU Mipmap-based Octree Construction Compute Shader
// Generates next mip level by examining 8 child nodes

// Node states
#define NODE_EMPTY 0
#define NODE_MIXED 2
#define NODE_SOLID 1
#define MAX_COMPLEXITY 255
// Use standard HLSL register binding (consistent with other shaders)
struct LevelInfo {
    uint current_level;        // Current mip level being built
    uint input_size;          // Input texture size
    uint output_size;         // Output texture size
    uint base_size;           // Base texture size (64)
};
ConstantBuffer<LevelInfo> cb : register(b0);

Texture3D<uint2> InputLevel : register(t1);
RWTexture3D<uint2> OutputLevel : register(u2); 
SamplerState InputSampler : register(s3);

static const float Hmax = log2(3.0f);
// Decode child offset from index (0-7)
uint3 DecodeChildOffset(uint childIndex) {
    return uint3(
        (childIndex & 1) ? 1 : 0,    // x offset
        (childIndex & 2) ? 1 : 0,    // y offset
        (childIndex & 4) ? 1 : 0);   // z offset);
}

uint CalculateValue(uint countA, uint countB, uint countC)
{
    float pA = float(countA) /8.0f;
    float pB = float(countB) /8.0f;
    float pC = float(countC) /8.0f;

    float mean = 1/3;
    float variance = (pow(pA-mean,2.0f)+pow(pB-mean,2.0f)+pow(pC-mean,2.0f))/3.0f;
    variance = variance *9.0f /2.0f;
    return (1.0f - variance)*MAX_COMPLEXITY;
}

float CalculateGeometricComplexity(uint solidCount, uint emptyCount)
{
    float Ns = float(solidCount);
    float Ne = float(emptyCount);
    float denom = (Ns + Ne) * (Ns + Ne) + 1e-5;
    float ratio = (4.0 * Ns * Ne) / denom;   // [0,1]，混合越均衡越高
    return ratio;
}

[numthreads(8, 8, 8)]
void main(uint3 id : SV_DispatchThreadID) {
    // Current position in output mip level
    uint3 outputPos = id.xyz;
    
    // Boundary check
    if (any(outputPos >= cb.output_size))
        return;
    
    // Count states of 8 child nodes
    uint solidCount = 0;
    uint emptyCount = 0;
    uint mixedCount = 0;
    
    uint parentComplexity =0;
    // Check all 8 child nodes
    for (uint i = 0; i < 8; i++) {
        // Calculate child position in input level
        uint3 childPos = outputPos * 2 + DecodeChildOffset(i);
        
        // Ensure child is within bounds
        if (any(childPos >= cb.input_size)) {
            emptyCount++;
            continue;
        }
        
        // Sample child state from voxel data (0=outside, 1=inside)
        uint2 voxelValue;
        if(cb.current_level==0)
        {
            voxelValue.x = InputLevel[childPos].x;
            voxelValue.y = 0;
        }
        else
        {
            voxelValue = InputLevel[childPos].xy;
        }
        parentComplexity += voxelValue.y;
        
        // For Level 0: input is binary voxel data, treat 1 as SOLID, 0 as EMPTY
        // For Level 1+: input is previous octree level with proper NODE_* values
        if (cb.current_level == 0) 
        {
            // First level: convert voxel binary data
            if (voxelValue.x == 1)
            {
                solidCount++;
            }
            else
            {
                emptyCount++;
            };  
        } 
        else 
        {
            // Subsequent levels: use octree node states
            if (voxelValue.x == NODE_SOLID)
                solidCount++;
            else if (voxelValue.x == NODE_EMPTY)
                emptyCount++;
            else if (voxelValue.x == NODE_MIXED)
                mixedCount++;
            else
                // Treat any unexpected values as EMPTY to prevent propagation
                emptyCount++;
        }
    }
    
    // Determine parent node state
    uint parentState = NODE_EMPTY;
    
    if (solidCount >= 6 ) {
        // All 8 children are solid -> parent is solid
        parentState = NODE_SOLID;
    }
    else if (emptyCount >= 6) {
        // All 8 children are empty -> parent is empty
        parentState = NODE_EMPTY;
    }
    else {
        // Mixed case: some children solid, some empty/mixed -> parent is mixed
        parentState = NODE_MIXED;
    }
    //uint complexity = CalculateValue(solidCount,emptyCount,mixedCount) + parentComplexity/8;

    float localComplex = CalculateValue(solidCount,emptyCount,mixedCount);
    float inherited = parentComplexity / 8.0;  // 子层平均复杂度
    float alpha = 0.75;                       // 局部占权重
    float finalComplex =  localComplex +inherited;
    
    uint complexity = uint(finalComplex);

    if(complexity>MAX_COMPLEXITY)
    {
        complexity = MAX_COMPLEXITY;
    }
    OutputLevel[outputPos] = uint2(parentState,complexity) ;
}