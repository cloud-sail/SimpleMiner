//-----------------------------------------------------------------------------------------------
// BloomDownsample.hlsl
// 13-tap Downsample with Karis Average
// Bilinear sample

struct BloomDownsampleResources
{
    uint inputTextureSRV;
    uint outputTextureUAV;
    uint samplerIndex;
    uint useKarisAverage;  // 0 = false, 1 = true
};

// Suppress Flicker (only does it in the first downsampling)
float3 KarisAverage(float3 color)
{
    float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
    return color / (1.0 + luminance);
}

//-----------------------------------------------------------------------------------------------
ConstantBuffer<BloomDownsampleResources> renderResources : register(b0);

//-----------------------------------------------------------------------------------------------
// 13-tap Downsample Kernel
//
[numthreads(8, 8, 1)]
void ComputeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D<float4> inputTexture = ResourceDescriptorHeap[renderResources.inputTextureSRV];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[renderResources.outputTextureUAV];
    SamplerState linearSampler = SamplerDescriptorHeap[renderResources.samplerIndex];

    uint2 outputDim;
    outputTexture.GetDimensions(outputDim.x, outputDim.y);

    if (dispatchThreadID.x >= outputDim.x || dispatchThreadID.y >= outputDim.y)
        return;

    uint2 inputDim;
    inputTexture.GetDimensions(inputDim.x, inputDim.y);

    float2 texelSize = 1.0 / float2(inputDim);
    float2 uv = (float2(dispatchThreadID.xy) + 0.5) / float2(outputDim); // Center of the pixel

    // 13-tap sample offset
    float2 offset = texelSize;

    // 1st row (3 sample points)
    float3 A = inputTexture.SampleLevel(linearSampler, uv + float2(-2.0, -2.0) * offset, 0).rgb;
    float3 B = inputTexture.SampleLevel(linearSampler, uv + float2( 0.0, -2.0) * offset, 0).rgb;
    float3 C = inputTexture.SampleLevel(linearSampler, uv + float2( 2.0, -2.0) * offset, 0).rgb;

    // 2nd row (2 sample points)
    float3 D = inputTexture.SampleLevel(linearSampler, uv + float2(-1.0, -1.0) * offset, 0).rgb;
    float3 E = inputTexture.SampleLevel(linearSampler, uv + float2( 1.0, -1.0) * offset, 0).rgb;

    // 3rd row (3 sample points)
    float3 F = inputTexture.SampleLevel(linearSampler, uv + float2(-2.0,  0.0) * offset, 0).rgb;
    float3 G = inputTexture.SampleLevel(linearSampler, uv + float2( 0.0,  0.0) * offset, 0).rgb;
    float3 H = inputTexture.SampleLevel(linearSampler, uv + float2( 2.0,  0.0) * offset, 0).rgb;

    // 4th row (2 sample points)
    float3 I = inputTexture.SampleLevel(linearSampler, uv + float2(-1.0,  1.0) * offset, 0).rgb;
    float3 J = inputTexture.SampleLevel(linearSampler, uv + float2( 1.0,  1.0) * offset, 0).rgb;

    // 5th row (3 sample points)
    float3 K = inputTexture.SampleLevel(linearSampler, uv + float2(-2.0,  2.0) * offset, 0).rgb;
    float3 L = inputTexture.SampleLevel(linearSampler, uv + float2( 0.0,  2.0) * offset, 0).rgb;
    float3 M = inputTexture.SampleLevel(linearSampler, uv + float2( 2.0,  2.0) * offset, 0).rgb;

    // Apply Karis Average supress the flicker
    float3 groups[5];
    groups[0] = (D + E + I + J) * 0.25;           // center
    groups[1] = (A + B + G + F) * 0.25;           // up-left
    groups[2] = (B + C + H + G) * 0.25;           // up-right
    groups[3] = (F + G + L + K) * 0.25;           // bottom-left
    groups[4] = (G + H + M + L) * 0.25;           // bottop-right

    if (renderResources.useKarisAverage != 0)
    {
            groups[0] = KarisAverage(groups[0]);
            groups[1] = KarisAverage(groups[1]);
            groups[2] = KarisAverage(groups[2]);
            groups[3] = KarisAverage(groups[3]);
            groups[4] = KarisAverage(groups[4]);
    }

    // weighted average
    float3 result = groups[0] * 0.5;
    result += groups[1] * 0.125;
    result += groups[2] * 0.125;
    result += groups[3] * 0.125;
    result += groups[4] * 0.125;

    // output
    outputTexture[dispatchThreadID.xy] = float4(result, 1.0);
}