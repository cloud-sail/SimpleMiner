#include "Common/ShaderConstants.hlsli"
#include "RainUtils.hlsli"

//-----------------------------------------------------------------------------------------------
// Vertex to Pixel
struct v2p_t
{
    float4 clipSpacePosition : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR;
};


//-----------------------------------------------------------------------------------------------
// Resources
struct RainRenderResources
{
    uint particleBufferIndex;      // StructuredBuffer<RainParticle>
    uint visibilityBufferIndex;    // StructuredBuffer<uint>
    uint cameraConstantsIndex;
    uint rainConstantsIndex;
};

//-----------------------------------------------------------------------------------------------
ConstantBuffer<RainRenderResources> renderResources : register(b0);

//-----------------------------------------------------------------------------------------------
// Vertex Shader - Generate billboard quads using instance drawing
// instanceID: particle index
v2p_t VertexMain(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    StructuredBuffer<RainParticle> particleBuffer = ResourceDescriptorHeap[renderResources.particleBufferIndex];
    StructuredBuffer<uint> visibilityBuffer = ResourceDescriptorHeap[renderResources.visibilityBufferIndex];
    ConstantBuffer<CameraConstants> cameraConstants = ResourceDescriptorHeap[renderResources.cameraConstantsIndex];
    ConstantBuffer<RainConstants> constants = ResourceDescriptorHeap[renderResources.rainConstantsIndex];

    uint particleIndex = instanceID;

    v2p_t output;

    // Check visibility
    uint isVisible = visibilityBuffer[particleIndex];

    if (isVisible == 0)
    {
        // Degenerate triangle: output all vertices to same point outside clip space
        output.clipSpacePosition = float4(0, 0, 0, 0);  // Will be clipped
        output.uv = float2(0, 0);
        output.color = float4(0, 0, 0, 0);
        return output;
    }

    // Read particle
    RainParticle particle = particleBuffer[particleIndex];

    // Convert relative position to world position
    float3 particlePosition = constants.boxCenter + particle.relativePosition;
    float3 cameraPosition = cameraConstants.cameraWorldPosition;
    float3 quadUp = -normalize(constants.velocity);
    float3 toCamera = normalize(cameraPosition - particlePosition);

    float alignment = abs(dot(quadUp, toCamera));
    float3 quadLeft;

    if (alignment > 0.99) 
    {
        float3 fallback = abs(quadUp.y) < 0.99 ? float3(0, 1, 0) : float3(1, 0, 0);
        quadLeft = normalize(cross(quadUp, fallback));
    } else 
    {
        quadLeft = normalize(cross(quadUp, toCamera));
    }

    uint quadVertexIndices[6] = {
        0, 1, 2, 
        0, 2, 3 
    };

    uint quadVertexID = quadVertexIndices[vertexID];  // 0-5 to 0,1,2,0,2,3

    // Quad vertices in local space 
    // 0: bottom-left, 1: bottom-right, 2: top-right, 3: top-left
    float2 quadOffsets[4] = {
        float2(-0.5,  0.5),     // Bottom-left
        float2( 0.5,  0.5),     // Bottom-right
        float2( 0.5, -0.5),     // Top-right
        float2(-0.5, -0.5)      // Top-left
    };

    // UV coordinates
    float2 quadUVs[4] = {
        float2(0.0, 1.0),   // Bottom-left
        float2(1.0, 1.0),   // Bottom-right
        float2(1.0, 0.0),   // Top-right
        float2(0.0, 0.0)    // Top-left
    };

    float2 offset = quadOffsets[quadVertexID] * constants.particleSize;
    float2 uv = quadUVs[quadVertexID];

    // Billboard the particle (camera-facing quad)
    float3 billboardPosition = particlePosition + quadLeft * offset.x + quadUp * offset.y;

    // Transform to clip space
    float4 worldPos4 = float4(billboardPosition, 1.0);
    float4 cameraSpacePosition = mul(cameraConstants.worldToCameraTransform, worldPos4);
    float4 renderSpacePosition = mul(cameraConstants.cameraToRenderTransform, cameraSpacePosition);
    float4 clipSpacePosition = mul(cameraConstants.renderToClipTransform, renderSpacePosition);

    output.clipSpacePosition = clipSpacePosition;
    output.uv = uv;
    output.color = constants.rainColor;

    return output;
}

float sdHexagram(in float2 p, in float r)
{
    const float4 k = float4(-0.5, 0.8660254038, 0.5773502692, 1.7320508076);
    p = abs(p);
    p -= 2.0 * min(dot(k.xy, p), 0.0) * k.xy;
    p -= 2.0 * min(dot(k.yx, p), 0.0) * k.yx;
    p -= float2(clamp(p.x, r * k.z, r * k.w), r);
    return length(p) * sign(p.y);
}

//-----------------------------------------------------------------------------------------------
// Pixel Shader - Render pure color rain with gradient
float4 PixelMain(v2p_t input) : SV_Target0
{
    float2 uv = input.uv * 2.0 - 1.0;
    
    float d = sdHexagram(uv, 0.5);
    
    float alpha = (1.0 - smoothstep(-0.01, 0.01, d)) * input.color.a;
    
    return float4(input.color.rgb, alpha);
}