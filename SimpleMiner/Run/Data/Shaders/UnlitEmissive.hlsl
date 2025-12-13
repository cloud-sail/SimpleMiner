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
	float4 color : COLOR;
	float2 uv : TEXCOORD;
};

//-----------------------------------------------------------------------------------------------
struct PS_Output
{
	float4 baseColor : SV_Target0;  // RT0: BaseColor
	float4 emissive : SV_Target1;   // RT1: Emissive
};

struct UnlitEmissiveResources
{
    uint diffuseTextureIndex;
    uint diffuseSamplerIndex;
    
    uint cameraConstantsIndex;
    uint modelConstantsIndex;

    uint emissiveTextureIndex;
	float emissiveStrength;

};


ConstantBuffer<UnlitEmissiveResources> renderResources : register(b0);

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
	return v2p;
}

PS_Output PixelMain(v2p_t input)
{
    ConstantBuffer<ModelConstants> modelConstants = ResourceDescriptorHeap[renderResources.modelConstantsIndex];

    Texture2D<float4> diffuseTexture = ResourceDescriptorHeap[renderResources.diffuseTextureIndex];

	SamplerState diffuseSampler = SamplerDescriptorHeap[renderResources.diffuseSamplerIndex];

	float4 textureColor = diffuseTexture.Sample(diffuseSampler, input.uv);

	float4 vertexColor = input.color;
	float4 modelColor = modelConstants.modelColor;
	float4 color = textureColor * vertexColor * modelColor;

	clip(color.a - 0.01f);

	PS_Output output;
	output.baseColor = color;

	Texture2D<float4> emissiveTexture = ResourceDescriptorHeap[renderResources.emissiveTextureIndex];
	float3 emissive = emissiveTexture.Sample(diffuseSampler, input.uv).rgb;
	output.emissive = float4(emissive * renderResources.emissiveStrength, 1.0);
	
	return output;
}