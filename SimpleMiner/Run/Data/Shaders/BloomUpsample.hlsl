struct BloomUpsampleResources
{
    uint lowResTextureSRV;
    uint highResTextureUAV;
    uint samplerIndex;
};

//-----------------------------------------------------------------------------------------------
ConstantBuffer<BloomUpsampleResources> renderResources : register(b0);

//-----------------------------------------------------------------------------------------------
// 3x3 Tent Filter Upsample
//
[numthreads(8, 8, 1)]
void ComputeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D<float4> lowResTexture = ResourceDescriptorHeap[renderResources.lowResTextureSRV];
    RWTexture2D<float4> highResTexture = ResourceDescriptorHeap[renderResources.highResTextureUAV];
    SamplerState linearSampler = SamplerDescriptorHeap[renderResources.samplerIndex];

    uint2 highResDim;
    highResTexture.GetDimensions(highResDim.x, highResDim.y);

    if (dispatchThreadID.x >= highResDim.x || dispatchThreadID.y >= highResDim.y)
        return;

    uint2 lowResDim;
    lowResTexture.GetDimensions(lowResDim.x, lowResDim.y);

    float2 texelSize = 1.0 / float2(lowResDim);
    float2 uv = (float2(dispatchThreadID.xy) + 0.5) / float2(highResDim);

    // 3x3 Tent Filter Sampling (Better method: 4 Bilinear sampling)
    float3 A = lowResTexture.SampleLevel(linearSampler, uv + float2(-0.5, -0.5) * texelSize, 0).rgb;
    float3 B = lowResTexture.SampleLevel(linearSampler, uv + float2( 0.5, -0.5) * texelSize, 0).rgb;
    float3 C = lowResTexture.SampleLevel(linearSampler, uv + float2(-0.5,  0.5) * texelSize, 0).rgb;
    float3 D = lowResTexture.SampleLevel(linearSampler, uv + float2( 0.5,  0.5) * texelSize, 0).rgb;

    float3 upsampledColor = (A + B + C + D) * 0.25;

    float3 highResColor = highResTexture[dispatchThreadID.xy].rgb;

    float3 result = lerp(highResColor, upsampledColor, 0.85);

    highResTexture[dispatchThreadID.xy] = float4(result, 1.0);
}
