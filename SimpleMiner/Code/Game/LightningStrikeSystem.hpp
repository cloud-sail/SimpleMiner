#pragma once
#include "Game/LightningBolt.hpp"

#include "Engine/Renderer/RendererCommon.hpp"

class Renderer;

//-----------------------------------------------------------------------------------------------
struct LightningBoltResources
{
	uint32_t diffuseTextureIndex = INVALID_INDEX_U32;
	uint32_t diffuseSamplerIndex = INVALID_INDEX_U32;

	uint32_t cameraConstantsIndex = INVALID_INDEX_U32;
	uint32_t modelConstantsIndex = INVALID_INDEX_U32;

	uint32_t emissiveTextureIndex = INVALID_INDEX_U32;
	float emissiveStrength = 1.0f;
};


//-----------------------------------------------------------------------------------------------
class LightningStrikeSystem
{
public:
	LightningStrikeSystem(Renderer* renderer);

	void SpawnLightningStrike(Vec3 const& skyPosition, Vec3 const& groundPosition);

	void Update(float deltaSeconds);

	void Render(float currentTime) const;

private:
	LightningManager m_lightningManager;
	LightningConfig m_defaultConfig;

	Renderer* m_renderer = nullptr;
	Shader* m_lightningBoltShader = nullptr;
};

