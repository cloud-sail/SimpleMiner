#pragma once

struct UnlitRenderResources
{
    uint diffuseTextureIndex;
    uint diffuseSamplerIndex;
    
    uint cameraConstantsIndex;
    uint modelConstantsIndex;
};

struct DiffuseRenderResources
{
    uint diffuseTextureIndex;
    uint diffuseSamplerIndex;
    
    uint cameraConstantsIndex;
    uint modelConstantsIndex;
    uint lightConstantsIndex;
};

struct BlinnPhongRenderResources
{
    uint diffuseTextureIndex;
    uint normalTextureIndex;
    uint specGlossEmitTextureIndex;
    
    uint engineConstantsIndex;
    uint cameraConstantsIndex;
    uint modelConstantsIndex;
    uint lightConstantsIndex;
};

struct SkyboxRenderResources
{
    uint cubeMapTextureIndex;

    uint cameraConstantsIndex;
    uint modelConstantsIndex;
};