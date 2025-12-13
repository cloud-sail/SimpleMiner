#include "Common/Utils.hlsli"
#include "Common/ShaderConstants.hlsli"
#include "Common/Math.hlsli"

//-----------------------------------------------------------------------------------------------
struct vs_input_t
{
	float3 modelSpacePosition : POSITION;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
};

//-----------------------------------------------------------------------------------------------
struct v2p_t
{
	float4 clipSpacePosition : SV_Position;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
	// float3 worldPos	: WORLD_POSITION;
};


float GetBayerValue(uint2 screenPos)
{
    static const float bayerMatrix[16] = {
        0.0f/16.0f,  8.0f/16.0f,  2.0f/16.0f, 10.0f/16.0f,
       12.0f/16.0f,  4.0f/16.0f, 14.0f/16.0f,  6.0f/16.0f,
        3.0f/16.0f, 11.0f/16.0f,  1.0f/16.0f,  9.0f/16.0f,
       15.0f/16.0f,  7.0f/16.0f, 13.0f/16.0f,  5.0f/16.0f
    };
    
    uint index = (screenPos.y % 4) * 4 + (screenPos.x % 4);
    return bayerMatrix[index];
}

struct UnlitDitheringRenderResources
{
    uint diffuseTextureIndex;
    uint diffuseSamplerIndex;
    
    uint cameraConstantsIndex;
    uint modelConstantsIndex;
};

ConstantBuffer<UnlitDitheringRenderResources> renderResources : register(b0);

v2p_t VertexMain(vs_input_t input)
{
    ConstantBuffer<CameraConstants> cameraConstants = ResourceDescriptorHeap[renderResources.cameraConstantsIndex];
    ConstantBuffer<ModelConstants> modelConstants = ResourceDescriptorHeap[renderResources.modelConstantsIndex];

	float4 modelSpacePosition = float4(input.modelSpacePosition, 1);
	float4 worldSpacePosition = mul(modelConstants.modelToWorldTransform, modelSpacePosition);
	float4 cameraSpacePosition = mul(cameraConstants.worldToCameraTransform, worldSpacePosition);
	float4 renderSpacePosition = mul(cameraConstants.cameraToRenderTransform, cameraSpacePosition);
	float4 clipSpacePosition = mul(cameraConstants.renderToClipTransform, renderSpacePosition);

	v2p_t v2p;
	v2p.clipSpacePosition = clipSpacePosition;
	v2p.color = input.color;
	v2p.uv = input.uv;
	// v2p.worldPos = worldSpacePosition.xyz;
	return v2p;
}

float4 PixelMain(v2p_t input) : SV_Target0
{
    ConstantBuffer<ModelConstants> modelConstants = ResourceDescriptorHeap[renderResources.modelConstantsIndex];
    ConstantBuffer<CameraConstants> cameraConstants = ResourceDescriptorHeap[renderResources.cameraConstantsIndex];

    Texture2D<float4> diffuseTexture = ResourceDescriptorHeap[renderResources.diffuseTextureIndex];

	SamplerState diffuseSampler = SamplerDescriptorHeap[renderResources.diffuseSamplerIndex];

	float4 textureColor = diffuseTexture.Sample(diffuseSampler, input.uv);
	float4 vertexColor = input.color;
	float4 modelColor = modelConstants.modelColor;
	float4 color = textureColor * vertexColor * modelColor;
	clip(color.a - 0.01f);

    const float fadeStartDistance = 0.11f;
    const float fadeEndDistance = 1.0f;

	// float distanceToCamera = length(input.worldPos - cameraConstants.cameraWorldPosition);

	const float nearPlane = 0.1f;
    const float farPlane = 850.0f;

	float depth = input.clipSpacePosition.z;
    float linearDepth = nearPlane * farPlane / (farPlane - depth * (farPlane - nearPlane));
	// distanceToCamera = linearDepth;

	if (linearDepth < fadeStartDistance)
    {
        discard;
    }
    
    if (linearDepth >= fadeEndDistance)
    {
        return color;
    }

	float fadeFactor = (linearDepth - fadeStartDistance) / (fadeEndDistance - fadeStartDistance);
	fadeFactor = SmoothStep3(fadeFactor);
    
    uint2 screenPos = (uint2)input.clipSpacePosition.xy;
    float ditherThreshold = GetBayerValue(screenPos);

	if (fadeFactor < ditherThreshold)
    {
        discard;
    }

	return float4(color);
}