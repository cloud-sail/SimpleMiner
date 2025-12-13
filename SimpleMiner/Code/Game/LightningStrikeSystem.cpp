#include "Game/LightningStrikeSystem.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Core/Vertex_PCU.hpp"

LightningStrikeSystem::LightningStrikeSystem(Renderer* renderer)
	: m_renderer(renderer)
{
	// Setup default configuration
	m_defaultConfig.numGenerations = 6;
	m_defaultConfig.maxOffsetDistance = 8.5f;
	m_defaultConfig.branchProbability = 0.55f;

	m_defaultConfig.baseBranchLengthScale = 0.85f;
	m_defaultConfig.minBranchLengthScale = 0.3f;

	m_defaultConfig.lightningColor = Rgba8(252, 192, 30); // Yellow-orange

	m_defaultConfig.emissiveStrength = 3.5f;

	m_defaultConfig.jitterAmount = 0.f;

	m_lightningBoltShader = m_renderer->CreateOrGetShader(ShaderConfig("Data/Shaders/LightningBolt"), VertexType::VERTEX_PCU);
}

void LightningStrikeSystem::SpawnLightningStrike(Vec3 const& skyPosition, Vec3 const& groundPosition)
{
	m_lightningManager.CreateLightning(skyPosition, groundPosition, m_defaultConfig);
}

void LightningStrikeSystem::Update(float deltaSeconds)
{
	m_lightningManager.Update(deltaSeconds);
	m_lightningManager.DestroyCompletedLightning();
}

void LightningStrikeSystem::Render(float currentTime) const
{
	std::vector<Vertex_PCU> verts;
	m_lightningManager.Render(verts, currentTime);

	if (verts.empty())
		return;

	// Setup emissive rendering
	m_renderer->SetModelConstants();

	LightningBoltResources resources;
	resources.diffuseTextureIndex = m_renderer->GetSrvIndexFromLoadedTexture(nullptr, DefaultTexture::WhiteOpaque2D);
	resources.diffuseSamplerIndex = m_renderer->GetDefaultSamplerIndex(SamplerMode::POINT_CLAMP);
	resources.cameraConstantsIndex = m_renderer->GetCurrentCameraConstantsIndex();
	resources.modelConstantsIndex = m_renderer->GetCurrentModelConstantsIndex();
	resources.emissiveTextureIndex = m_renderer->GetSrvIndexFromLoadedTexture(nullptr, DefaultTexture::WhiteOpaque2D);
	resources.emissiveStrength = m_defaultConfig.emissiveStrength;

	m_renderer->SetGraphicsBindlessResources(sizeof(LightningBoltResources), &resources);
	m_renderer->BindShader(m_lightningBoltShader);
	m_renderer->SetBlendMode(BlendMode::ADDITIVE);  // #ToDo MAXIMUM Blend, the overlap part between segment
	m_renderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	m_renderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);

	m_renderer->DrawVertexArray(verts);
}
