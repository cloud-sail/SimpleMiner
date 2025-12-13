#pragma once
#include "Engine/Renderer/RendererCommon.hpp"

// Example Mips: 0~4
// 
// Copy (Format Transform)
// emissiveSRV	-> MipUAV_0
// 
// Downsampling:
// MipUAV_0		-> MipUAV_1
// MipUAV_1		-> MipUAV_2
// MipUAV_2		-> MipUAV_3
// MipUAV_3		-> MipUAV_4
// 
// Upsampling
// MipUAV_3 + MipSRV_4 -> MipUAV_3
// MipUAV_2 + MipSRV_3 -> MipUAV_2
// MipUAV_1 + MipSRV_2 -> MipUAV_1
// MipUAV_0 + MipSRV_1 -> MipUAV_0
//


class BloomEffect
{
public:
	BloomEffect();
	~BloomEffect();

	void Initialize();

	// Rebuild texture resources when 
	void OnResize(unsigned int width, unsigned int height);

	// emissiveTextureSRV (highlight part)
	// + 
	// sceneTextureSRV (original scene texture)
	// = 
	// outputTextureUAV (composite result)
	// Notes: These textures are not managed by BloomEffect
	void Execute(DescriptorHandle emissiveTextureSRV,
		DescriptorHandle sceneTextureSRV,
		DescriptorHandle outputTextureUAV) const;


	void SetBloomIntensity(float intensity) { m_bloomIntensity = intensity; }
	float GetBloomIntensity() const { return m_bloomIntensity; }

	// Extracting High light part
	void SetBloomThreshold(float threshold) { m_bloomThreshold = threshold; }
	float GetBloomThreshold() const { return m_bloomThreshold; }

	unsigned int GetWidth() const { return m_width; }
	unsigned int GetHeight() const { return m_height; }

private:
	void ReleaseTextures();

	void CreateMipTextures();

	// Copy + Threshold Filter (Emissive -> Mip0)
	void ExecuteCopy(DescriptorHandle emissiveTextureSRV) const;

	// Downsample: Mip0 -> Mip1 -> ... -> Mip4
	void ExecuteDownsampleChain() const;

	// Upsample: Mip4 -> Mip3 -> ... -> Mip0
	void ExecuteUpsampleChain() const;

	// Final Composite: Scene + Mip0 -> Output
	void ExecuteComposite(DescriptorHandle sceneTextureSRV,
		DescriptorHandle outputTextureUAV) const;

private:
	unsigned int m_width = 0;
	unsigned int m_height = 0;

	float m_bloomIntensity = 1.0f; // When Composite
	float m_bloomThreshold = 1.0f; // Before downsample, Pixel with high luminance blooms 
	// float luminance = dot(color.rgb, float3(0.2126, 0.7152, 0.0722));
	// Or Directly render to the emissive texture  

	static constexpr int MIP_LEVELS = 5; // #ToDo not fixed mip, minimum > 64 px
	// 1080 >> 4 = 67.5


	Texture* m_mipTextures[MIP_LEVELS] = {};	// Mip0 ~ Mip4
	DescriptorHandle m_mipSRVs[MIP_LEVELS];		// SRV for reading
	DescriptorHandle m_mipUAVs[MIP_LEVELS];		// UAV for writing

	// Compute Shaders
	Shader* m_copyShader = nullptr;
	Shader* m_downsampleShader = nullptr;   // 13-tap downsample shader
	Shader* m_upsampleShader = nullptr;     // 3x3 tent filter upsample shader
	Shader* m_compositeShader = nullptr;    // Final composite shader

};

// BloomCopy.hlsl (COPY, Apply Threshold, Format Auto Transform)
struct BloomCopyResources
{
	uint32_t inputTextureSRV = INVALID_INDEX_U32;
	uint32_t outputTextureUAV = INVALID_INDEX_U32;

	// Params
	float bloomIntensity = 1.0f;
	float bloomThreshold = 1.0f;
};

// BloomDownsample.hlsl 
struct BloomDownsampleResources
{
	uint32_t inputTextureSRV = INVALID_INDEX_U32;
	uint32_t outputTextureUAV = INVALID_INDEX_U32;
	uint32_t samplerIndex = INVALID_INDEX_U32;
	uint32_t useKarisAverage = 0;  // 0 = false, 1 = true #ToDo MipLevel? MipLevel == 0 means need karis average
};

// BloomUpsample.hlsl
struct BloomUpsampleResources
{
	uint32_t lowResTextureSRV = INVALID_INDEX_U32;	// Read & Blur
	uint32_t highResTextureUAV = INVALID_INDEX_U32;	// Read & Write
	uint32_t samplerIndex = INVALID_INDEX_U32;
};

// BloomComposite.hlsl
struct BloomCompositeResources
{
	uint32_t sceneTextureSRV = INVALID_INDEX_U32;
	uint32_t bloomTextureSRV = INVALID_INDEX_U32;    // Mip0
	uint32_t outputTextureUAV = INVALID_INDEX_U32;
	uint32_t samplerIndex = INVALID_INDEX_U32;

	// Params
	float bloomIntensity = 1.0f;
	float bloomThreshold = 1.0f;
};