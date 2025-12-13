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

	float3 baseColor = textureColor.rgb * vertexColor.rgb * modelColor.rgb;

	// IMPORTANT: input.color.a is already normalized to 0.0-1.0 by GPU
	// We need to map it back to 0.0-2.0 range
	// Formula: brightness = alpha * 2.0
	// Example: alpha = 1.0   → brightness = 2.0 (peak flash)
	//          alpha = 0.5   → brightness = 1.0 (normal)
	//          alpha = 0.197 → brightness = 0.4 (leader phase)
	//          alpha = 0.0   → brightness = 0.0 (dark)
	float brightness = input.color.a * 2.0; // 2.0 is the largest brightness in the animation

	clip(textureColor.a - 0.01f);

	PS_Output output;
	output.baseColor = float4(baseColor * brightness, 1.0);

	Texture2D<float4> emissiveTexture = ResourceDescriptorHeap[renderResources.emissiveTextureIndex];
	float3 emissiveMask = emissiveTexture.Sample(diffuseSampler, input.uv).rgb;

	// WHITENING EFFECT: Real lightning becomes white when very bright
	// At low brightness: show full color (yellow, blue, etc.)
	// At high brightness (>1.0): lerp towards white (overexposure effect)
	// 
	// whiteFactor calculation:
	// brightness = 0.5 → whiteFactor = 0.0 (0% white, 100% color)
	// brightness = 1.0 → whiteFactor = 0.0 (0% white, 100% color)
	// brightness = 1.5 → whiteFactor = 0.5 (50% white, 50% color)
	// brightness = 2.0 → whiteFactor = 1.0 (100% white, 0% color) ← Intense white flash!
	float whiteFactor = saturate((brightness - 1.0) / 1.0);
	float3 emissiveColor = lerp(baseColor, float3(1.0, 1.0, 1.0), whiteFactor);

		// Final emissive calculation with whitening
	// This produces HDR values (can be >> 1.0) for bloom
	// 
	// Example with yellow lightning (0.99, 0.75, 0.12), emissiveStrength = 4.0:
	// 
	// Leader phase (brightness = 0.4):
	//   whiteFactor = 0.0 (no whitening)
	//   emissiveColor = (0.99, 0.75, 0.12)
	//   emissive = (0.99, 0.75, 0.12) * 0.4 * 4.0 = (1.58, 1.20, 0.19)
	//   Luminance = 1.18 → light bloom
	// 
	// Return Stroke 1 peak (brightness = 2.0):
	//   whiteFactor = 1.0 (full whitening!)
	//   emissiveColor = (1.0, 1.0, 1.0) ← Pure white!
	//   emissive = (1.0, 1.0, 1.0) * 2.0 * 4.0 = (8.0, 8.0, 8.0) ← Intense white flash!
	//   Luminance = 8.0 → explosive bloom
	// 
	// Return Stroke 1 decay (brightness = 1.2):
	//   whiteFactor = 0.2 (20% white)
	//   emissiveColor = lerp((0.99,0.75,0.12), (1,1,1), 0.2) = (0.99, 0.80, 0.30) ← Yellowish-white
	//   emissive = (0.99, 0.80, 0.30) * 1.2 * 4.0 = (4.75, 3.84, 1.44)
	//   Luminance = 3.92 → strong bloom
	// 
	// Fade out (brightness = 0.2):
	//   whiteFactor = 0.0 (no whitening)
	//   emissiveColor = (0.99, 0.75, 0.12)
	//   emissive = (0.99, 0.75, 0.12) * 0.2 * 4.0 = (0.79, 0.60, 0.10)
	//   Luminance = 0.59 → below threshold, no bloom
	float3 emissive = emissiveColor * brightness * renderResources.emissiveStrength;
	
	output.emissive = float4(emissive * emissiveMask, 1.0);
	
	return output;
}
