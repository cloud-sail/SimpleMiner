

struct RainParticle
{
    float3 relativePosition;  // Position relative to box center
    float padding0;
};

struct RainConstants
{
    float4x4 worldToClipTransform;
    float4 rainColor;

    float2 particleSize;
    uint numParticles;
    float padding0;

    float3 velocity;            // the particle is move in uniform speed
    float padding1;

    float3 boxCenter;          // Player position / box center (world space)
    float padding2;

    float3 boxExtents;         // Half-size of the simulation box
    uint padding3;

    float3 boxCenterDelta;     // Movement of box center this frame
    float deltaTime;
};


// struct RainCullConstants
// {
//     float4x4 worldToClipTransform;
    
//     float3 boxCenter;
//     float cullBias;            // Bias to avoid z-fighting

//     float3 boxExtents;
//     uint numParticles;
// };

