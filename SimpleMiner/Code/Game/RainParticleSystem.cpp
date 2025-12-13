#include "Game/RainParticleSystem.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Math/AABB3.hpp"
#include <random>

RainParticleSystem::RainParticleSystem()
{

}

RainParticleSystem::~RainParticleSystem()
{
	Shutdown();
}

void RainParticleSystem::Initialize(unsigned int numParticles /*= 16000*/)
{
	if (m_isInitialized)
	{
		Shutdown();
	}
	 
	m_numParticles = numParticles;

	// Create shaders
	ShaderConfig updateConfig;
	updateConfig.m_name = "Data/Shaders/RainUpdate";
	updateConfig.m_stages = SHADER_STAGE_CS;
	m_updateShader = g_theRenderer->CreateOrGetShader(updateConfig, VertexType::VERTEX_NONE);

	ShaderConfig cullConfig;
	cullConfig.m_name = "Data/Shaders/RainCull";
	cullConfig.m_stages = SHADER_STAGE_CS;
	m_cullShader = g_theRenderer->CreateOrGetShader(cullConfig, VertexType::VERTEX_NONE);

	ShaderConfig renderConfig;
	renderConfig.m_name = "Data/Shaders/RainRender";
	m_renderShader = g_theRenderer->CreateOrGetShader(renderConfig, VertexType::VERTEX_NONE);

	// Initialize particles on CPU
	InitializeParticles();

	// Create GPU buffers
	CreateBuffers();

	// Create depth textures for culling
	CreateDepthTextures();

	m_isInitialized = true;
}

void RainParticleSystem::Shutdown()
{
	if (!m_isInitialized)
		return;

	DestroyDepthTextures();
	DestroyBuffers();

	m_particlesCPU.clear();
	m_isInitialized = false;
}

void RainParticleSystem::Update(Vec3 const& playerPosition, float deltaSeconds)
{
	if (!m_isInitialized)
		return;

	m_tempRainConstantsIndex = INVALID_INDEX_U32;

	Vec3 currentBoxCenter = playerPosition + m_boxCenterOffset;

	// Calculate delta (how much the box moved this frame)
	Vec3 boxCenterDelta = currentBoxCenter - m_previousBoxCenter;
	m_previousBoxCenter = currentBoxCenter;

	RainConstants constants;
	constants.rainColor[0] = m_rainColor[0];
	constants.rainColor[1] = m_rainColor[1];
	constants.rainColor[2] = m_rainColor[2];
	constants.rainColor[3] = m_rainColor[3];
	constants.particleSize = m_particleSize;
	constants.numParticles = m_numParticles;
	constants.velocity = m_velocity;
	constants.boxCenter = currentBoxCenter;
	constants.boxExtents = m_boxExtents;
	constants.boxCenterDelta = boxCenterDelta;
	constants.deltaTime = deltaSeconds;

	// Update Camera for depth map
	m_depthCamera = CreateOrthoCameraForAABB3(AABB3(currentBoxCenter - m_boxExtents, currentBoxCenter + m_boxExtents), m_velocity);

	Mat44 worldToClipTransform;

	worldToClipTransform.Append(m_depthCamera.GetRenderToClipTransform());
	worldToClipTransform.Append(m_depthCamera.GetCameraToRenderTransform());
	worldToClipTransform.Append(m_depthCamera.GetWorldToCameraTransform());

	constants.worldToClipTransform = worldToClipTransform;

	m_tempRainConstantsIndex = g_theRenderer->AllocateTempConstantBuffer(sizeof(RainConstants), &constants);

	//-----------------------------------------------------------------------------------------------
	// Update Particles
	// Transition buffer for compute shader
	g_theRenderer->TransitionToUnorderedAccess(*m_particleBuffer);

	// Set resources
	RainUpdateResources resources;
	resources.particleBufferIndex = m_particleBufferUAV.m_index;
	resources.rainConstantsIndex = m_tempRainConstantsIndex;

	g_theRenderer->SetComputeBindlessResources(sizeof(RainUpdateResources), &resources);

	// Dispatch compute shader
	g_theRenderer->BindComputeShader(m_updateShader);
	g_theRenderer->Dispatch1D(m_numParticles, 256);
	// Barrier
	g_theRenderer->AddUAVBarrier(*m_particleBuffer);  // Not necessary, Only use this when use UAV states consecutively (no state change)
}

void RainParticleSystem::Render() const
{
	if (!m_isInitialized)
		return;

	//-----------------------------------------------------------------------------------------------
	// Culling
	{
		// Transition resources
		g_theRenderer->TransitionToGenericRead(*m_particleBuffer);
		g_theRenderer->TransitionToUnorderedAccess(*m_visibilityBuffer);
		g_theRenderer->TransitionToAllShaderResource(*m_depthTexture);

		RainCullResources resources;

		resources.particleBufferIndex = m_particleBufferSRV.m_index;
		resources.visibilityBufferIndex = m_visibilityBufferUAV.m_index;
		resources.depthTextureIndex = m_depthSRV.m_index;
		resources.samplerIndex = g_theRenderer->GetDefaultSamplerIndex(SamplerMode::POINT_CLAMP); 
		// Do not use Bilinear: Depth is not linear, Percentage Closer Filtering (PCF)
		// For Rain Culling: no need to use this method
		// https://developer.nvidia.com/gpugems/gpugems/part-ii-lighting-and-shadows/chapter-11-shadow-map-antialiasing
		resources.rainConstantsIndex = m_tempRainConstantsIndex;

		g_theRenderer->SetComputeBindlessResources(sizeof(RainCullResources), &resources);
		g_theRenderer->BindComputeShader(m_cullShader);
		g_theRenderer->Dispatch1D(m_numParticles, 256);
		// Barrier
		g_theRenderer->AddUAVBarrier(*m_visibilityBuffer); // Not necessary, Only use this when use UAV states consecutively (no state change)
		// UAV Barriers are only needed between consecutive UAV accesses.


	}


	//-----------------------------------------------------------------------------------------------
	// Rendering
	{
		g_theRenderer->TransitionToGenericRead(*m_particleBuffer);
		g_theRenderer->TransitionToGenericRead(*m_visibilityBuffer);

		RainRenderResources resources;
		resources.particleBufferIndex = m_particleBufferSRV.m_index;
		resources.visibilityBufferIndex = m_visibilityBufferSRV.m_index;
		resources.cameraConstantsIndex = g_theRenderer->GetCurrentCameraConstantsIndex();
		resources.rainConstantsIndex = m_tempRainConstantsIndex;

		g_theRenderer->SetGraphicsBindlessResources(sizeof(RainRenderResources), &resources);

		// Set render states
		g_theRenderer->BindShader(m_renderShader);
		g_theRenderer->SetBlendMode(BlendMode::ALPHA);  // Alpha blending for rain
		g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);  // No culling for billboards
		g_theRenderer->SetDepthMode(DepthMode::READ_ONLY_LESS_EQUAL);

		g_theRenderer->DrawProceduralInstanced(6, m_numParticles);
	}
	

}

void RainParticleSystem::PrepareDepthPrePass() const
{
	if (!m_isInitialized)
		return;

	g_theRenderer->TransitionToDepthWrite(*m_depthTexture);
	g_theRenderer->ClearDepthAndStencilByIndex(m_depthDSV.m_index, 1.f);
	g_theRenderer->SetRenderTargetFormats({}, DXGI_FORMAT_D32_FLOAT); // #ToDo if there is a warning, no rt set nullptr
	g_theRenderer->SetRenderTargetsByIndex({}, m_depthDSV.m_index);
}

void RainParticleSystem::InitializeParticles()
{
	m_particlesCPU.clear();
	m_particlesCPU.reserve(m_numParticles);

	Vec3 relativeMins = -m_boxExtents;
	Vec3 relativeMaxs = m_boxExtents;

	// #ToDo find a better distribution
	std::random_device rd;
	std::mt19937 gen(rd());

	std::uniform_real_distribution<float> distX(relativeMins.x, relativeMaxs.x);
	std::uniform_real_distribution<float> distY(relativeMins.y, relativeMaxs.y);
	std::uniform_real_distribution<float> distZ(relativeMins.z, relativeMaxs.z);

	for (int i = 0; i < (int)m_numParticles; ++i) 
	{
		RainParticle particle;

		particle.relativePosition = Vec3(distX(gen), distY(gen), distZ(gen));

		m_particlesCPU.push_back(particle);
	}

}

void RainParticleSystem::CreateBuffers()
{
	{
		BufferInit bufferInit;
		bufferInit.m_size = m_numParticles * sizeof(RainParticle);
		bufferInit.m_allowUnorderedAccess = true;
		m_particleBuffer = g_theRenderer->CreateBuffer(bufferInit);

		// Upload initial particle data
		g_theRenderer->UpdateBuffer(*m_particleBuffer, bufferInit.m_size, m_particlesCPU.data());

		// Allocate descriptors
		m_particleBufferSRV = g_theRenderer->AllocateStructuredBufferSRV(*m_particleBuffer, sizeof(RainParticle), m_numParticles);
		m_particleBufferUAV = g_theRenderer->AllocateStructuredBufferUAV(*m_particleBuffer, sizeof(RainParticle), m_numParticles);
	}

	{
		// Create visibility buffer (one uint per particle)
		BufferInit visibilityInit;
		visibilityInit.m_size = m_numParticles * sizeof(uint32_t);
		visibilityInit.m_allowUnorderedAccess = true;
		m_visibilityBuffer = g_theRenderer->CreateBuffer(visibilityInit);

		// Initialize all particles as visible
		std::vector<uint32_t> initialVisibility(m_numParticles, 1);
		g_theRenderer->UpdateBuffer(*m_visibilityBuffer, visibilityInit.m_size, initialVisibility.data());

		m_visibilityBufferSRV = g_theRenderer->AllocateStructuredBufferSRV(*m_visibilityBuffer, sizeof(uint32_t), m_numParticles);
		m_visibilityBufferUAV = g_theRenderer->AllocateStructuredBufferUAV(*m_visibilityBuffer, sizeof(uint32_t), m_numParticles);
	}

	//{
	//	// Create constant buffer for rain constants
	//	BufferInit constantInit;
	//	constantInit.m_isConstantBuffer = true;
	//	constantInit.m_size = sizeof(RainConstants);
	//	m_constantBuffer = g_theRenderer->CreateBuffer(constantInit);

	//	m_constantBufferCBV = g_theRenderer->AllocateConstantBufferView(*m_constantBuffer);
	//}

	//{
	//	// Create constant buffer for cull constants
	//	BufferInit cullConstantInit;
	//	cullConstantInit.m_isConstantBuffer = true;
	//	cullConstantInit.m_size = sizeof(RainCullConstants);
	//	m_cullConstantBuffer = g_theRenderer->CreateBuffer(cullConstantInit);

	//	m_cullConstantBufferCBV = g_theRenderer->AllocateConstantBufferView(*m_cullConstantBuffer);
	//}

}

void RainParticleSystem::DestroyBuffers()
{
	g_theRenderer->DestroyBuffer(m_particleBuffer);
	g_theRenderer->EnqueueDeferredRelease(m_particleBufferSRV);
	g_theRenderer->EnqueueDeferredRelease(m_particleBufferUAV);

	g_theRenderer->DestroyBuffer(m_visibilityBuffer);
	g_theRenderer->EnqueueDeferredRelease(m_visibilityBufferSRV);
	g_theRenderer->EnqueueDeferredRelease(m_visibilityBufferUAV);

	//g_theRenderer->DestroyBuffer(m_constantBuffer);
	//g_theRenderer->EnqueueDeferredRelease(m_constantBufferCBV);

	//g_theRenderer->DestroyBuffer(m_cullConstantBuffer);
	//g_theRenderer->EnqueueDeferredRelease(m_cullConstantBufferCBV);
}

void RainParticleSystem::CreateDepthTextures()
{
	// Create depth texture for top-down view
	TextureInit depthInit;
	depthInit.m_width = m_depthMapResolution;
	depthInit.m_height = m_depthMapResolution;
	depthInit.m_format = DXGI_FORMAT_R32_TYPELESS;
	depthInit.m_allowDSV = true;
	depthInit.m_allowSRV = true;
	depthInit.m_debugName = L"Rain Top-Down Depth";
	depthInit.m_dsvClearFormat = DXGI_FORMAT_D32_FLOAT;

	m_depthTexture = g_theRenderer->CreateTexture(depthInit);

	m_depthDSV = g_theRenderer->AllocateDSV(*m_depthTexture, DXGI_FORMAT_D32_FLOAT);
	m_depthSRV = g_theRenderer->AllocateSRV(*m_depthTexture, DXGI_FORMAT_R32_FLOAT);
}

void RainParticleSystem::DestroyDepthTextures()
{
	g_theRenderer->DestroyTexture(m_depthTexture);
	g_theRenderer->EnqueueDeferredRelease(m_depthDSV);
	g_theRenderer->EnqueueDeferredRelease(m_depthSRV);
}
