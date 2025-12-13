#pragma once
#include "Game/BlockIterator.hpp"
#include "Game/GameCommon.hpp"

#include "Engine/Core/HashUtils.hpp"
#include "Engine/Core/JobSystem.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/Gradient.hpp"
#include "Engine/Renderer/RendererCommon.hpp"
#include "Engine/Renderer/Camera.hpp"
#include <vector>
#include <unordered_map>
#include <queue>

struct Vec2;
class Chunk;
class ChunkFadeOutManager;
class BlockIterator;
class Buffer;
class Clock;
class Game;
class GameCamera;
class Entity;
class LightningStrikeSystem;
class BloomEffect;
class RainParticleSystem;

//-----------------------------------------------------------------------------------------------
#pragma region Job
class GenerateChunkJob : public Job
{
public:
	GenerateChunkJob(Chunk* chunk)
		: m_chunk(chunk)
		, Job(JobType::GENERIC, JobPriority::NORMAL)
	{}

	void Execute() override;

public:
	Chunk* m_chunk = nullptr;
};

class LoadChunkJob : public Job 
{
public:
	LoadChunkJob(Chunk* chunk)
		: m_chunk(chunk)
		, Job(JobType::FILE_IO, JobPriority::NORMAL)
	{
	}

	void Execute() override;

public:
	Chunk* m_chunk = nullptr;
};


class SaveChunkJob : public Job
{
public:
	SaveChunkJob(Chunk* chunk)
		: m_chunk(chunk)
		, Job(JobType::FILE_IO, JobPriority::CRITICAL)
	{
	}

	void Execute() override;

public:
	Chunk* m_chunk = nullptr;
};
#pragma endregion

//-----------------------------------------------------------------------------------------------
struct WorldConstants
{
	float m_indoorLightColor[4];
	float m_outdoorLightColor[4];
	float m_skyColor[4]; // Fog strength, only alpha is being used
	
	float m_fogNearDistance;
	float m_fogFarDistance;

	float m_waterSurfaceHeight; // 64.f
	float m_isUnderwater;		// 0 is not in water, 1 is camera in water block

	float m_waterScatterColorShallow[4];
	float m_waterScatterColorDeep[4];

	float m_waterAbsorptionR;
	float m_waterAbsorptionG;
	float m_waterAbsorptionB;
	float m_waterVisibilityRange; // (blocks)

	float m_waterDepthTransitionStart; // (blocks)
	float m_waterDepthTransitionEnd;   // (blocks)
	float m_padding[2];

	Vec3 m_lightDirection;
	float m_lightningIntensity;

	WorldConstants();
};

void RenderWorldConstantsUI(WorldConstants& worldConstants);

//-----------------------------------------------------------------------------------------------
struct BlockRaycastResult3D
{
	Vec3	m_rayStartPos;
	Vec3	m_rayFwdNormal;
	float	m_rayLength = 1.f;

	bool	m_didImpact = false;
	float	m_impactDist = 0.0f;
	Vec3	m_impactPos;
	Vec3	m_impactNormal;

	BlockFace m_impactFace = BlockFace::EAST;
	BlockIterator m_impactedBlockIter;
	BlockIterator m_previousBlockIter;
};

//-----------------------------------------------------------------------------------------------
struct BlockDebugInfo
{
	

	int m_indoorLight;
	int m_outdoorLight;

	uint8_t m_type;
	
	bool m_isValid = false;
};


//-----------------------------------------------------------------------------------------------
class World
{
public:
	~World();
	World(Game* game);

	void Update();
	void Render() const;

	void OnWindowResized();

private:
	void RenderDepthPrePass() const;
	void RenderSkyPass() const;
	void RenderOpaquePass() const;
	void RenderEmissivePass() const;
	void RenderRainParticles() const;
	void RenderBloomPass() const;
	void CopyFinalToBackBuffer() const;

private:
	void CreateHDRRenderTargets();
	void ReleaseHDRRenderTargets();

public:
	Shader* GetDepthOnlyShader() const { return m_depthOnlyShader; }
private:
	BloomEffect* m_bloomEffect = nullptr;
	bool m_enableBloom = true;
	RainParticleSystem* m_rainSystem = nullptr;
	bool m_enableRain = true;

	Texture* m_emissiveRT = nullptr;        // Emissive Only (HDR)
	Texture* m_skyQuadRT = nullptr;			// Full Screen Quad

	Texture* m_sceneRT = nullptr;	// World RenderTarget (HDR)
	Texture* m_finalRT = nullptr;	// PostProcess Result (HDR) Ping Pong: If has multiple postprocessing, sceneRT<->finalRT
	// Finally Draw Full Screen Quad to back buffer(Blit)
	// Two Render target has different blend op IndependentBlendEnable
	// Or Add a new Blend Max, or both using Opaque? For Lightning it is ok

	DXGI_FORMAT const m_hdrFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

	DescriptorHandle m_emissiveRTV;
	DescriptorHandle m_emissiveSRV;

	DescriptorHandle m_skyQuadRTV;
	DescriptorHandle m_skyQuadSRV;

	DescriptorHandle m_sceneRTV;
	DescriptorHandle m_sceneSRV;


	DescriptorHandle m_finalSRV;
	DescriptorHandle m_finalUAV;

	Texture* m_sceneDepthBuffer = nullptr;
	DescriptorHandle m_sceneDepthDSV;
	//DXGI_FORMAT const m_sceneDepthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DXGI_FORMAT const m_sceneDepthFormat = DXGI_FORMAT_D32_FLOAT;

	Shader* m_copyToBackBufferShader = nullptr;
	Shader* m_depthOnlyShader = nullptr;
	Shader* m_skyShader = nullptr;

	// Also in game common
	unsigned int m_windowWidth = 0;
	unsigned int m_windowHeight = 0;



public:
	int GetActiveChunkCount() const { return static_cast<int>(m_activeChunks.size()); }
	int GetTotalVertexNum() const { return m_totalNumVertices; }
	int GetTotalIndexNum() const { return m_totalNumIndices; }

	int GetGeneratingChunkCount() const { return static_cast<int>(m_generatingChunks.size()); }
	int GetLoadingChunkCount() const { return static_cast<int>(m_loadingChunks.size()); }
	int GetSavingChunkCount() const { return static_cast<int>(m_savingChunks.size()); }

private:
	bool IsChunkActive(IntVec2 const& coords) const;
	Chunk* GetActiveChunk(IntVec2 const& coords) const;
	BlockIterator GetBlockIterFromGlobalCoords(IntVec3 const& globalCoords) const;

	// Only Hook/Unhook when active
	void HookupNeighbors(Chunk* chunk);
	void UnhookNeighbors(Chunk* chunk);

	static bool IsWithinActivationRange(const Vec2& playerXY, const IntVec2& coords);
	static bool IsBeyondDeactivationRange(const Vec2& playerXY, const IntVec2& coords);

	bool HasRunningJobInChunk(IntVec2 const& coords) const;

private:
	// Destructor
	void FlushJobSystemAndRetrieveJobs();
	void CleanupPendingChunks();
	void SaveAndCleanupActiveChunks();

	// Update
	void ResetPerFrameData();
	void ProcessCompletedJobs();
	void ActivateNearbyChunks(const Vec2& playerPositionXY, const IntVec2& playerChunkCoords);
	void DeactivateFarChunks(const Vec2& playerPositionXY);
	void RegenerateDirtyChunkMeshes(const Vec2& playerPositionXY);
	void UpdateAllActiveChunks();


private:
	std::unordered_map<IntVec2, Chunk*> m_activeChunks;

	// Is Running with jobs
	std::unordered_map<IntVec2, Chunk*> m_generatingChunks;
	std::unordered_map<IntVec2, Chunk*> m_loadingChunks;
	std::unordered_map<IntVec2, Chunk*> m_savingChunks;

	int m_totalNumVertices = 0;
	int m_totalNumIndices = 0;

private:
	void HandleDiggingAndPlacing();

	void DigBlock(BlockIterator iter, uint8_t airType = 0);
	void PlaceBlock(BlockIterator iter, uint8_t newType);


	void ChoosePlacedBlock();
	void DigOneNonAirBlockAtOrUnderPlayer(Chunk* chunk, IntVec3 const& playerLocalCoords);
	void PlaceOneBlockAboveNonAirBlockUnderPlayer(Chunk* chunk, IntVec3 const& playerLocalCoords);

public:
	uint8_t m_blockToBePlaced = 0;
	std::vector<std::string> m_blockLists = { "Glowstone", "Cobblestone", "ChiseledBrick" };

private:
	ChunkFadeOutManager* m_fadeOutManager = nullptr;

public:
	void MarkLightingDirty(BlockIterator const& iter);
	void MarkLightingDirtyIfNotOpaque(BlockIterator const& iter);
	
	void UndirtyAllBlocksInChunk(Chunk* chunk);

private:
	void ProcessDirtyLighting(); // process the whole dirty lighting queue
	void ProcessNextDirtyLightBlock();

private:
	std::queue<BlockIterator> m_dirtyBlockLighting;

public:
	uint32_t GetWorldConstantsIndex() const { return m_worldConstantBufferCBV.m_index; }
	uint32_t GetSkyQuadSRVIndex() const { return m_skyQuadSRV.m_index; }

private:
	void UpdateWorldConstantBuffer();
	void CreateWorldConstantBuffer();
	void DestroyWorldConstantBuffer();

private:
	void ShowImGuiWindow();

private:
	bool m_isSimulating = true;
	bool m_hasLightning = true;
	bool m_showRainParticles = true;
private:
	WorldConstants m_worldConstants;

	Buffer* m_worldConstantsBuffer = nullptr;
	DescriptorHandle m_worldConstantBufferCBV;

private:
	void UpdateDayNightSystem();

private:
	Clock* m_worldClock = nullptr;
	Game* m_game = nullptr;
	Gradient m_outDoorLightGradient;


public:
	BlockDebugInfo GetSelectedBlockInfo() const;
public:
	BlockRaycastResult3D FastVoxelRaycast(Vec3 rayStart, Vec3 rayForwardNormal, float rayLength) const;

	BlockRaycastResult3D RaycastSolid(Vec3 rayStart, Vec3 rayForwardNormal, float rayLength) const;


private:
	void UpdateRayAndDoRaycast();


private:
	BlockRaycastResult3D m_raycastResult;
	bool m_isRaycastLocked = false;
	Vec3 m_rayStart;
	Vec3 m_rayFwdNormal;
	float m_rayLength = RAY_LENGTH;

private:
	float m_fixedTimeStep = 0.02f;
	float m_owedPhysicsSeconds = 0.f;

public:
	Camera GetWorldCamera() const;
	bool IsPlayerCameraLocked() const { return m_isPlayerCameraLocked; }
	std::string GetCameraModeName() const;
	std::string GetPlayerPhysicsModeName() const;

	void RefreshAspectRatio();

	Vec3 GetPlayerCameraPosition() const;
	Rgba8 GetSkyColor() const;

private:
	Frustum GetPlayerCameraFrustum() const;
	EulerAngles GetPlayerCameraOrientation() const;

private:
	// Test Frustum Culling, Auto Switch to SpectatorXY Mode
	Camera m_lockedPlayerCamera;
	bool m_isPlayerCameraLocked = false;

private:
	GameCamera* m_gameCamera = nullptr;
	Entity* m_player = nullptr;

private:
	void RenderEmissiveShapes() const;

private:
	void SpawnOneLightningStrike();

private:
	LightningStrikeSystem* m_lightningStrikeSystem = nullptr;
	static constexpr float LIGHTNING_COOLDOWN_SECONDS = 0.2f;
	float m_lastLightningSeconds = -1.f;
};


#pragma region SingleThreadMethods
/* 
Vec3 playerPos = g_theGame->GetPlayerCameraPosition();
Vec2 playerXY = Vec2(playerPos.x, playerPos.y);

bool didWorkThisFrame = false;

if (!didWorkThisFrame)
{
	didWorkThisFrame = TryRegenerateNearestDirtyChunk(playerXY);
}

if (!didWorkThisFrame && GetActiveChunkCount() < MAX_ACTIVE_CHUNKS)
{
	didWorkThisFrame = TryActivateNearestMissingChunkWithInRange(playerXY);
}

if (!didWorkThisFrame)
{
	didWorkThisFrame = TryDeactivateFarthestChunkOutOfRange(playerXY);
}*/

/*
// Once per frame tasks
bool TryRegenerateNearestDirtyChunk(Vec2 const& playerXY);
bool TryActivateNearestMissingChunkWithInRange(Vec2 const& playerXY);
bool TryDeactivateFarthestChunkOutOfRange(Vec2 const& playerXY);
void DeactivateChunk(Chunk* chunk);
Chunk* ActivateChunkAt(IntVec2 const& coords); // create new chunk at coords

*/

 
/*
void World::DeactivateChunk(Chunk* chunk)
{
	if (!chunk) return;

	UnhookNeighbors(chunk);

	m_activeChunks.erase(chunk->GetChunkCoords());

	// Save if needed
	// #ToDo Put it in Destructor?
	if (chunk->NeedsSaving())
	{
		chunk->SaveToDiskIfNeeded();
	}

	// Chunk destructor: Remember to delete vb ib
	delete chunk;
}

Chunk* World::ActivateChunkAt(IntVec2 const& coords)
{
	// Construct chunk, populate blocks (load or generate), mark mesh dirty and not needing save
	// Do not generate mesh in constructor
	Chunk* chunk = new Chunk(this, coords);

	m_activeChunks.emplace(coords, chunk);

	HookupNeighbors(chunk);

	return chunk;
}

bool World::TryRegenerateNearestDirtyChunk(Vec2 const& playerXY)
{
	float bestDist = std::numeric_limits<float>::max();
	Chunk* bestChunk = nullptr;

	for (auto& kv : m_activeChunks)
	{
		Chunk* chunk = kv.second;
		if (chunk && chunk->IsMeshDirty())
		{
			Vec2 const center = GetChunkCenter(chunk->GetChunkCoords());
			float const dist = GetDistance2D(playerXY, center);
			if (dist < bestDist)
			{
				bestDist = dist;
				bestChunk = chunk;
			}
		}
	}

	if (bestChunk)
	{
		bestChunk->RegenerateMeshIfDirty();
		return true;
	}
	return false;

}

bool World::TryActivateNearestMissingChunkWithInRange(Vec2 const& playerXY)
{
	const IntVec2 playerChunk = GetChunkCoordsFromWorld(playerXY);

	float bestDist = std::numeric_limits<float>::max();
	IntVec2 bestCoords;

	const int minX = playerChunk.x - CHUNK_ACTIVATION_RADIUS_X;
	const int maxX = playerChunk.x + CHUNK_ACTIVATION_RADIUS_X;
	const int minY = playerChunk.y - CHUNK_ACTIVATION_RADIUS_Y;
	const int maxY = playerChunk.y + CHUNK_ACTIVATION_RADIUS_Y;

	bool foundCandidate = false;

	for (int y = minY; y <= maxY; ++y)
	{
		for (int x = minX; x <= maxX; ++x)
		{
			IntVec2 coords(x, y);
			if (IsChunkActive(coords))
			{
				continue;
			}
			if (!IsWithinActivationRange(playerXY, coords))
			{
				continue;
			}
			const float dist = GetDistance2D(playerXY, GetChunkCenter(coords));
			if (dist < bestDist)
			{
				bestDist = dist;
				bestCoords = coords;
				foundCandidate = true;
			}
		}
	}

	if (foundCandidate)
	{
		ActivateChunkAt(bestCoords);
		return true;
	}

	return false;
}

bool World::TryDeactivateFarthestChunkOutOfRange(Vec2 const& playerXY)
{
	float farthestDist = -1.0f;
	Chunk* farthestChunk = nullptr;

	for (auto& kv : m_activeChunks)
	{
		Chunk* chunk = kv.second;
		if (!chunk) continue;

		const IntVec2& coords = chunk->GetChunkCoords();
		if (!IsBeyondDeactivationRange(playerXY, coords))
		{
			continue;
		}

		const float dist = GetDistance2D(playerXY, GetChunkCenter(coords));
		if (dist > farthestDist)
		{
			farthestDist = dist;
			farthestChunk = chunk;
		}
	}

	if (farthestChunk)
	{
		DeactivateChunk(farthestChunk);
		return true;
	}

	return false;
}
*/

#pragma endregion


