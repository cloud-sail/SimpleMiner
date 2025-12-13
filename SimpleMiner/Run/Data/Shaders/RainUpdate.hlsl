#include "RainUtils.hlsli"

//-----------------------------------------------------------------------------------------------
// Resources
struct RainUpdateResources
{
    uint particleBufferIndex;  // RWStructuredBuffer<RainParticle>
    uint rainConstantsIndex;   // ConstantBuffer<RainConstants>
};

ConstantBuffer<RainUpdateResources> renderResources : register(b0);

//-----------------------------------------------------------------------------------------------
[numthreads(256, 1, 1)]
void ComputeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    RWStructuredBuffer<RainParticle> particleBuffer = ResourceDescriptorHeap[renderResources.particleBufferIndex];
    ConstantBuffer<RainConstants> constants = ResourceDescriptorHeap[renderResources.rainConstantsIndex];

    uint particleIndex = dispatchThreadID.x;
    if (particleIndex >= constants.numParticles)
        return;

    // Read particle
    RainParticle particle = particleBuffer[particleIndex];

    // Step 1: Apply box center delta (compensate for player movement)
    // When player moves, particles appear to move in opposite direction in relative space
    particle.relativePosition -= constants.boxCenterDelta;

    // Step 2: Update physics
    particle.relativePosition += constants.velocity * constants.deltaTime;

    // Step 3: Wrap particles on all axes
    // Wrap X axis
    if (particle.relativePosition.x > constants.boxExtents.x)
        particle.relativePosition.x -= 2.0 * constants.boxExtents.x;
    else if (particle.relativePosition.x < -constants.boxExtents.x)
        particle.relativePosition.x += 2.0 * constants.boxExtents.x;

    // Wrap Y axis
    if (particle.relativePosition.y > constants.boxExtents.y)
        particle.relativePosition.y -= 2.0 * constants.boxExtents.y;
    else if (particle.relativePosition.y < -constants.boxExtents.y)
        particle.relativePosition.y += 2.0 * constants.boxExtents.y;

    // Wrap Z axis (vertical)
    if (particle.relativePosition.z > constants.boxExtents.z)
        particle.relativePosition.z -= 2.0 * constants.boxExtents.z;
    else if (particle.relativePosition.z < -constants.boxExtents.z)
        particle.relativePosition.z += 2.0 * constants.boxExtents.z;

    // Write back
    particleBuffer[particleIndex] = particle;

}