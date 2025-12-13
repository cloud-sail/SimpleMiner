  //-----------------------------------------------------------------------------------------------
  // BloomCopy.hlsl
  // Pixel-level copy: EmissiveRT -> Mip0
  // - Auto format transform
  // - Apply threshold filtering
  //

struct BloomCopyResources
{
    uint inputTextureSRV;
    uint outputTextureUAV;

    float bloomIntensity;
    float bloomThreshold;
};

float3 ApplyThreshold(float3 color, float threshold)
{
    float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
    float brightness = max(0.0, luminance - threshold);

    // Soft transition (avoid hard edges)
    float softness = 0.5;
    brightness = smoothstep(0.0, softness, brightness);

    // keep output color luminance is brightness?
    return color * (brightness / (luminance + 0.0001));
}


//-----------------------------------------------------------------------------------------------
ConstantBuffer<BloomCopyResources> renderResources : register(b0);

//-----------------------------------------------------------------------------------------------
[numthreads(8, 8, 1)]
void ComputeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D<float4> inputTexture = ResourceDescriptorHeap[renderResources.inputTextureSRV];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[renderResources.outputTextureUAV];

    uint2 outputDim;
    outputTexture.GetDimensions(outputDim.x, outputDim.y);

    if (dispatchThreadID.x >= outputDim.x || dispatchThreadID.y >= outputDim.y)
    {
        return;
    }

    float3 color = inputTexture[dispatchThreadID.xy].rgb;

    color = ApplyThreshold(color, renderResources.bloomThreshold);

    outputTexture[dispatchThreadID.xy] = float4(color, 1.0);
}
