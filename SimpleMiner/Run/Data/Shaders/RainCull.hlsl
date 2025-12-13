#include "RainUtils.hlsli"

//-----------------------------------------------------------------------------------------------
// Resources
struct RainCullResources
{
    uint particleBufferIndex;      // StructuredBuffer<RainParticle>
    uint visibilityBufferIndex;    // RWStructuredBuffer<uint>
    uint depthTextureIndex;        // Texture2D<float>
    uint samplerIndex;             // SamplerState
    uint rainConstantsIndex;       // ConstantBuffer<RainCullConstants>
};

ConstantBuffer<RainCullResources> renderResources : register(b0);

//-----------------------------------------------------------------------------------------------
[numthreads(256, 1, 1)]
void ComputeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    StructuredBuffer<RainParticle> particleBuffer = ResourceDescriptorHeap[renderResources.particleBufferIndex];
    RWStructuredBuffer<uint> visibilityBuffer = ResourceDescriptorHeap[renderResources.visibilityBufferIndex];
    Texture2D<float> depthTexture = ResourceDescriptorHeap[renderResources.depthTextureIndex];
    SamplerState depthSampler = SamplerDescriptorHeap[renderResources.samplerIndex];
    ConstantBuffer<RainConstants> constants = ResourceDescriptorHeap[renderResources.rainConstantsIndex];

    uint particleIndex = dispatchThreadID.x;
    if (particleIndex >= constants.numParticles)
        return;

    // Read particle
    RainParticle particle = particleBuffer[particleIndex];

    // Calculate world position
    float3 worldPos = constants.boxCenter + particle.relativePosition;
    float4 worldSpacePosition = float4(worldPos, 1.0);
    float4 clipSpacePosition = mul(constants.worldToClipTransform, worldSpacePosition);

    // Perspective divide
    float3 ndc = clipSpacePosition.xyz / clipSpacePosition.w;

    // if out side of ndc, return visible is true
    if (abs(ndc.x) > 1.0 || abs(ndc.y) > 1.0 || ndc.z < 0.0 || ndc.z > 1.0)
    {
        visibilityBuffer[particleIndex] = 1; 
        return;
    }

    // Convert to UV space [0, 1]
    float2 uv = ndc.xy * 0.5 + 0.5;
    uv.y = 1.0 - uv.y;  // Flip Y axis

    // Default to visible
    uint visible = 1;

        // Check if particle is within depth map bounds
    if (uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0)
    {
        // Sample depth from depth map
        float sceneDepth = depthTexture.SampleLevel(depthSampler, uv, 0);

        // Get particle depth in NDC space
        float particleDepth = ndc.z;

        // If particle is below the scene geometry, mark as occluded
        // Note: In DirectX, depth increases into the screen (0 = near, 1 = far)
        // if (particleDepth > sceneDepth + constants.cullBias) // 0.0001f?
        if (particleDepth > sceneDepth)
        {
            visible = 0;  // Occluded
        }
    }


    // Write visibility
    visibilityBuffer[particleIndex] = visible;
}