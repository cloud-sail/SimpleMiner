//-----------------------------------------------------------------------------------------------
// BloomComposite.hlsl
// Scene + Bloom -> Output
//

struct BloomCompositeResources
{
    uint sceneTextureSRV;
    uint bloomTextureSRV;
    uint outputTextureUAV;
    uint samplerIndex;

    float bloomIntensity;
    float bloomThreshold;
};


//-----------------------------------------------------------------------------------------------
ConstantBuffer<BloomCompositeResources> renderResources : register(b0);

//-----------------------------------------------------------------------------------------------
// Composite Main
//
[numthreads(8, 8, 1)]
void ComputeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D<float4> sceneTexture = ResourceDescriptorHeap[renderResources.sceneTextureSRV];
    Texture2D<float4> bloomTexture = ResourceDescriptorHeap[renderResources.bloomTextureSRV];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[renderResources.outputTextureUAV];

    uint2 outputDim;
    outputTexture.GetDimensions(outputDim.x, outputDim.y);

    if (dispatchThreadID.x >= outputDim.x || dispatchThreadID.y >= outputDim.y)
        return;

    float3 sceneColor = sceneTexture[dispatchThreadID.xy].rgb;

    float3 bloomColor = bloomTexture[dispatchThreadID.xy].rgb;

    bloomColor *= renderResources.bloomIntensity;

    // Composite：Scene + Bloom
    float3 finalColor = sceneColor + bloomColor;

    outputTexture[dispatchThreadID.xy] = float4(finalColor, 1.0); // #ToDo Keep scene color alpha?
}
