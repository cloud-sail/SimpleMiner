#include "Common/Resources.hlsli"

struct FullScreenQuadResources
{
    uint textureIndex;
    uint samplerIndex;
};


//-----------------------------------------------------------------------------------------------
struct v2p_t
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

ConstantBuffer<FullScreenQuadResources> renderResources : register(b0);

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

float4 PixelMain(v2p_t input) : SV_Target0
{
    Texture2D<float4> tex = ResourceDescriptorHeap[renderResources.textureIndex];
	SamplerState samp = SamplerDescriptorHeap[renderResources.samplerIndex];

	float4 color = tex.Sample(samp, input.uv);
	clip(color.a - 0.01f);
	return float4(color);
}