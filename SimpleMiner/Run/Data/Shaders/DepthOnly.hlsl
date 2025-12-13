#include "Common/Utils.hlsli"
#include "Common/ShaderConstants.hlsli"

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
};


struct DepthOnlyResources
{
    uint cameraConstantsIndex;
    uint modelConstantsIndex;
};


ConstantBuffer<DepthOnlyResources> renderResources : register(b0);

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
	return v2p;
}

// // Depth-only pixel shader - can be empty or omitted entirely
void PixelMain(v2p_t input)
{
	// Empty - depth is automatically written by hardware
}
