#include "Common/ShaderConstants.hlsli"
#include "WorldConstants.hlsli"

struct SkyResources
{
    uint cameraConstantsIndex;
	uint worldConstantsIndex;
};


//-----------------------------------------------------------------------------------------------
struct v2p_t
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

//-----------------------------------------------------------------------------------------------
struct PS_Output
{
	float4 color0 : SV_Target0; // RT0: sceneRT
	float4 color1 : SV_Target1; // RT1: skyRT
};

float sdHexagon(in float2 p, in float r)
{
    const float3 k = float3(-0.866025404, 0.5, 0.577350269);
    p = abs(p);
    p -= 2.0 * min(dot(k.xy, p), 0.0) * k.xy;
    p -= float2(clamp(p.x, -k.z * r, k.z * r), r);
    return length(p) * sign(p.y);
}

float2 rotate2D(float2 p, float angle)
{
    float c = cos(angle);
    float s = sin(angle);
    return float2(p.x * c - p.y * s, p.x * s + p.y * c);
}

// MakeFromX
void createLocalFrame(float3 direction, out float3 right, out float3 up)
{
    float3 arbitrary = abs(direction.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    right = normalize(cross(arbitrary, direction));
    up = cross(direction, right);
}

// 
float3 hexagonWithFacets(float2 uv, float3 baseColor, float brightness)
{
    float dist = sdHexagon(uv, 1.0);
    
    float hexMask = 1.0 - smoothstep(-0.02, 0.02, dist);
    
    if (hexMask < 0.01) return float3(0, 0, 0);
    
    float angle = atan2(uv.y, uv.x);
    
    // 3 Sectors (120 * 3)
    float normalizedAngle = (angle + 3.14159265) / (3.14159265 * 2.0);
    float sector = floor(normalizedAngle * 3.0);
    
    // Different brightness in different sectors
    float3 facetColor = baseColor;
    if (sector == 0.0)
    {
        facetColor *= brightness * 1.0;
    }
    else if (sector == 1.0)
    {
        facetColor *= brightness * 0.7;
    }
    else
    {
        facetColor *= brightness * 0.5;
    }
    
    float edge = smoothstep(-0.05, 0.0, dist) * smoothstep(0.0, -0.05, dist);
    facetColor += edge * 0.3;
    
    return facetColor * hexMask;
}

ConstantBuffer<SkyResources> renderResources : register(b0);

v2p_t VertexMain(uint vertexID : SV_VertexID)
{
    float2 ndc[4] = {
        float2(-1.0,  1.0),
        float2(-1.0, -1.0),
        float2( 1.0,  1.0),
        float2( 1.0, -1.0)
    };
    float2 uv[4] = {
        float2(0.0, 0.0),
        float2(0.0, 1.0),
        float2(1.0, 0.0),
        float2(1.0, 1.0)
    };

    uint indices[6] = { 0, 1, 2, 2, 1, 3 }; // triangle list

	v2p_t v2p;
    v2p.pos = float4(ndc[indices[vertexID]], 1.0, 1.0);
    v2p.uv  = uv[indices[vertexID]];
    return v2p;
}

// https://godotshaders.com/shader/stylized-sky-shader-with-clouds/ 
PS_Output  PixelMain(v2p_t input)
{
    ConstantBuffer<CameraConstants> cameraConstants = ResourceDescriptorHeap[renderResources.cameraConstantsIndex];
    ConstantBuffer<WorldConstants> worldConstants = ResourceDescriptorHeap[renderResources.worldConstantsIndex];

    float2 ndc = input.uv * 2.0 - 1.0;
    ndc.y = -ndc.y; 
    float4 clipPos = float4(ndc, 1.0, 1.0);
    float4 worldPos = mul(cameraConstants.clipToWorldTransform, clipPos);
    //-----------------------------------------------------------------------------------------------
    // Preparation
    const float3 EYE_DIR = normalize(worldPos.xyz / worldPos.w - cameraConstants.cameraWorldPosition);
    const float3 LIGHT_DIRECTION = normalize(worldConstants.LightDirection); // Must be normalized, z > 0 Sun, z < 0 Moon
    const float2 SKY_UV = EYE_DIR.xy / EYE_DIR.z; // 3D -> 2D for cloud, star sampling

    const float3 DAY_BOTTOM_COLOR =  float3(0.5, 0.8, 1.0);     // LIGHT BLUE at horizon
    const float3 DAY_TOP_COLOR =  float3(0.5, 0.8, 1.0);        // DARK BLUE at zenith

    const float3 SUNSET_BOTTOM_COLOR =  float3(1.0, 0.6, 0.2);  // ORANGE-RED at horizon
    const float3 SUNSET_TOP_COLOR =  float3(0.8, 0.4, 0.6);     // PURPLE-PINK at zenith

    const float3 NIGHT_BOTTOM_COLOR =  float3(0.02, 0.02, 0.08);   // LIGHT BLUE at horizon
    const float3 NIGHT_TOP_COLOR =  float3(0.0, 0.1, 0.2);      // DARK BLUE at zenith

    const float HORIZON_FALLOFF = -0.1f;
    const float HORIZON_FACTOR = 0.3f;
    const float3 HOIZON_DAY_COLOR = float3(0.6, 0.8, 1.0) ;
    const float3 HOIZON_SUNSET_COLOR = float3(1.0, 0.4, 0.15);
    const float3 HOIZON_NIGHT_COLOR = float3(0.05, 0.08, 0.15);

    const float SUN_SIZE = 0.16f;
    const float SUN_BLUR = 0.08f;
    const float3 SUN_COLOR = float3(1.0, 0.9, 0.7);

    const float MOON_SIZE = 0.13f;
    const float MOON_BLUR = 0.05f;
    const float3 MOON_COLOR = float3(0.8, 0.8, 1.0);

    //-----------------------------------------------------------------------------------------------
    // Sky Gradient
    float dayAmount = saturate(LIGHT_DIRECTION.z);
    float3 dayGradient = lerp(DAY_BOTTOM_COLOR, DAY_TOP_COLOR, saturate(EYE_DIR.z)) * dayAmount;
    
    float sunsetAmount = saturate(1.0 - abs(LIGHT_DIRECTION.z));
    float3 sunsetGradient = lerp(SUNSET_BOTTOM_COLOR, SUNSET_TOP_COLOR, saturate(EYE_DIR.z)) * sunsetAmount;
    
    float nightAmount = saturate(-LIGHT_DIRECTION.z);
    float3 nightGradient = lerp(NIGHT_BOTTOM_COLOR, NIGHT_TOP_COLOR, saturate(EYE_DIR.z)) * nightAmount;
    
    float3 skyGradients = dayGradient + sunsetGradient + nightGradient;


    //-----------------------------------------------------------------------------------------------
    // Horizon Glow
    float horizon = 1.0 - abs(EYE_DIR.z + HORIZON_FALLOFF);
    
    float3 horizonGlowAmountDay = saturate(horizon * dayAmount) * HOIZON_DAY_COLOR;
    float3 horizonGlowAmountSunset = saturate(horizon * sunsetAmount) * HOIZON_SUNSET_COLOR;
    float3 horizonGlowAmountNight = saturate(horizon * nightAmount) * HOIZON_NIGHT_COLOR;

    float3 horizonGlow = (horizonGlowAmountDay + horizonGlowAmountSunset + horizonGlowAmountNight) * HORIZON_FACTOR;
    

    //-----------------------------------------------------------------------------------------------
    float sunDistance = distance(EYE_DIR, LIGHT_DIRECTION);
    
    float3 sun = float3(0, 0, 0);
    if (sunDistance < SUN_SIZE * 2.0)
    {
        // Local Coords
        float3 sunRight, sunUp;
        createLocalFrame(LIGHT_DIRECTION, sunRight, sunUp);
        
        // float3 toEye = EYE_DIR - LIGHT_DIRECTION * dot(EYE_DIR, LIGHT_DIRECTION);
        // float2 sunUV = float2(dot(toEye, sunRight), dot(toEye, sunUp)) / SUN_SIZE;
        float2 sunUV = float2(dot(EYE_DIR, sunRight), dot(EYE_DIR, sunUp)) / SUN_SIZE;
        
        sunUV = rotate2D(sunUV, 0.0);
        sun = hexagonWithFacets(sunUV, SUN_COLOR, 1.2);
        
        float sunGlow = 1.0 - saturate(sunDistance / (SUN_SIZE * 2.0));
        sun += SUN_COLOR * sunGlow * sunGlow * 0.3;
    }
    
    //-----------------------------------------------------------------------------------------------
    float moonDistance = distance(EYE_DIR, -LIGHT_DIRECTION);
    
    float3 moon = float3(0, 0, 0);
    if (moonDistance < MOON_SIZE * 2.0)
    {
        float3 moonRight, moonUp;
        createLocalFrame(-LIGHT_DIRECTION, moonRight, moonUp);
        
        // float3 toEye = EYE_DIR - (-LIGHT_DIRECTION) * dot(EYE_DIR, -LIGHT_DIRECTION);
        // float2 moonUV = float2(dot(toEye, moonRight), dot(toEye, moonUp)) / MOON_SIZE;
        float2 moonUV = float2(dot(EYE_DIR, moonRight), dot(EYE_DIR, moonUp)) / MOON_SIZE;
        
        moonUV = rotate2D(moonUV, 0.0);
        moon = hexagonWithFacets(moonUV, MOON_COLOR, 0.9);
        
        float moonGlow = 1.0 - saturate(moonDistance / (MOON_SIZE * 2.0));
        moon += MOON_COLOR * moonGlow * moonGlow * 0.2;
    }
    // //-----------------------------------------------------------------------------------------------
    // // Sun SDF
    // float sunDistance = distance(EYE_DIR, LIGHT_DIRECTION);
    // float sunPower = 1.0 - saturate(sunDistance / SUN_SIZE);
    // float sunDisc = saturate(sunPower / SUN_BLUR);
    // float3 sun = SUN_COLOR * sunDisc;
    
    
    // //-----------------------------------------------------------------------------------------------
    // // Moon SDF
    // float moonDistance = distance(EYE_DIR, -LIGHT_DIRECTION);
    // float moonPower = 1.0 - saturate(moonDistance / MOON_SIZE);
    // float moonDisc = saturate(moonPower / MOON_BLUR);
    // float3 moon = MOON_COLOR * moonDisc;
    
    float3 sky = skyGradients + horizonGlow * 0.5;
    
    // Apply lightning effect to sky ONLY (not to sun/moon)
    float lightningIntensity = saturate(worldConstants.LightningIntensity);
    sky = lerp(sky, float3(1.0, 1.0, 1.0), lightningIntensity);
    
    // Screen blend
    sky = 1.0 - (1.0 - sky) * (1.0 - sun);

    // Screen blend
    sky = 1.0 - (1.0 - sky) * (1.0 - moon);
    // sky = lerp(sky, sun, saturate(length(sun)));
    // sky = lerp(sky, moon, saturate(length(moon)));
    
    sky = saturate(sky);

    PS_Output output;
    output.color0 = float4(sky, 1.0);
    output.color1 = float4(sky, 1.0);
    
    return output;
}