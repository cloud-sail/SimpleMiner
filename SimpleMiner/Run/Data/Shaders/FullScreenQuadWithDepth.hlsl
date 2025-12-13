#include "Common/Resources.hlsli"

struct FullScreenQuadWithDepthResources
{
    uint textureIndex;
    uint depthTexIndex;
    uint samplerIndex;
};


//-----------------------------------------------------------------------------------------------
struct v2p_t
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

ConstantBuffer<FullScreenQuadWithDepthResources> renderResources : register(b0);

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
    v2p.pos = float4(ndc[indices[vertexID]], 0.0, 1.0);
    v2p.uv  = uv[indices[vertexID]];
    return v2p;
}

float4 PixelMain(v2p_t input, out float depth : SV_Depth) : SV_Target0
{
    Texture2D<float4> tex = ResourceDescriptorHeap[renderResources.textureIndex];
    Texture2D<float> depthTex = ResourceDescriptorHeap[renderResources.depthTexIndex];
	SamplerState samp = SamplerDescriptorHeap[renderResources.samplerIndex];

	float4 color = tex.Sample(samp, input.uv);
	clip(color.a - 0.01f);
    depth = depthTex.Sample(samp, input.uv);
	return float4(color);
}