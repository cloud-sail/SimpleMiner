#include "Common/Utils.hlsli"
#include "Common/ShaderConstants.hlsli"
#include "Common/Resources.hlsli"
#include "Common/StaticSampler.hlsli"
#include "Common/Math.hlsli"
#include "WorldConstants.hlsli"

//-----------------------------------------------------------------------------------------------
struct vs_input_t
{
	float3 modelSpacePosition : POSITION;
	float4 color : COLOR;	// R = Outdoor Light Influence, G = Indoor Light Influence
	float2 uv : TEXCOORD;
};

//-----------------------------------------------------------------------------------------------
struct v2p_t
{
	float4 clipSpacePosition : SV_Position;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
	float3 worldPos	: WORLD_POSITION;
    noperspective float2 screenUV : SCREEN_UV;
};

struct DitherFadeRenderResources
{
    uint diffuseTextureIndex;
    uint diffuseSamplerIndex;
    
    uint cameraConstantsIndex;
    uint modelConstantsIndex;

	float fadeAmount; // 0 is transparent 1 is opaque

	uint worldConstantsIndex;
    uint skyQuadSRVIndex;
};

//-----------------------------------------------------------------------------------------------
float GetBayerValue(uint2 screenPos)
{
    static const float bayerMatrix[64] = {
         0.0f/64.0f, 32.0f/64.0f,  8.0f/64.0f, 40.0f/64.0f,  2.0f/64.0f, 34.0f/64.0f, 10.0f/64.0f, 42.0f/64.0f,
        48.0f/64.0f, 16.0f/64.0f, 56.0f/64.0f, 24.0f/64.0f, 50.0f/64.0f, 18.0f/64.0f, 58.0f/64.0f, 26.0f/64.0f,
        12.0f/64.0f, 44.0f/64.0f,  4.0f/64.0f, 36.0f/64.0f, 14.0f/64.0f, 46.0f/64.0f,  6.0f/64.0f, 38.0f/64.0f,
        60.0f/64.0f, 28.0f/64.0f, 52.0f/64.0f, 20.0f/64.0f, 62.0f/64.0f, 30.0f/64.0f, 54.0f/64.0f, 22.0f/64.0f,
         3.0f/64.0f, 35.0f/64.0f, 11.0f/64.0f, 43.0f/64.0f,  1.0f/64.0f, 33.0f/64.0f,  9.0f/64.0f, 41.0f/64.0f,
        51.0f/64.0f, 19.0f/64.0f, 59.0f/64.0f, 27.0f/64.0f, 49.0f/64.0f, 17.0f/64.0f, 57.0f/64.0f, 25.0f/64.0f,
        15.0f/64.0f, 47.0f/64.0f,  7.0f/64.0f, 39.0f/64.0f, 13.0f/64.0f, 45.0f/64.0f,  5.0f/64.0f, 37.0f/64.0f,
        63.0f/64.0f, 31.0f/64.0f, 55.0f/64.0f, 23.0f/64.0f, 61.0f/64.0f, 29.0f/64.0f, 53.0f/64.0f, 21.0f/64.0f
    };
    
    uint index = (screenPos.y % 8) * 8 + (screenPos.x % 8);
    return bayerMatrix[index];
}

float3 DiminishingAdd(float3 a, float3 b)
{
    return 1.0f - (1.0f - a) * (1.0f - b);
}


//-----------------------------------------------------------------------------------------------
float3 ApplyUnderwaterOptics(
    float3 sceneColor, 
    float distanceToCamera,
    float3 worldPos,
    ConstantBuffer<WorldConstants> worldConstants)
{
    // === 1. Calculate underwater depth ===
    float waterDepth = max(0.0f, worldConstants.WaterSurfaceHeight - worldPos.z);
    
    // === 2. Build absorption coefficients (Beer-Lambert Law) ===
    float3 absorption = float3(
        worldConstants.WaterAbsorptionR,
        worldConstants.WaterAbsorptionG,
        worldConstants.WaterAbsorptionB
    );
    
    // === 3. Calculate transmittance ===
    float3 transmittance = exp(-distanceToCamera * absorption);
    
    // === 4. Depth-dependent scattering color ===
    float depthBlend = smoothstep(
        worldConstants.WaterDepthTransitionStart,
        worldConstants.WaterDepthTransitionEnd,
        waterDepth
    );
    
    float3 scatterColor = lerp(
        worldConstants.WaterScatterColorShallow.rgb,
        worldConstants.WaterScatterColorDeep.rgb,
        depthBlend
    );
    
    // === 5. Compose final color (energy conservation) ===
    // Scene color is absorbed, scattered light fills in
    float3 attenuatedScene = sceneColor * transmittance;
    float3 scatteredLight = scatterColor * (1.0f - transmittance);
    
    return attenuatedScene + scatteredLight;
}

//-----------------------------------------------------------------------------------------------
ConstantBuffer<DitherFadeRenderResources> renderResources : register(b0);

v2p_t VertexMain(vs_input_t input)
{
    ConstantBuffer<CameraConstants> cameraConstants = ResourceDescriptorHeap[renderResources.cameraConstantsIndex];
    ConstantBuffer<ModelConstants> modelConstants = ResourceDescriptorHeap[renderResources.modelConstantsIndex];

	float4 modelSpacePosition = float4(input.modelSpacePosition, 1);
	float4 worldSpacePosition = mul(modelConstants.modelToWorldTransform, modelSpacePosition);

	const float MAX_HEIGHT = 64.0f;
    float verticalOffset = (1.0f - renderResources.fadeAmount) * MAX_HEIGHT;
    worldSpacePosition.z -= verticalOffset;

	float4 cameraSpacePosition = mul(cameraConstants.worldToCameraTransform, worldSpacePosition);
	float4 renderSpacePosition = mul(cameraConstants.cameraToRenderTransform, cameraSpacePosition);
	float4 clipSpacePosition = mul(cameraConstants.renderToClipTransform, renderSpacePosition);

	v2p_t v2p;
	v2p.clipSpacePosition = clipSpacePosition;
	v2p.color = input.color;
	v2p.uv = input.uv;
	v2p.worldPos = worldSpacePosition.xyz;

    float2 ndc = clipSpacePosition.xy / clipSpacePosition.w;  // [-1, 1]
    v2p.screenUV = ndc * 0.5 + 0.5;                           // [0, 1]
    v2p.screenUV.y = 1.0 - v2p.screenUV.y;                    // flip Y

	return v2p;
}

float4 PixelMain(v2p_t input) : SV_Target0
{
	// Step 1: Dither Fading
    uint2 screenPos = uint2(input.clipSpacePosition.xy);
    float ditherThreshold = GetBayerValue(screenPos);
	if (renderResources.fadeAmount <= 0.f)
	{
		discard;
	}
    clip(renderResources.fadeAmount - ditherThreshold);

	// Step 2: Get Constant Buffers
    ConstantBuffer<CameraConstants> cameraConstants = ResourceDescriptorHeap[renderResources.cameraConstantsIndex];
    ConstantBuffer<ModelConstants> modelConstants = ResourceDescriptorHeap[renderResources.modelConstantsIndex];
    ConstantBuffer<WorldConstants> worldConstants = ResourceDescriptorHeap[renderResources.worldConstantsIndex];

	// Step 3: Sample Texture
    Texture2D<float4> diffuseTexture = ResourceDescriptorHeap[renderResources.diffuseTextureIndex];
	float4 texelColor = diffuseTexture.Sample(s_pointMipLinearClamp, input.uv);
	clip(texelColor.a - 0.01f);

	// Step 4: Light Influence
    float3 outdoorInfluence = input.color.r * worldConstants.OutdoorLightColor.rgb;
    float3 indoorInfluence = input.color.g * worldConstants.IndoorLightColor.rgb;
	// Alpha Blending
    float3 diffuseLightColor = DiminishingAdd(outdoorInfluence, indoorInfluence);

	// Step 5: Calculate Color
	float3 litColor = texelColor.rgb * diffuseLightColor * input.color.b * modelConstants.modelColor.rgb;

	// Step 6: Calculate Distance
    float3 cameraToPixel = input.worldPos - cameraConstants.cameraWorldPosition;
    float distanceToCamera = length(cameraToPixel);
    
    // === Step 7: Apply Underwater Optics (if underwater) ===
    if (worldConstants.IsUnderwater > 0.5f)
    {
        litColor = ApplyUnderwaterOptics(
            litColor,
            distanceToCamera,
            input.worldPos,
            worldConstants
        );
    }
    else
    {
        // === Step 8: Apply Atmospheric Fog (if not underwater) ===
        float fogFraction = saturate((distanceToCamera - worldConstants.FogNearDistance) / 
                                     (worldConstants.FogFarDistance - worldConstants.FogNearDistance));
        // Test feature
        fogFraction = SmoothStep3(fogFraction);
        float fogAlpha = fogFraction * worldConstants.SkyColor.a; // Fog Max Alpha? = 1


        if (fogAlpha > 0.f)
        {
            Texture2D<float4> skyQuadTexture = ResourceDescriptorHeap[renderResources.skyQuadSRVIndex];
            float4 skyColor = skyQuadTexture.Sample(s_pointClamp, input.screenUV); // Or use GetDimensions
            litColor = lerp(litColor, skyColor.rgb, fogAlpha);
        }
    }

    float finalAlpha = texelColor.a * modelConstants.modelColor.a;
	clip(finalAlpha - 0.01f);

	return float4(litColor, finalAlpha);
}


	// SamplerState diffuseSampler = SamplerDescriptorHeap[renderResources.diffuseSamplerIndex];
	// float4 textureColor = diffuseTexture.Sample(diffuseSampler, input.uv);
	// float4 textureColor = diffuseTexture.SampleBias(diffuseSampler, input.uv, -0.5);
