RWStructuredBuffer<uint> GlobalHistogram : register(u7);

#define NUM_ELEMENTS (16 * 256)
groupshared uint g_scan_array[NUM_ELEMENTS];

[numthreads(1024, 1, 1)]
void main(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
    uint i = DTid.x;

    // Load elements per thread into shared memory based on thread count
    uint elements_per_thread = NUM_ELEMENTS / 1024;
    for (uint e = 0; e < elements_per_thread; ++e)
    {
        uint index = i * elements_per_thread + e;
        if (index < NUM_ELEMENTS)
        {
            g_scan_array[index] = GlobalHistogram[index];
        }
    }

    // Parallel Prefix Sum (Blelloch Scan) - Up-sweep phase
    for (uint d = 0; d < log2(NUM_ELEMENTS); ++d)
    {
        GroupMemoryBarrierWithGroupSync();
        uint powerOf2 = 1 << d;
        if (i >= powerOf2)
        {
            g_scan_array[i] += g_scan_array[i - powerOf2];
        }
    }

    // Clear the last element
    if (i == 0)
    {
        g_scan_array[NUM_ELEMENTS - 1] = 0;
    }

    // Down-sweep phase
    for (uint d = log2(NUM_ELEMENTS) - 1; d != 0xFFFFFFFF; --d)
    {
        GroupMemoryBarrierWithGroupSync();
        uint powerOf2 = 1 << d;
        if (i >= powerOf2)
        {
            uint temp = g_scan_array[i - powerOf2];
            g_scan_array[i - powerOf2] = g_scan_array[i];
            g_scan_array[i] += temp;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    // Write back to global memory  
    for (uint e = 0; e < elements_per_thread; ++e)
    {
        uint index = i * elements_per_thread + e;
        if (index < NUM_ELEMENTS)
        {
            GlobalHistogram[index] = g_scan_array[index];
        }
    }
}