#include "Game/BloomEffect.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/Texture.hpp"
#include "Engine/Renderer/Shader.hpp"
#include "Engine/Renderer/Buffer.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Window/Window.hpp"


BloomEffect::BloomEffect()
{
}

BloomEffect::~BloomEffect()
{
	ReleaseTextures();
}

void BloomEffect::Initialize()
{
	ShaderConfig copyConfig;
	copyConfig.m_name = "Data/Shaders/BloomCopy";
	copyConfig.m_stages = SHADER_STAGE_CS;
	m_copyShader = g_theRenderer->CreateOrGetShader(copyConfig, VertexType::VERTEX_NONE);


	ShaderConfig downsampleConfig;
	downsampleConfig.m_name = "Data/Shaders/BloomDownsample";
	downsampleConfig.m_stages = SHADER_STAGE_CS;
	m_downsampleShader = g_theRenderer->CreateOrGetShader(downsampleConfig, VertexType::VERTEX_NONE);

	ShaderConfig upsampleConfig;
	upsampleConfig.m_name = "Data/Shaders/BloomUpsample";
	upsampleConfig.m_stages = SHADER_STAGE_CS;
	m_upsampleShader = g_theRenderer->CreateOrGetShader(upsampleConfig, VertexType::VERTEX_NONE);

	ShaderConfig compositeConfig;
	compositeConfig.m_name = "Data/Shaders/BloomComposite";
	compositeConfig.m_stages = SHADER_STAGE_CS;
	m_compositeShader = g_theRenderer->CreateOrGetShader(compositeConfig, VertexType::VERTEX_NONE);

	IntVec2 clientDimensions = Window::s_mainWindow->GetClientDimensions();
	OnResize(static_cast<unsigned int>(clientDimensions.x), static_cast<unsigned int>(clientDimensions.y));
}

void BloomEffect::OnResize(unsigned int width, unsigned int height)
{
	if (width == 0 || height == 0)
		return;

	if (m_width == width && m_height == height && m_mipTextures[0] != nullptr)
		return;

	m_width = width;
	m_height = height;

	ReleaseTextures();

	CreateMipTextures();
}

void BloomEffect::Execute(DescriptorHandle emissiveTextureSRV, DescriptorHandle sceneTextureSRV, DescriptorHandle outputTextureUAV) const
{
	ExecuteCopy(emissiveTextureSRV);

	ExecuteDownsampleChain();

	ExecuteUpsampleChain();

	ExecuteComposite(sceneTextureSRV, outputTextureUAV);
}

void BloomEffect::ReleaseTextures()
{
	for (int i = 0; i < MIP_LEVELS; ++i)
	{
		g_theRenderer->DestroyTexture(m_mipTextures[i]);
		g_theRenderer->EnqueueDeferredRelease(m_mipSRVs[i]);
		g_theRenderer->EnqueueDeferredRelease(m_mipUAVs[i]);
	}
}

void BloomEffect::CreateMipTextures()
{
	unsigned int currentWidth = m_width;
	unsigned int currentHeight = m_height;

	for (int i = 0; i < MIP_LEVELS; ++i)
	{
		if (i > 0)
		{
			currentWidth = currentWidth / 2;
			currentHeight = currentHeight / 2;
		}

		currentWidth = (currentWidth > 0) ? currentWidth : 1;
		currentHeight = (currentHeight > 0) ? currentHeight : 1;

		TextureInit textureInit;
		textureInit.m_width = currentWidth;
		textureInit.m_height = currentHeight;
		textureInit.m_format = DXGI_FORMAT_R16G16B16A16_FLOAT; // HDR Format
		textureInit.m_allowSRV = true;
		textureInit.m_allowUAV = true;
		textureInit.m_debugName = L"Bloom Mip Texture";

		m_mipTextures[i] = g_theRenderer->CreateTexture(textureInit);

		m_mipSRVs[i] = g_theRenderer->AllocateSRV(*m_mipTextures[i]);
		m_mipUAVs[i] = g_theRenderer->AllocateUAV(*m_mipTextures[i]);
	}
}

void BloomEffect::ExecuteCopy(DescriptorHandle emissiveTextureSRV) const
{
	// Transition state
	g_theRenderer->TransitionToUnorderedAccess(*m_mipTextures[0]);

	// Set Resources
	BloomCopyResources resources;
	resources.inputTextureSRV = emissiveTextureSRV.m_index;
	resources.outputTextureUAV = m_mipUAVs[0].m_index;
	resources.bloomIntensity = m_bloomIntensity;
	resources.bloomThreshold = m_bloomThreshold;

	g_theRenderer->SetComputeBindlessResources(sizeof(BloomCopyResources), &resources);

	// Dispatch
	g_theRenderer->BindComputeShader(m_copyShader);
	g_theRenderer->Dispatch2D(m_width, m_height, 8, 8);

	// UAV Barrier
	g_theRenderer->AddUAVBarrier(*m_mipTextures[0]);  // Not necessary, Only use this when use UAV states consecutively (no state change)
	g_theRenderer->TransitionToAllShaderResource(*m_mipTextures[0]);
}

void BloomEffect::ExecuteDownsampleChain() const
{
	// Default Sampler
	unsigned int samplerIndex = g_theRenderer->GetDefaultSamplerIndex(SamplerMode::BILINEAR_CLAMP);

	// Step 1~4: Downsample Chain (Mip0 -> Mip1 -> Mip2 -> Mip3 -> Mip4)
	for (int i = 1; i < MIP_LEVELS; ++i)
	{
		unsigned int dstWidth = m_width >> i;
		unsigned int dstHeight = m_height >> i;

		// Transition state
		g_theRenderer->TransitionToUnorderedAccess(*m_mipTextures[i]);

		// Set Resources
		BloomDownsampleResources resources;
		resources.inputTextureSRV = m_mipSRVs[i - 1].m_index;
		resources.outputTextureUAV = m_mipUAVs[i].m_index;
		resources.samplerIndex = samplerIndex;
		resources.useKarisAverage = (i == 1) ? 1 : 0;

		g_theRenderer->SetComputeBindlessResources(sizeof(BloomDownsampleResources), &resources);

		// Dispatch
		g_theRenderer->BindComputeShader(m_downsampleShader);
		g_theRenderer->Dispatch2D(dstWidth, dstHeight, 8, 8);

		// UAV Barrier
		g_theRenderer->AddUAVBarrier(*m_mipTextures[i]);  // Not necessary, Only use this when use UAV states consecutively (no state change)
		g_theRenderer->TransitionToAllShaderResource(*m_mipTextures[i]);
	}
}

void BloomEffect::ExecuteUpsampleChain() const
{
	// Default Sampler
	unsigned int samplerIndex = g_theRenderer->GetDefaultSamplerIndex(SamplerMode::BILINEAR_CLAMP);

	// Upsample: Mip4 -> Mip3 -> Mip2 -> Mip1 -> Mip0
	for (int i = MIP_LEVELS - 2; i >= 0; --i)
	{
		unsigned int dstWidth = m_width >> i;
		unsigned int dstHeight = m_height >> i;

		// Transition state
		g_theRenderer->TransitionToUnorderedAccess(*m_mipTextures[i]);

		// Set Resources
		BloomUpsampleResources resources;
		resources.lowResTextureSRV = m_mipSRVs[i + 1].m_index;
		resources.highResTextureUAV = m_mipUAVs[i].m_index;
		resources.samplerIndex = samplerIndex;

		g_theRenderer->SetComputeBindlessResources(sizeof(BloomUpsampleResources), &resources);

		// Dispatch
		g_theRenderer->BindComputeShader(m_upsampleShader);
		g_theRenderer->Dispatch2D(dstWidth, dstHeight, 8, 8);

		// UAV Barrier
		g_theRenderer->AddUAVBarrier(*m_mipTextures[i]); // Not necessary, Only use this when use UAV states consecutively (no state change)
		g_theRenderer->TransitionToAllShaderResource(*m_mipTextures[i]);
	}
}

void BloomEffect::ExecuteComposite(DescriptorHandle sceneTextureSRV, DescriptorHandle outputTextureUAV) const
{
	unsigned int samplerIndex = g_theRenderer->GetDefaultSamplerIndex(SamplerMode::BILINEAR_CLAMP);

	BloomCompositeResources resources;
	resources.sceneTextureSRV = sceneTextureSRV.m_index;
	resources.bloomTextureSRV = m_mipSRVs[0].m_index;
	resources.outputTextureUAV = outputTextureUAV.m_index;
	resources.samplerIndex = samplerIndex;
	resources.bloomIntensity = m_bloomIntensity;
	resources.bloomThreshold = m_bloomThreshold;

	g_theRenderer->SetComputeBindlessResources(sizeof(BloomCompositeResources), &resources);

	// Dispatch
	g_theRenderer->BindComputeShader(m_compositeShader);
	g_theRenderer->Dispatch2D(m_width, m_height, 8, 8);
}
