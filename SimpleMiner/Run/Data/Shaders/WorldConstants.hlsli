
struct WorldConstants
{
    float4 IndoorLightColor;  
    float4 OutdoorLightColor; 
    float4 SkyColor;       

    float FogNearDistance; 
    float FogFarDistance;   

    float WaterSurfaceHeight;  // Water surface height (Y coordinate)
    float IsUnderwater;        // Whether camera is underwater (0 or 1)

    // Shallow water scattering color (near surface)
    float4 WaterScatterColorShallow; // RGB + intensity
    
    // Deep water scattering color (far from surface)
    float4 WaterScatterColorDeep;    // RGB + intensity

    // Light absorption coefficients (independent RGB attenuation)
    float WaterAbsorptionR;    // Red light absorption coefficient
    float WaterAbsorptionG;    // Green light absorption coefficient
    float WaterAbsorptionB;    // Blue light absorption coefficient
    float WaterVisibilityRange; // Underwater visibility range (blocks)
    
    // Depth color transition parameters
    float WaterDepthTransitionStart; // Depth where color transition starts (blocks)
    float WaterDepthTransitionEnd;   // Depth where color transition completes (blocks)
    float _padding1, _padding2;      // Padding to align to 16 bytes

    // Sun/Moon Angles
    float3 LightDirection;
    float LightningIntensity;

};
