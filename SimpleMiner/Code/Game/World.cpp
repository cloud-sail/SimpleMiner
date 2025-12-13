#include "Game/World.hpp"
#include "Game/Chunk.hpp"
#include "Game/Game.hpp"
#include "Game/ChunkUtils.hpp"
#include "Game/BlockDefinition.hpp"
#include "Game/Player.hpp"
#include "Game/GameCamera.hpp"
#include "Game/ChunkFadeOutManager.hpp"
#include "Game/LightningStrikeSystem.hpp"
#include "Game/BloomEffect.hpp"
#include "Game/RainParticleSystem.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Core/DebugRender.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/Frustum.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/Buffer.hpp"
#include "Engine/Window/Window.hpp"
#include <limits>

#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/Noise/SmoothNoise.hpp"

World::~World()
{
	delete m_rainSystem;
	m_rainSystem = nullptr;

	ReleaseHDRRenderTargets();

	delete m_bloomEffect;
	m_bloomEffect = nullptr;

	delete m_lightningStrikeSystem;
	m_lightningStrikeSystem = nullptr;

	delete m_gameCamera;
	m_gameCamera = nullptr;

	delete m_player;
	m_player = nullptr;

	delete m_worldClock;
	m_worldClock = nullptr;

	DestroyWorldConstantBuffer();
	delete m_fadeOutManager;
	m_fadeOutManager = nullptr;
	FlushJobSystemAndRetrieveJobs();
	CleanupPendingChunks();
	SaveAndCleanupActiveChunks();
}

World::World(Game* game)
	: m_game(game)
{
	m_worldClock = new Clock(*m_game->GetClock());
	m_outDoorLightGradient = Gradient::MakeSkyAmbientLightGradient();

	m_blockToBePlaced = BlockDefinition::GetBlockTypeIDByName(m_blockLists[0]);

	EnsureDirectoryExists(Chunk::GetSavesDirectory());
	g_theJobSystem->StartAcceptingJobs();

	m_fadeOutManager = new ChunkFadeOutManager(this);

	CreateWorldConstantBuffer();

	Player* player = new Player(this);
	m_player = player;

	m_gameCamera = new GameCamera(player);

	m_lightningStrikeSystem = new LightningStrikeSystem(g_theRenderer);

	//-----------------------------------------------------------------------------------------------
	m_bloomEffect = new BloomEffect();
	m_bloomEffect->Initialize();

	m_bloomEffect->SetBloomIntensity(2.0f);
	m_bloomEffect->SetBloomThreshold(0.3f);

	m_copyToBackBufferShader = g_theRenderer->CreateOrGetShader(ShaderConfig("Data/Shaders/FullScreenQuad"), VertexType::VERTEX_NONE);

	ShaderConfig depthOnlyConfig;
	depthOnlyConfig.m_name = "Data/Shaders/DepthOnly";
	//depthOnlyConfig.m_stages = SHADER_STAGE_VS; // PS is not used
	m_depthOnlyShader = g_theRenderer->CreateOrGetShader(ShaderConfig("Data/Shaders/DepthOnly"), VertexType::VERTEX_PCU);

	m_skyShader = g_theRenderer->CreateOrGetShader(ShaderConfig("Data/Shaders/Sky"), VertexType::VERTEX_NONE);


	IntVec2 clientDimensions = Window::s_mainWindow->GetClientDimensions();
	m_windowWidth = static_cast<unsigned int>(clientDimensions.x);
	m_windowHeight = static_cast<unsigned int>(clientDimensions.y);
	CreateHDRRenderTargets();

	m_rainSystem = new RainParticleSystem();
	m_rainSystem->Initialize(50000);
}

void World::Update()
{
	ResetPerFrameData();

	ProcessCompletedJobs();


	Vec3 playerPosition = GetPlayerCameraPosition();
	Vec2 playerPositionXY = Vec2(playerPosition.x, playerPosition.y);
	IntVec3 playerGlobalCoords = GetBlockGlobalCoordsFromWorld(playerPosition);
	IntVec2 playerChunkCoords = GetChunkCoordsFromBlockGlobal(playerGlobalCoords);
	//IntVec3 playerLocalCoords = GetBlockLocalCoordsFromGlobal(playerGlobalCoords);

	ActivateNearbyChunks(playerPositionXY, playerChunkCoords);
	DeactivateFarChunks(playerPositionXY);

	//-----------------------------------------------------------------------------------------------
	// Gameplay Logic
	
	if (g_theInput->WasKeyJustPressed(KEYCODE_F))
	{
		m_isPlayerCameraLocked = !m_isPlayerCameraLocked;
		if (m_isPlayerCameraLocked)
		{
			// Copy camera to locked one
			m_lockedPlayerCamera = m_gameCamera->m_camera;
			m_gameCamera->SetCameraMode(GameCameraMode::SPECTATOR_XY); 
			// Be careful in this mode, not recommend to change camera mode
		}
	}
	
	
	float deltaSeconds = g_theGame->GetDeltaSeconds();

	if (g_theInput->WasKeyJustPressed(KEYCODE_L))
	{
		m_lightningStrikeSystem->SpawnLightningStrike(Vec3(0.f, 0.f, 130.f), Vec3(0.f, 0.f, 70.f));
	}

	m_lightningStrikeSystem->Update(deltaSeconds);
	
	m_gameCamera->Update();
	m_player->Update(deltaSeconds);

	// Physics
	m_owedPhysicsSeconds += deltaSeconds;
	while (m_owedPhysicsSeconds >= m_fixedTimeStep)
	{
		// Entity Update Physics
		m_player->UpdatePhysics(m_fixedTimeStep);
		

		m_owedPhysicsSeconds -= m_fixedTimeStep;
	}

	// Then Update Game Camera
	m_gameCamera->LateUpdate();


	UpdateRayAndDoRaycast();
	HandleDiggingAndPlacing();

	//-----------------------------------------------------------------------------------------------
	ProcessDirtyLighting();

	RegenerateDirtyChunkMeshes(playerPositionXY);
	UpdateAllActiveChunks();
	

	UpdateDayNightSystem();
	ShowImGuiWindow();





	m_rainSystem->Update(playerPosition, deltaSeconds);


	UpdateWorldConstantBuffer();
	// Task Can generate vertex indices in cpu, but copy from cpu to gpu need to be done in main thread, in Chunk Update function?
	// Updating VB IB needs to be done in main thread, but delete doesn't (a queue)

	//-----------------------------------------------------------------------------------------------
	// Debug Draw
	if (m_isPlayerCameraLocked)
	{
		m_lockedPlayerCamera.DebugDrawFrustum();
	}
}


void World::Render() const
{
	RenderDepthPrePass();

	g_theRenderer->BeginCamera(GetWorldCamera());
	RenderSkyPass();
	RenderOpaquePass();
	RenderEmissivePass();
	RenderRainParticles();

	g_theRenderer->EndCamera(GetWorldCamera());

	
	RenderBloomPass();
	CopyFinalToBackBuffer();
}

void World::OnWindowResized()
{
	IntVec2 clientDimensions = Window::s_mainWindow->GetClientDimensions();
	m_windowWidth = static_cast<unsigned int>(clientDimensions.x);
	m_windowHeight = static_cast<unsigned int>(clientDimensions.y);

	CreateHDRRenderTargets();
	m_bloomEffect->OnResize(m_windowWidth, m_windowHeight);

	RefreshAspectRatio();
}

void World::RenderDepthPrePass() const
{
	// No RT, Only Depth
	m_rainSystem->PrepareDepthPrePass();

	g_theRenderer->BeginCamera(m_rainSystem->m_depthCamera, m_rainSystem->GetDepthBufferSize());
	Frustum frustum = m_rainSystem->m_depthCamera.GetFrustum();

	if (m_isPlayerCameraLocked)
	{
		m_rainSystem->m_depthCamera.DebugDrawFrustum();
	}

	g_theRenderer->SetModelConstants();
	for (auto const& kv : m_activeChunks)
	{
		Chunk const* chunk = kv.second;
		if (chunk)
		{
			if (IsAABBOnFrustum(chunk->GetWorldBounds(), frustum))
			{
				chunk->RenderDepth();
			}
		}
	}

	g_theRenderer->EndCamera(m_rainSystem->m_depthCamera);
}

void World::RenderSkyPass() const
{
	// ToDo Lightning

	// RT0: sceneRT, RT1: skyQuadRT, Disable Depth
	g_theRenderer->TransitionToRenderTarget(*m_sceneRT);
	g_theRenderer->TransitionToRenderTarget(*m_skyQuadRT);
	g_theRenderer->TransitionToDepthWrite(*m_sceneDepthBuffer);

	g_theRenderer->ClearRenderTargetByIndex(m_sceneRTV.m_index, Rgba8::MAGENTA);
	g_theRenderer->ClearRenderTargetByIndex(m_skyQuadRTV.m_index, Rgba8::TRANSPARENT_BLACK);
	g_theRenderer->ClearDepthAndStencilByIndex(m_sceneDepthDSV.m_index, 1.f);


	std::vector<DXGI_FORMAT> rtvFormats = {
		m_hdrFormat,
		m_hdrFormat
	};
	g_theRenderer->SetRenderTargetFormats(rtvFormats, m_sceneDepthFormat);

	std::vector<uint32_t> rtvIndexes = { m_sceneRTV.m_index , m_skyQuadRTV.m_index };
	g_theRenderer->SetRenderTargetsByIndex(rtvIndexes, m_sceneDepthDSV.m_index);

	SkyResources resources;
	resources.cameraConstantsIndex = g_theRenderer->GetCurrentCameraConstantsIndex();
	resources.worldConstantsIndex = GetWorldConstantsIndex();

	g_theRenderer->SetGraphicsBindlessResources(sizeof(SkyResources), &resources);
	g_theRenderer->BindShader(m_skyShader);
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	//g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	g_theRenderer->SetDepthMode(DepthMode::DISABLED);

	g_theRenderer->DrawProcedural(6);
}

void World::RenderOpaquePass() const
{
	//-----------------------------------------------------------------------------------------------
	g_theRenderer->TransitionToPixelShaderResource(*m_skyQuadRT);
	//-----------------------------------------------------------------------------------------------
	Frustum frustum = GetPlayerCameraFrustum();
	int debugLayerMode = g_theGame->GetDebugLayerMode();

	g_theRenderer->SetModelConstants();
	for (auto const& kv : m_activeChunks)
	{
		Chunk const* chunk = kv.second;
		if (chunk)
		{
			if (IsAABBOnFrustum(chunk->GetWorldBounds(), frustum))
			{
				chunk->Render();

				chunk->DebugDrawBiome(debugLayerMode);
			}
		}
	}

	m_fadeOutManager->Render(frustum);

	if (g_isDebugDraw)
	{
		Vec3 playerPosition = GetPlayerCameraPosition();
		IntVec3 playerGlobalCoords = GetBlockGlobalCoordsFromWorld(playerPosition);
		IntVec2 playerChunkCoords = GetChunkCoordsFromBlockGlobal(playerGlobalCoords);

		Chunk* playerChunk = GetActiveChunk(playerChunkCoords);
		if (playerChunk)
		{
			playerChunk->DebugRenderChunkGrid();
		}
	}
	//-----------------------------------------------------------------------------------------------
}

void World::RenderEmissivePass() const
{
	g_theRenderer->TransitionToRenderTarget(*m_emissiveRT);
	g_theRenderer->ClearRenderTargetByIndex(m_emissiveRTV.m_index, Rgba8::TRANSPARENT_BLACK);

	std::vector<DXGI_FORMAT> rtvFormats = {
		m_hdrFormat,
		m_hdrFormat
	};
	g_theRenderer->SetRenderTargetFormats(rtvFormats, m_sceneDepthFormat);

	std::vector<uint32_t> rtvIndexes = { m_sceneRTV.m_index, m_emissiveRTV.m_index };
	g_theRenderer->SetRenderTargetsByIndex(rtvIndexes, m_sceneDepthDSV.m_index);

	//-----------------------------------------------------------------------------------------------
	m_lightningStrikeSystem->Render(static_cast<float>(m_worldClock->GetTotalSeconds()));
	//-----------------------------------------------------------------------------------------------
}

void World::RenderRainParticles() const
{
	if (!m_showRainParticles) return;
	// ToDo change rt rt formats
	m_rainSystem->Render();
}

void World::RenderBloomPass() const
{
	if (m_enableBloom)
	{
		g_theRenderer->TransitionToAllShaderResource(*m_emissiveRT);
		g_theRenderer->TransitionToAllShaderResource(*m_sceneRT);
		g_theRenderer->TransitionToUnorderedAccess(*m_finalRT);

		m_bloomEffect->Execute(m_emissiveSRV, m_sceneSRV, m_finalUAV);
	}
	else
	{
		g_theRenderer->CopyTexture(*m_sceneRT, *m_finalRT); // Same size Same format
	}
}

void World::CopyFinalToBackBuffer() const
{
	std::vector<uint32_t> rtvIndexes = {
		g_theRenderer->GetCurrentBackBufferIndex()
	};

	g_theRenderer->SetRenderTargetsByIndex(rtvIndexes, m_sceneDepthDSV.m_index);
	g_theRenderer->SetRenderTargetFormats({ DXGI_FORMAT_R8G8B8A8_UNORM }, m_sceneDepthFormat); // Notes: Depth Buffer Format

	g_theRenderer->BindShader(m_copyToBackBufferShader);
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
	g_theRenderer->SetDepthMode(DepthMode::DISABLED);

	DescriptorHandle inputSRV = m_finalSRV;

	FullScreenQuadResources resources;
	resources.textureIndex = inputSRV.m_index;
	resources.samplerIndex = g_theRenderer->GetDefaultSamplerIndex(SamplerMode::POINT_CLAMP);

	g_theRenderer->SetGraphicsBindlessResources(sizeof(FullScreenQuadResources), &resources);

	g_theRenderer->DrawProcedural(6);
}

void World::CreateHDRRenderTargets()
{
	ReleaseHDRRenderTargets();

	TextureInit sceneInit;
	sceneInit.m_width = m_windowWidth;
	sceneInit.m_height = m_windowHeight;
	sceneInit.m_format = m_hdrFormat;
	sceneInit.m_allowSRV = true;
	sceneInit.m_allowRTV = true;
	sceneInit.m_debugName = L"Game_SceneRT";
	sceneInit.m_rtvClearFormat = m_hdrFormat;
	Rgba8::MAGENTA.GetAsFloats(sceneInit.m_rtvClearColor);

	m_sceneRT = g_theRenderer->CreateTexture(sceneInit);
	m_sceneRTV = g_theRenderer->AllocateRTV(*m_sceneRT);
	m_sceneSRV = g_theRenderer->AllocateSRV(*m_sceneRT);

	TextureInit depthInit;
	depthInit.m_width = m_windowWidth;
	depthInit.m_height = m_windowHeight;
	depthInit.m_format = m_sceneDepthFormat;
	depthInit.m_allowDSV = true;
	depthInit.m_debugName = L"Game_SceneDepth";
	depthInit.m_dsvClearFormat = m_sceneDepthFormat;

	m_sceneDepthBuffer = g_theRenderer->CreateTexture(depthInit);
	m_sceneDepthDSV = g_theRenderer->AllocateDSV(*m_sceneDepthBuffer);

	TextureInit finalInit;
	finalInit.m_width = m_windowWidth;
	finalInit.m_height = m_windowHeight;
	finalInit.m_format = m_hdrFormat;
	finalInit.m_allowSRV = true;
	finalInit.m_allowUAV = true;
	finalInit.m_debugName = L"Game_FinalRT";

	m_finalRT = g_theRenderer->CreateTexture(finalInit);
	m_finalSRV = g_theRenderer->AllocateSRV(*m_finalRT);
	m_finalUAV = g_theRenderer->AllocateUAV(*m_finalRT);

	TextureInit emissiveInit;
	emissiveInit.m_width = m_windowWidth;
	emissiveInit.m_height = m_windowHeight;
	emissiveInit.m_format = m_hdrFormat;
	emissiveInit.m_allowSRV = true;
	emissiveInit.m_allowRTV = true;
	emissiveInit.m_debugName = L"Game_EmissiveRT";
	emissiveInit.m_rtvClearFormat = m_hdrFormat;

	m_emissiveRT = g_theRenderer->CreateTexture(emissiveInit);
	m_emissiveRTV = g_theRenderer->AllocateRTV(*m_emissiveRT);
	m_emissiveSRV = g_theRenderer->AllocateSRV(*m_emissiveRT);

	TextureInit skyQuadInit;
	skyQuadInit.m_width = m_windowWidth;
	skyQuadInit.m_height = m_windowHeight;
	skyQuadInit.m_format = m_hdrFormat;
	skyQuadInit.m_allowSRV = true;
	skyQuadInit.m_allowRTV = true;
	skyQuadInit.m_debugName = L"Game_SkyQuadRT";
	skyQuadInit.m_rtvClearFormat = m_hdrFormat;

	m_skyQuadRT = g_theRenderer->CreateTexture(emissiveInit);
	m_skyQuadRTV = g_theRenderer->AllocateRTV(*m_skyQuadRT);
	m_skyQuadSRV = g_theRenderer->AllocateSRV(*m_skyQuadRT);
}

void World::ReleaseHDRRenderTargets()
{
	g_theRenderer->DestroyTexture(m_sceneRT);
	g_theRenderer->EnqueueDeferredRelease(m_sceneRTV);
	g_theRenderer->EnqueueDeferredRelease(m_sceneSRV);

	g_theRenderer->DestroyTexture(m_sceneDepthBuffer);
	g_theRenderer->EnqueueDeferredRelease(m_sceneDepthDSV);

	g_theRenderer->DestroyTexture(m_finalRT);
	g_theRenderer->EnqueueDeferredRelease(m_finalSRV);
	g_theRenderer->EnqueueDeferredRelease(m_finalUAV);

	g_theRenderer->DestroyTexture(m_emissiveRT);
	g_theRenderer->EnqueueDeferredRelease(m_emissiveRTV);
	g_theRenderer->EnqueueDeferredRelease(m_emissiveSRV);

	g_theRenderer->DestroyTexture(m_skyQuadRT);
	g_theRenderer->EnqueueDeferredRelease(m_skyQuadRTV);
	g_theRenderer->EnqueueDeferredRelease(m_skyQuadSRV);
}

Chunk* World::GetActiveChunk(IntVec2 const& coords) const
{
	auto it = m_activeChunks.find(coords);
	return (it != m_activeChunks.end()) ? it->second : nullptr;
}

BlockIterator World::GetBlockIterFromGlobalCoords(IntVec3 const& globalCoords) const
{
	IntVec2 chunkCoords = GetChunkCoordsFromBlockGlobal(globalCoords);
	Chunk* chunk = GetActiveChunk(chunkCoords);
	if (chunk == nullptr)
	{
		return BlockIterator();
	}
	IntVec3 localCoords = GetBlockLocalCoordsFromGlobal(globalCoords);

	int blockIndex = GetBlockIndexInChunk(localCoords);

	return BlockIterator(chunk, blockIndex);
}

bool World::IsChunkActive(IntVec2 const& coords) const
{
	return m_activeChunks.find(coords) != m_activeChunks.end();
}

void World::HookupNeighbors(Chunk* chunk)
{
	if (!chunk) return;
	const IntVec2& c = chunk->GetChunkCoords();

	// east (x+1), west (x-1), north (y+1), south (y-1)
	IntVec2 eastCoords(c.x + 1, c.y);
	IntVec2 westCoords(c.x - 1, c.y);
	IntVec2 northCoords(c.x, c.y + 1);
	IntVec2 southCoords(c.x, c.y - 1);

	Chunk* east = GetActiveChunk(eastCoords);
	Chunk* west = GetActiveChunk(westCoords);
	Chunk* north = GetActiveChunk(northCoords);
	Chunk* south = GetActiveChunk(southCoords);

	chunk->SetEastNeighbor(east);
	chunk->SetWestNeighbor(west);
	chunk->SetNorthNeighbor(north);
	chunk->SetSouthNeighbor(south);

	if (east)  east->SetWestNeighbor(chunk);
	if (west)  west->SetEastNeighbor(chunk);
	if (north) north->SetSouthNeighbor(chunk);
	if (south) south->SetNorthNeighbor(chunk);
}

void World::UnhookNeighbors(Chunk* chunk)
{
	if (!chunk) return;

	Chunk* east = chunk->GetEastNeighbor();
	Chunk* west = chunk->GetWestNeighbor();
	Chunk* north = chunk->GetNorthNeighbor();
	Chunk* south = chunk->GetSouthNeighbor();

	if (east)  east->SetWestNeighbor(nullptr);
	if (west)  west->SetEastNeighbor(nullptr);
	if (north) north->SetSouthNeighbor(nullptr);
	if (south) south->SetNorthNeighbor(nullptr);

	chunk->SetEastNeighbor(nullptr);
	chunk->SetWestNeighbor(nullptr);
	chunk->SetNorthNeighbor(nullptr);
	chunk->SetSouthNeighbor(nullptr);
}

bool World::IsWithinActivationRange(const Vec2& playerXY, const IntVec2& coords)
{
	float const squaredDist = GetDistanceSquared2D(playerXY, GetChunkCenter(coords));
	float const activationRange = (float)CHUNK_ACTIVATION_RANGE;
	return squaredDist <= activationRange * activationRange;
}

bool World::IsBeyondDeactivationRange(const Vec2& playerXY, const IntVec2& coords)
{
	float const squaredDist = GetDistanceSquared2D(playerXY, GetChunkCenter(coords));
	float const deactivationRange = (float)CHUNK_DEACTIVATION_RANGE;
	return squaredDist >= deactivationRange * deactivationRange;
}

bool World::HasRunningJobInChunk(IntVec2 const& coords) const
{
	return m_generatingChunks.find(coords) != m_generatingChunks.end() ||
		m_loadingChunks.find(coords) != m_loadingChunks.end() ||
		m_savingChunks.find(coords) != m_savingChunks.end();
}

void World::FlushJobSystemAndRetrieveJobs()
{
	g_theJobSystem->Flush(JobPriority::CRITICAL);
	std::vector<Job*> completedJobs = g_theJobSystem->RetrieveCompletedJobs();
	UNUSED(completedJobs);
}

void World::CleanupPendingChunks()
{
	// Delete allocated chunks (just generated, just loaded, just saved) assume not need to save
	std::vector<Chunk*> toDelete;
	toDelete.reserve(m_generatingChunks.size() + m_loadingChunks.size() + m_savingChunks.size());

	for (auto& kv : m_generatingChunks)
	{
		toDelete.push_back(kv.second);
	}
	m_generatingChunks.clear();

	for (auto& kv : m_loadingChunks)
	{
		toDelete.push_back(kv.second);
	}
	m_loadingChunks.clear();

	for (auto& kv : m_savingChunks)
	{
		toDelete.push_back(kv.second);
	}
	m_savingChunks.clear();

	for (Chunk* c : toDelete)
	{
		delete c;
	}
}

void World::SaveAndCleanupActiveChunks()
{
	std::vector<Chunk*> toSaveAndDelete;
	toSaveAndDelete.reserve(m_activeChunks.size());
	for (auto& kv : m_activeChunks)
	{
		toSaveAndDelete.push_back(kv.second);
	}
	m_activeChunks.clear();
	for (Chunk* c : toSaveAndDelete)
	{
		// No need to Unhook
		c->SaveToDiskIfNeeded();
		delete c;
	}
}

void World::ResetPerFrameData()
{
	m_totalNumVertices = 0;
	m_totalNumIndices = 0;
}

void World::ProcessCompletedJobs()
{
	// Retrieve completed Jobs
	std::vector<Job*> completedJobs = g_theJobSystem->RetrieveCompletedJobs();

	for (Job* job : completedJobs)
	{
		GenerateChunkJob* generateChunkJob = dynamic_cast<GenerateChunkJob*>(job);
		if (generateChunkJob)
		{
			Chunk* chunk = generateChunkJob->m_chunk;

			GUARANTEE_OR_DIE(m_generatingChunks.erase(chunk->GetChunkCoords()) > 0, "The Chunk is not from generatingChunks.");

			m_activeChunks.emplace(chunk->GetChunkCoords(), chunk);
			HookupNeighbors(chunk);
			chunk->UpdateLightInfluenceAfterActivating();

			delete job;
			job = nullptr;
			continue;
		}

		LoadChunkJob* loadChunkJob = dynamic_cast<LoadChunkJob*>(job);
		if (loadChunkJob)
		{
			Chunk* chunk = loadChunkJob->m_chunk;

			GUARANTEE_OR_DIE(m_loadingChunks.erase(chunk->GetChunkCoords()) > 0, "The Chunk is not from loadingChunks.");

			m_activeChunks.emplace(chunk->GetChunkCoords(), chunk);
			HookupNeighbors(chunk);
			chunk->UpdateLightInfluenceAfterActivating();

			delete job;
			job = nullptr;

			continue;
		}

		SaveChunkJob* saveChunkJob = dynamic_cast<SaveChunkJob*>(job);
		if (saveChunkJob)
		{
			Chunk* chunk = saveChunkJob->m_chunk;

			GUARANTEE_OR_DIE(m_savingChunks.erase(chunk->GetChunkCoords()) > 0, "The Chunk is not from saveChunks.");

			delete chunk;
			chunk = nullptr;

			delete job;
			job = nullptr;

			continue;
		}
	}
}

void World::ActivateNearbyChunks(const Vec2& playerPositionXY, const IntVec2& playerChunkCoords)
{
	if (GetActiveChunkCount() < MAX_ACTIVE_CHUNKS)
	{

		for (IntVec2 const& offset : g_chunkActivationOffsets)
		{
			IntVec2 coords = playerChunkCoords + offset;
			if (IsChunkActive(coords) || HasRunningJobInChunk(coords))
			{
				continue;
			}
			if (!IsWithinActivationRange(playerPositionXY, coords))
			{
				continue;
			}

			std::string const filePath = Chunk::GetChunkFilePath(coords);

			bool hasReachedJobLimit = true;

			if (FileExists(filePath))
			{
				// Loading
				if (m_loadingChunks.size() < MAX_LOADING_CHUNKS)
				{
					hasReachedJobLimit = false;
					Chunk* chunk = new Chunk(this, coords);
					chunk->m_state = ChunkState::ACTIVATING_QUEUED_LOAD;

					auto [iter, inserted] = m_loadingChunks.try_emplace(coords, chunk);
					GUARANTEE_OR_DIE(inserted, "This chunk already has jobs");

					g_theJobSystem->AddJob(new LoadChunkJob(chunk));
				}
			}
			else
			{
				// Generating
				if (m_generatingChunks.size() < MAX_GENERATING_CKUNKS)
				{
					hasReachedJobLimit = false;
					Chunk* chunk = new Chunk(this, coords);
					chunk->m_state = ChunkState::ACTIVATING_QUEUED_GENERATE;

					auto [iter, inserted] = m_generatingChunks.try_emplace(coords, chunk);
					GUARANTEE_OR_DIE(inserted, "This chunk already has jobs");

					g_theJobSystem->AddJob(new GenerateChunkJob(chunk));
				}
			}

			if (hasReachedJobLimit)
			{
				break;
			}

			if (GetActiveChunkCount() + m_loadingChunks.size() + m_generatingChunks.size() >= MAX_ACTIVE_CHUNKS)
			{
				break;
			}

		}
	}
}

void World::DeactivateFarChunks(const Vec2& playerPositionXY)
{
	// Deactivate Chunks (For loop for 256 chunks and if they need to be saved queue them. if no need to save, delete it immediately)

	std::vector<std::pair<float, IntVec2>> chunksToDeactivate;
	chunksToDeactivate.reserve(MAX_CHUNKS_DEACTIVATED_PER_FRAME);

	for (auto& pair : m_activeChunks)
	{
		const IntVec2& chunkCoords = pair.first;
		if (IsBeyondDeactivationRange(playerPositionXY, chunkCoords))
		{
			float squaredDist = GetDistanceSquared2D(playerPositionXY, GetChunkCenter(chunkCoords));
			chunksToDeactivate.push_back({ squaredDist, chunkCoords });
		}
	}

	std::sort(chunksToDeactivate.begin(), chunksToDeactivate.end(),
		[](const auto& a, const auto& b) {
			return a.first > b.first;
		});

	int numToRemove = std::min(MAX_CHUNKS_DEACTIVATED_PER_FRAME, (int)chunksToDeactivate.size());
	for (int i = 0; i < numToRemove; ++i)
	{
		// Try To Deactivate Chunk
		IntVec2 coords = chunksToDeactivate[i].second;
		auto iter = m_activeChunks.find(coords);
		if (iter != m_activeChunks.end()) // should always find
		{
			Chunk* chunk = iter->second; // should not be nullptr
			if (chunk)
			{
				if (chunk->NeedsSaving())
				{
					// Wait for saving, Delete it later
					if (m_savingChunks.size() < MAX_SAVING_CHUNKS)
					{
						chunk->UpdateLightInfluenceBeforeDeactivating();

						GUARANTEE_OR_DIE(m_activeChunks.erase(coords) > 0, "The Chunk is not in activeChunks.");
						UnhookNeighbors(chunk);
						m_fadeOutManager->AddChunk(chunk);
						m_savingChunks.emplace(coords, chunk);

						g_theJobSystem->AddJob(new SaveChunkJob(chunk));
					}
				}
				else
				{
					// Delete it immediately (#ToDo make a function)
					chunk->UpdateLightInfluenceBeforeDeactivating();

					UnhookNeighbors(chunk);
					m_fadeOutManager->AddChunk(chunk);
					m_activeChunks.erase(chunk->GetChunkCoords());

					delete chunk;
					chunk = nullptr;
				}
			}
		}
		else
		{
			ERROR_AND_DIE("Unexpected");
		}

	}
}

void World::RegenerateDirtyChunkMeshes(const Vec2& playerPositionXY)
{
	std::vector<std::pair<float, Chunk*>> chunksToRegenerate;
	chunksToRegenerate.reserve(MAX_CHUNK_MESHES_BUILT_PER_FRAME);

	for (auto& pair : m_activeChunks)
	{
		const IntVec2& chunkCoords = pair.first;
		Chunk* chunk = pair.second;

		// Only construct meshes for chunks with all neighbors active

		if (chunk && chunk->IsMeshDirty() && 
			chunk->GetNorthNeighbor() && chunk->GetSouthNeighbor() && chunk->GetEastNeighbor() && chunk->GetWestNeighbor())
		{
			float squaredDist = GetDistanceSquared2D(playerPositionXY, GetChunkCenter(chunkCoords));
			chunksToRegenerate.push_back({ squaredDist, chunk });
		}
	}

	std::sort(chunksToRegenerate.begin(), chunksToRegenerate.end(),
		[](const auto& a, const auto& b) {
			return a.first < b.first;
		});

	int numToRegenerate = std::min(MAX_CHUNK_MESHES_BUILT_PER_FRAME, (int)chunksToRegenerate.size());
	for (int i = 0; i < numToRegenerate; ++i)
	{
		Chunk* chunk = chunksToRegenerate[i].second;
		chunk->RegenerateMeshIfDirty();
		chunk->UploadToGpuIfNeeded();
		chunk->m_hasMeshGenerated = true; // for fade in
	}
}

void World::UpdateAllActiveChunks()
{
	// Do not add/remove key in activeChunk map when updating
	// Or push all chunks into a vector as a backup first then iterate the vector
	float deltaSeconds = g_theGame->GetDeltaSeconds();

	for (auto& kv : m_activeChunks)
	{
		Chunk* chunk = kv.second;
		chunk->Update(deltaSeconds);
		m_totalNumVertices += chunk->m_totalNumVertices;
		m_totalNumIndices += chunk->m_totalNumIndices;
	}

	m_fadeOutManager->Update(deltaSeconds);
}

void World::HandleDiggingAndPlacing()
{
	ChoosePlacedBlock();

	if (m_raycastResult.m_didImpact)
	{
		XboxController const& controller = g_theInput->GetController(0);
		if (g_theInput->WasKeyJustPressed(KEYCODE_LEFT_MOUSE) || controller.WasButtonJustPressed(XboxButtonId::XBOX_BUTTON_X))
		{
			DigBlock(m_raycastResult.m_impactedBlockIter);
		}

		if (g_theInput->WasKeyJustPressed(KEYCODE_RIGHT_MOUSE) || controller.WasButtonJustPressed(XboxButtonId::XBOX_BUTTON_Y))
		{
			PlaceBlock(m_raycastResult.m_previousBlockIter, m_blockToBePlaced);
		}


	}

	//Vec3 playerPos = g_theGame->GetPlayerCameraPosition();
	//IntVec3 playerGlobalCoords = GetBlockGlobalCoordsFromWorld(playerPos);

	//IntVec2 playerChunkCoords = GetChunkCoordsFromBlockGlobal(playerGlobalCoords);
	//IntVec3 playerLocalCoords = GetBlockLocalCoordsFromGlobal(playerGlobalCoords);

	//Chunk* chunk = GetActiveChunk(playerChunkCoords);
	//if (chunk == nullptr) return;

	//XboxController const& controller = g_theInput->GetController(0);
	//if (g_theInput->WasKeyJustPressed(KEYCODE_LEFT_MOUSE) || controller.WasButtonJustPressed(XboxButtonId::XBOX_BUTTON_X))
	//{
	//	DigOneNonAirBlockAtOrUnderPlayer(chunk, playerLocalCoords);
	//}

	//if (g_theInput->WasKeyJustPressed(KEYCODE_RIGHT_MOUSE) || controller.WasButtonJustPressed(XboxButtonId::XBOX_BUTTON_Y))
	//{
	//	PlaceOneBlockAboveNonAirBlockUnderPlayer(chunk, playerLocalCoords);
	//}
}

void World::DigBlock(BlockIterator iter, uint8_t airType /*= 0*/)
{
	if (!iter.IsValid()) return;
	Block* block = iter.GetBlock();
	block->SetTypeID(airType);
	MarkLightingDirty(iter);

	BlockIterator aboveBlock = iter.GetSkywardNeighbor();
	if (aboveBlock.IsValid() && aboveBlock.GetBlock()->IsSky())
	{
		BlockIterator currentIter = iter;
		while (currentIter.IsValid())
		{
			Block* currentBlock = currentIter.GetBlock();
			if (currentBlock->IsFullOpaque())
			{
				break;
			}

			currentBlock->SetIsSky(true);
			MarkLightingDirty(currentIter);

			currentIter = currentIter.GetDownwardNeighbor(); 
		}
	}

	iter.MarkChunkMeshDirty();
	iter.SetChunkNeedsSaving();

	// Removing block at edge and the light influence is still 0 (not changed), need to mark neighbor chunk mesh dirty
	{
		BlockIterator neighborIter = iter.GetEastNeighbor();
		if (neighborIter.IsValid())
		{
			if (neighborIter.m_chunk != iter.m_chunk)
			{
				neighborIter.MarkChunkMeshDirty();
			}
		}
	}

	{
		BlockIterator neighborIter = iter.GetWestNeighbor();
		if (neighborIter.IsValid())
		{
			if (neighborIter.m_chunk != iter.m_chunk)
			{
				neighborIter.MarkChunkMeshDirty();
			}
		}
	}

	{
		BlockIterator neighborIter = iter.GetNorthNeighbor();
		if (neighborIter.IsValid())
		{
			if (neighborIter.m_chunk != iter.m_chunk)
			{
				neighborIter.MarkChunkMeshDirty();
			}
		}
	}

	{
		BlockIterator neighborIter = iter.GetSouthNeighbor();
		if (neighborIter.IsValid())
		{
			if (neighborIter.m_chunk != iter.m_chunk)
			{
				neighborIter.MarkChunkMeshDirty();
			}
		}
	}
}

void World::PlaceBlock(BlockIterator iter, uint8_t newType)
{
	if (!iter.IsValid()) return;
	Block* block = iter.GetBlock();

	//bool wasSky = block->IsSky();

	block->SetTypeID(newType);
	MarkLightingDirty(iter);

	if (block->IsSky() && block->IsFullOpaque()) // was flagged as SKY and now is opaque
	{
		block->SetIsSky(false);

		BlockIterator currentIter = iter.GetDownwardNeighbor();
		while (currentIter.IsValid())
		{
			Block* currentBlock = currentIter.GetBlock();
			if (currentBlock->IsFullOpaque())
			{
				break;
			}

			currentBlock->SetIsSky(false);
			MarkLightingDirty(currentIter);

			currentIter = currentIter.GetDownwardNeighbor();
		}
	}

	iter.MarkChunkMeshDirty();
	iter.SetChunkNeedsSaving();
}

void World::ChoosePlacedBlock()
{
	XboxController const& controller = g_theInput->GetController(0);
	// Select block type
	if (g_theInput->WasKeyJustPressed('1') || controller.WasButtonJustPressed(XboxButtonId::XBOX_BUTTON_LEFT))
	{
		m_blockToBePlaced = BlockDefinition::GetBlockTypeIDByName(m_blockLists[0]);
	}

	if (g_theInput->IsKeyDown('2') || controller.WasButtonJustPressed(XboxButtonId::XBOX_BUTTON_DOWN))
	{
		m_blockToBePlaced = BlockDefinition::GetBlockTypeIDByName(m_blockLists[1]);
	}

	if (g_theInput->WasKeyJustPressed('3') || controller.WasButtonJustPressed(XboxButtonId::XBOX_BUTTON_RIGHT))
	{
		m_blockToBePlaced = BlockDefinition::GetBlockTypeIDByName(m_blockLists[2]);
	}
}

void World::DigOneNonAirBlockAtOrUnderPlayer(Chunk* chunk, IntVec3 const& playerLocalCoords)
{
	if (chunk == nullptr) return;

	int startZ = playerLocalCoords.z;
	if (startZ > CHUNK_MAX_Z) startZ = CHUNK_MAX_Z;
	if (startZ < 0) startZ = 0;

	const uint8_t airType = BlockDefinition::GetBlockTypeIDByName("Air");

	for (int z = startZ; z >= 0; --z)
	{
		const int idx = GetBlockIndexInChunk(playerLocalCoords.x, playerLocalCoords.y, z);
		const uint8_t type = chunk->m_blocks[idx].GetTypeID();
		if (type != airType)
		{
			DigBlock(BlockIterator(chunk, idx));

			//chunk->m_blocks[idx].SetTypeID(airType);
			//chunk->MarkMeshDirty();
			//chunk->SetNeedsSaving();
			return;
		}
	}
}

void World::PlaceOneBlockAboveNonAirBlockUnderPlayer(Chunk* chunk, IntVec3 const& playerLocalCoords)
{
	if (chunk == nullptr) return;

	int startZ = playerLocalCoords.z;
	if (startZ > CHUNK_MAX_Z) startZ = CHUNK_MAX_Z;
	if (startZ < 0) startZ = 0;

	const uint8_t airType = BlockDefinition::GetBlockTypeIDByName("Air");

	int currentIndex = GetBlockIndexInChunk(playerLocalCoords.x, playerLocalCoords.y, startZ);

	if (chunk->m_blocks[currentIndex].GetTypeID() != airType)
	{
		PlaceBlock(BlockIterator(chunk, currentIndex), m_blockToBePlaced);
		//chunk->m_blocks[currentIndex].SetTypeID(m_blockToBePlaced);
		//chunk->MarkMeshDirty();
		//chunk->SetNeedsSaving();
		return;
	}

	const int strideZ = 1 << (CHUNK_BITS_X + CHUNK_BITS_Y);
	currentIndex -= strideZ;
	while (currentIndex >= 0)
	{
		if (chunk->m_blocks[currentIndex].GetTypeID() != airType)
		{
			PlaceBlock(BlockIterator(chunk, currentIndex + strideZ), m_blockToBePlaced);
			//chunk->m_blocks[currentIndex + strideZ].SetTypeID(m_blockToBePlaced);
			//chunk->MarkMeshDirty();
			//chunk->SetNeedsSaving();
			return;
		}

		currentIndex -= strideZ;
	}

	// All blocks are air under player
	PlaceBlock(BlockIterator(chunk, currentIndex + strideZ), m_blockToBePlaced);
	//chunk->m_blocks[currentIndex + strideZ].SetTypeID(m_blockToBePlaced);
	//chunk->MarkMeshDirty();
	//chunk->SetNeedsSaving();
}



void World::MarkLightingDirty(BlockIterator const& iter)
{
	Block* block = iter.GetBlock();
	if (block == nullptr)
	{
		return;
	}

	if (block->IsLightDirty())
	{
		return;
	}

	block->SetIsLightDirty(true);
	//iter.m_chunk->MarkMeshDirty(); // It should be marked dirty in processing the dirty lighting queue
	m_dirtyBlockLighting.push(iter);
}

void World::MarkLightingDirtyIfNotOpaque(BlockIterator const& iter)
{
	Block* block = iter.GetBlock();
	if (block == nullptr)
	{
		return;
	}

	if (block->IsFullOpaque() || block->IsLightDirty())
	{
		return;
	}

	block->SetIsLightDirty(true);
	//iter.m_chunk->MarkMeshDirty();
	m_dirtyBlockLighting.push(iter);
}

void World::ProcessDirtyLighting()
{
	while (!m_dirtyBlockLighting.empty())
	{
		ProcessNextDirtyLightBlock();
	}
}

void World::ProcessNextDirtyLightBlock()
{
	// Pop front
	BlockIterator currentIter = m_dirtyBlockLighting.front();
	m_dirtyBlockLighting.pop();

	// Processing
	Block* block = currentIter.GetBlock();
	if (!block) return;
	block->SetIsLightDirty(false);

	// Calculate correct Light Influence
	int indoorLightInfluence = 0;

	indoorLightInfluence = BlockDefinition::GetLightInfluenceByType(block->GetTypeID());

	// Light Propagation
	if (!block->IsFullOpaque())
	{
		BlockIterator neighborIter = currentIter.GetEastNeighbor();
		indoorLightInfluence = std::max(indoorLightInfluence, neighborIter.GetIndoorLightInfluence() - 1);
	
		neighborIter = currentIter.GetWestNeighbor();
		indoorLightInfluence = std::max(indoorLightInfluence, neighborIter.GetIndoorLightInfluence() - 1);
	
		neighborIter = currentIter.GetNorthNeighbor();
		indoorLightInfluence = std::max(indoorLightInfluence, neighborIter.GetIndoorLightInfluence() - 1);
	
		neighborIter = currentIter.GetSouthNeighbor();
		indoorLightInfluence = std::max(indoorLightInfluence, neighborIter.GetIndoorLightInfluence() - 1);
	
		neighborIter = currentIter.GetSkywardNeighbor();
		indoorLightInfluence = std::max(indoorLightInfluence, neighborIter.GetIndoorLightInfluence() - 1);
	
		neighborIter = currentIter.GetDownwardNeighbor();
		indoorLightInfluence = std::max(indoorLightInfluence, neighborIter.GetIndoorLightInfluence() - 1);
	}

	int outdoorLightInfluence = 0;

	if (block->IsSky())
	{
		outdoorLightInfluence = LIGHT_MAX_VALUE;
	}
	else
	{
		if (!block->IsFullOpaque())
		{
			BlockIterator neighborIter = currentIter.GetEastNeighbor();
			outdoorLightInfluence = std::max(outdoorLightInfluence, neighborIter.GetOutdoorLightInfluence() - 1);

			neighborIter = currentIter.GetWestNeighbor();
			outdoorLightInfluence = std::max(outdoorLightInfluence, neighborIter.GetOutdoorLightInfluence() - 1);

			neighborIter = currentIter.GetNorthNeighbor();
			outdoorLightInfluence = std::max(outdoorLightInfluence, neighborIter.GetOutdoorLightInfluence() - 1);

			neighborIter = currentIter.GetSouthNeighbor();
			outdoorLightInfluence = std::max(outdoorLightInfluence, neighborIter.GetOutdoorLightInfluence() - 1);

			neighborIter = currentIter.GetSkywardNeighbor();
			outdoorLightInfluence = std::max(outdoorLightInfluence, neighborIter.GetOutdoorLightInfluence() - 1);

			neighborIter = currentIter.GetDownwardNeighbor();
			outdoorLightInfluence = std::max(outdoorLightInfluence, neighborIter.GetOutdoorLightInfluence() - 1);
		}
	}
	// #ToDo In rendering part set top block skyward face outdoor influence to MAX

	// Dirty And Incorrect Lighting: Mark Neightbor Dirty
	if (indoorLightInfluence != block->GetIndoorLightInfluence() || outdoorLightInfluence != block->GetOutdoorLightInfluence())
	{
		// Update Light Influence
		block->SetIndoorLightInfluence(static_cast<uint8_t>(indoorLightInfluence));
		block->SetOutdoorLightInfluence(static_cast<uint8_t>(outdoorLightInfluence));

		// Mark this chunk and neighbor blocks' chunk mesh Dirty
		// Mark Neighbor blocks Lighting Dirty If NotOpaque

		//currentIter.MarkChunkMeshDirty(); // not necessary

		BlockIterator neighborIter = currentIter.GetEastNeighbor();
		neighborIter.MarkChunkMeshDirty();
		MarkLightingDirtyIfNotOpaque(neighborIter);

		neighborIter = currentIter.GetWestNeighbor();
		neighborIter.MarkChunkMeshDirty();
		MarkLightingDirtyIfNotOpaque(neighborIter);

		neighborIter = currentIter.GetNorthNeighbor();
		neighborIter.MarkChunkMeshDirty();
		MarkLightingDirtyIfNotOpaque(neighborIter);

		neighborIter = currentIter.GetSouthNeighbor();
		neighborIter.MarkChunkMeshDirty();
		MarkLightingDirtyIfNotOpaque(neighborIter);

		neighborIter = currentIter.GetSkywardNeighbor();
		neighborIter.MarkChunkMeshDirty();
		MarkLightingDirtyIfNotOpaque(neighborIter);

		neighborIter = currentIter.GetDownwardNeighbor();
		neighborIter.MarkChunkMeshDirty();
		MarkLightingDirtyIfNotOpaque(neighborIter);
	}
}

void World::UndirtyAllBlocksInChunk(Chunk* chunk)
{
	std::queue<BlockIterator> newQueue;

	while (!m_dirtyBlockLighting.empty())
	{
		BlockIterator blockIter = m_dirtyBlockLighting.front();
		m_dirtyBlockLighting.pop();

		if (blockIter.m_chunk != chunk)
		{
			newQueue.push(blockIter);
		}
	}

	m_dirtyBlockLighting = std::move(newQueue);
}

void World::UpdateWorldConstantBuffer()
{
	const uint8_t waterType = BlockDefinition::GetBlockTypeIDByName("Water");

	Vec3 cameraWorldPos = m_gameCamera->m_position;
	IntVec3 cameraBlockCoords = GetBlockGlobalCoordsFromWorld(cameraWorldPos);
	BlockIterator cameraBlock = GetBlockIterFromGlobalCoords(cameraBlockCoords);

	Block* cameraBlockPtr = cameraBlock.GetBlock();

	if (cameraBlockPtr && cameraBlockPtr->GetTypeID() == waterType)
	{
		m_worldConstants.m_isUnderwater = 1.f;
	}
	else
	{
		m_worldConstants.m_isUnderwater = 0.f;
	}
	g_theRenderer->UpdateBuffer(*m_worldConstantsBuffer, sizeof(WorldConstants), &m_worldConstants);
}

void World::CreateWorldConstantBuffer()
{
	BufferInit initData;
	initData.m_isConstantBuffer = true;
	initData.m_size = sizeof(WorldConstants);

	m_worldConstantsBuffer = g_theRenderer->CreateBuffer(initData);

	m_worldConstantBufferCBV = g_theRenderer->AllocateConstantBufferView(*m_worldConstantsBuffer);
}

void World::DestroyWorldConstantBuffer()
{
	g_theRenderer->DestroyBuffer(m_worldConstantsBuffer);
	g_theRenderer->EnqueueDeferredRelease(m_worldConstantBufferCBV);
}

void World::ShowImGuiWindow()
{
	if (ImGui::Begin("World Settings"))
	{
		ImGui::Text(FormatWorldTime((float)m_worldClock->GetTotalSeconds()).c_str());

		ImGui::Separator();
		ImGui::Checkbox("Show Rain Particles", &m_showRainParticles);

		Vec3 velocity = m_rainSystem->GetVelocity();
		float velocityArray[3] = { velocity.x, velocity.y, velocity.z };

		if (ImGui::DragFloat3("Rain Velocity", velocityArray, 0.1f, -25.0f, 25.0f)) 
		{
			m_rainSystem->SetVelocity(Vec3(velocityArray[0], velocityArray[1], velocityArray[2]));
		}
		ImGui::Separator();

		ImGui::Checkbox("Simulate", &m_isSimulating);

		
		ImGui::BeginDisabled(!m_isSimulating);
		ImGui::SameLine();
		ImGui::Checkbox("Lightning", &m_hasLightning);
		ImGui::EndDisabled();


		ImGui::BeginDisabled(m_isSimulating);

		if (ImGui::CollapsingHeader("World Constants"))
		{
			RenderWorldConstantsUI(m_worldConstants);
		}

		ImGui::EndDisabled();



	}
	ImGui::End();
}

void World::UpdateDayNightSystem()
{
	bool isAccelerating = g_theInput->IsKeyDown(KEYCODE_Y);
	m_worldClock->SetTimeScale(isAccelerating ? TIME_ACCELERATION_FACTOR : 1.0);

	if (!m_isSimulating) return;

	float worldSeconds = (float)m_worldClock->GetTotalSeconds();
	float t = GetDayProgress(worldSeconds); // 0:00 ~ 23:59

	// Light Direction
	{
		float dayDegrees = t * 360.0f;

		float x = SinDegrees(dayDegrees);
		float y = 0.3f; // #ToDo a little offset, not right above
		float z = -CosDegrees(dayDegrees);

		m_worldConstants.m_lightDirection = Vec3(x, y, z).GetNormalized();
	}

	// Outdoor Lighting with Lightning
	{
		Rgba8 currentOutdoorLightColor = m_outDoorLightGradient.Evaluate(t);;

		if (m_hasLightning)
		{
			float lightningPerlin = Compute1dPerlinNoise(
				worldSeconds,
				1.0f,
				9, 
				0.5f, 
				2.0f, 
				true,
				7  
			);

			float lightningStrength = RangeMapClamped(lightningPerlin, 0.6f, 0.9f, 0.0f, 1.0f);

			if (lightningStrength > 0.f)
			{
				if (m_lastLightningSeconds + LIGHTNING_COOLDOWN_SECONDS < worldSeconds)
				{
					// Spawn A random lightning in world

					SpawnOneLightningStrike();
					m_lastLightningSeconds = worldSeconds;
				}
			}

			m_worldConstants.m_lightningIntensity = lightningStrength;

			currentOutdoorLightColor = Interpolate(currentOutdoorLightColor, Rgba8::OPAQUE_WHITE, lightningStrength);
			
			//currentOutdoorLightColor.GetAsFloats(m_worldConstants.m_skyColor);
		}

		currentOutdoorLightColor.GetAsFloats(m_worldConstants.m_outdoorLightColor);
	}

	// Indoor Lighting Glow
	{
		Rgba8 currentIndoorLightColor(255, 230, 204);

		float glowPerlin = Compute1dPerlinNoise(
			worldSeconds,
			1.5f,
			2,
			0.5f,
			2.0f,
			true,
			11
		);

		float glowStrength = RangeMapClamped(glowPerlin, -1.f, 1.f, 0.8f, 1.0f);

		currentIndoorLightColor.ScaleRGB(glowStrength);
		currentIndoorLightColor.GetAsFloats(m_worldConstants.m_indoorLightColor);
	}
}

BlockDebugInfo World::GetSelectedBlockInfo() const
{
	BlockDebugInfo result;

	if (m_raycastResult.m_didImpact && m_raycastResult.m_impactedBlockIter.IsValid() && m_raycastResult.m_previousBlockIter.IsValid())
	{
		result.m_isValid = true;
		result.m_type = m_raycastResult.m_impactedBlockIter.GetBlock()->GetTypeID();
		result.m_indoorLight = m_raycastResult.m_previousBlockIter.GetIndoorLightInfluence();
		result.m_outdoorLight = m_raycastResult.m_previousBlockIter.GetOutdoorLightInfluence();
	}

	return result;
}

BlockRaycastResult3D World::FastVoxelRaycast(Vec3 rayStart, Vec3 rayForwardNormal, float rayLength) const
{
	BlockRaycastResult3D result;
	result.m_rayStartPos = rayStart;
	result.m_rayFwdNormal = rayForwardNormal;
	result.m_rayLength = rayLength;

	IntVec3 currentBlockCoords = GetBlockGlobalCoordsFromWorld(rayStart);
	BlockIterator previousBlock = GetBlockIterFromGlobalCoords(currentBlockCoords);
	//IntVec3 previousBlockCoords = currentBlockCoords;

	if (!previousBlock.IsValid())
	{
		return result;
	}

	// ===== Setup 3D voxel traversal parameters =====
	// X axis
	float fwdDistPerXCrossing = 1.0f / fabsf(rayForwardNormal.x);
	int stepDirectionX = (rayForwardNormal.x < 0.0f) ? -1 : 1;
	float xAtFirstCrossing = static_cast<float>(currentBlockCoords.x) + static_cast<float>(stepDirectionX + 1) * 0.5f;
	float xDistToFirstCrossing = xAtFirstCrossing - rayStart.x;
	float fwdDistAtNextXCrossing = fabsf(xDistToFirstCrossing) * fwdDistPerXCrossing;

	// Y axis
	float fwdDistPerYCrossing = 1.0f / fabsf(rayForwardNormal.y);
	int stepDirectionY = (rayForwardNormal.y < 0.0f) ? -1 : 1;
	float yAtFirstCrossing = static_cast<float>(currentBlockCoords.y) + static_cast<float>(stepDirectionY + 1) * 0.5f;
	float yDistToFirstCrossing = yAtFirstCrossing - rayStart.y;
	float fwdDistAtNextYCrossing = fabsf(yDistToFirstCrossing) * fwdDistPerYCrossing;

	// Z axis
	float fwdDistPerZCrossing = 1.0f / fabsf(rayForwardNormal.z);
	int stepDirectionZ = (rayForwardNormal.z < 0.0f) ? -1 : 1;
	float zAtFirstCrossing = static_cast<float>(currentBlockCoords.z) + static_cast<float>(stepDirectionZ + 1) * 0.5f;
	float zDistToFirstCrossing = zAtFirstCrossing - rayStart.z;
	float fwdDistAtNextZCrossing = fabsf(zDistToFirstCrossing) * fwdDistPerZCrossing;

	// ===== Main traversal loop =====
	BlockIterator currentBlock = previousBlock;

	while (true)
	{
		BlockFace hitFace;
		float currentCrossingDist;

		// Step to the nearest block boundary
		if (fwdDistAtNextXCrossing <= fwdDistAtNextYCrossing && fwdDistAtNextXCrossing <= fwdDistAtNextZCrossing)
		{
			currentCrossingDist = fwdDistAtNextXCrossing;
			if (currentCrossingDist > rayLength)
			{
				result.m_impactDist = rayLength;
				return result;
			}

			if (stepDirectionX > 0)
			{
				currentBlock = currentBlock.GetEastNeighbor();
				hitFace = BlockFace::WEST;
			}
			else
			{
				currentBlock = currentBlock.GetWestNeighbor();
				hitFace = BlockFace::EAST;
			}
			fwdDistAtNextXCrossing += fwdDistPerXCrossing;
		}
		else if (fwdDistAtNextYCrossing <= fwdDistAtNextZCrossing)
		{
			currentCrossingDist = fwdDistAtNextYCrossing;
			if (currentCrossingDist > rayLength)
			{
				result.m_impactDist = rayLength;
				return result;
			}

			if (stepDirectionY > 0)
			{
				currentBlock = currentBlock.GetNorthNeighbor();
				hitFace = BlockFace::SOUTH;
			}
			else
			{
				currentBlock = currentBlock.GetSouthNeighbor();
				hitFace = BlockFace::NORTH;
			}
			fwdDistAtNextYCrossing += fwdDistPerYCrossing;
		}
		else
		{
			currentCrossingDist = fwdDistAtNextZCrossing;
			if (currentCrossingDist > rayLength)
			{
				result.m_impactDist = rayLength;
				return result;
			}

			if (stepDirectionZ > 0)
			{
				currentBlock = currentBlock.GetSkywardNeighbor();
				hitFace = BlockFace::DOWNWARD;
			}
			else
			{
				currentBlock = currentBlock.GetDownwardNeighbor();
				hitFace = BlockFace::SKYWARD;
			}
			fwdDistAtNextZCrossing += fwdDistPerZCrossing;
		}

		if (!currentBlock.IsValid())
		{
			result.m_impactDist = currentCrossingDist;
			return result;
		}

		// ===== Check for non-opaque to opaque transition =====
		Block* prevBlockPtr = previousBlock.GetBlock();
		Block* currBlockPtr = currentBlock.GetBlock();

		if (prevBlockPtr && currBlockPtr)
		{
			if (!prevBlockPtr->IsFullOpaque() && currBlockPtr->IsFullOpaque())
			{
				result.m_didImpact = true;
				result.m_impactDist = currentCrossingDist;
				result.m_impactPos = rayStart + rayForwardNormal * currentCrossingDist;
				result.m_impactFace = hitFace;
				result.m_impactedBlockIter = currentBlock;
				result.m_previousBlockIter = previousBlock;

				// ===== Calculate impact normal based on hit face =====
				switch (hitFace)
				{
				case BlockFace::EAST:     result.m_impactNormal = Vec3(1.0f, 0.0f, 0.0f); break;
				case BlockFace::WEST:     result.m_impactNormal = Vec3(-1.0f, 0.0f, 0.0f); break;
				case BlockFace::NORTH:    result.m_impactNormal = Vec3(0.0f, 1.0f, 0.0f); break;
				case BlockFace::SOUTH:    result.m_impactNormal = Vec3(0.0f, -1.0f, 0.0f); break;
				case BlockFace::SKYWARD:  result.m_impactNormal = Vec3(0.0f, 0.0f, 1.0f); break;
				case BlockFace::DOWNWARD: result.m_impactNormal = Vec3(0.0f, 0.0f, -1.0f); break;
				}

				return result;
			}
		}

		// Move to next block
		previousBlock = currentBlock;
	}

	return result;
}

BlockRaycastResult3D World::RaycastSolid(Vec3 rayStart, Vec3 rayForwardNormal, float rayLength) const
{
	BlockRaycastResult3D result;
	result.m_rayStartPos = rayStart;
	result.m_rayFwdNormal = rayForwardNormal;
	result.m_rayLength = rayLength;

	IntVec3 currentBlockCoords = GetBlockGlobalCoordsFromWorld(rayStart);
	BlockIterator previousBlock = GetBlockIterFromGlobalCoords(currentBlockCoords);

	if (!previousBlock.IsValid())
	{
		return result;
	}

	// ===== Setup 3D voxel traversal parameters =====
	// X axis
	float fwdDistPerXCrossing = 1.0f / fabsf(rayForwardNormal.x);
	int stepDirectionX = (rayForwardNormal.x < 0.0f) ? -1 : 1;
	float xAtFirstCrossing = static_cast<float>(currentBlockCoords.x) + static_cast<float>(stepDirectionX + 1) * 0.5f;
	float xDistToFirstCrossing = xAtFirstCrossing - rayStart.x;
	float fwdDistAtNextXCrossing = fabsf(xDistToFirstCrossing) * fwdDistPerXCrossing;

	// Y axis
	float fwdDistPerYCrossing = 1.0f / fabsf(rayForwardNormal.y);
	int stepDirectionY = (rayForwardNormal.y < 0.0f) ? -1 : 1;
	float yAtFirstCrossing = static_cast<float>(currentBlockCoords.y) + static_cast<float>(stepDirectionY + 1) * 0.5f;
	float yDistToFirstCrossing = yAtFirstCrossing - rayStart.y;
	float fwdDistAtNextYCrossing = fabsf(yDistToFirstCrossing) * fwdDistPerYCrossing;

	// Z axis
	float fwdDistPerZCrossing = 1.0f / fabsf(rayForwardNormal.z);
	int stepDirectionZ = (rayForwardNormal.z < 0.0f) ? -1 : 1;
	float zAtFirstCrossing = static_cast<float>(currentBlockCoords.z) + static_cast<float>(stepDirectionZ + 1) * 0.5f;
	float zDistToFirstCrossing = zAtFirstCrossing - rayStart.z;
	float fwdDistAtNextZCrossing = fabsf(zDistToFirstCrossing) * fwdDistPerZCrossing;

	// ===== Main traversal loop =====
	BlockIterator currentBlock = previousBlock;

	while (true)
	{
		BlockFace hitFace;
		float currentCrossingDist;

		// Step to the nearest block boundary
		if (fwdDistAtNextXCrossing <= fwdDistAtNextYCrossing && fwdDistAtNextXCrossing <= fwdDistAtNextZCrossing)
		{
			currentCrossingDist = fwdDistAtNextXCrossing;
			if (currentCrossingDist > rayLength)
			{
				result.m_impactDist = rayLength;
				return result;
			}

			if (stepDirectionX > 0)
			{
				currentBlock = currentBlock.GetEastNeighbor();
				hitFace = BlockFace::WEST;
			}
			else
			{
				currentBlock = currentBlock.GetWestNeighbor();
				hitFace = BlockFace::EAST;
			}
			fwdDistAtNextXCrossing += fwdDistPerXCrossing;
		}
		else if (fwdDistAtNextYCrossing <= fwdDistAtNextZCrossing)
		{
			currentCrossingDist = fwdDistAtNextYCrossing;
			if (currentCrossingDist > rayLength)
			{
				result.m_impactDist = rayLength;
				return result;
			}

			if (stepDirectionY > 0)
			{
				currentBlock = currentBlock.GetNorthNeighbor();
				hitFace = BlockFace::SOUTH;
			}
			else
			{
				currentBlock = currentBlock.GetSouthNeighbor();
				hitFace = BlockFace::NORTH;
			}
			fwdDistAtNextYCrossing += fwdDistPerYCrossing;
		}
		else
		{
			currentCrossingDist = fwdDistAtNextZCrossing;
			if (currentCrossingDist > rayLength)
			{
				result.m_impactDist = rayLength;
				return result;
			}

			if (stepDirectionZ > 0)
			{
				currentBlock = currentBlock.GetSkywardNeighbor();
				hitFace = BlockFace::DOWNWARD;
			}
			else
			{
				currentBlock = currentBlock.GetDownwardNeighbor();
				hitFace = BlockFace::SKYWARD;
			}
			fwdDistAtNextZCrossing += fwdDistPerZCrossing;
		}

		if (!currentBlock.IsValid())
		{
			result.m_impactDist = currentCrossingDist;
			return result;
		}

		// ===== Check if current block is solid =====
		Block* currBlockPtr = currentBlock.GetBlock();

		if (currBlockPtr&& currBlockPtr->IsSolid())
		{
			result.m_didImpact = true;
			result.m_impactDist = currentCrossingDist;
			result.m_impactPos = rayStart + rayForwardNormal * currentCrossingDist;
			result.m_impactFace = hitFace;
			result.m_impactedBlockIter = currentBlock;
			result.m_previousBlockIter = previousBlock;

			// ===== Calculate impact normal based on hit face =====
			switch (hitFace)
			{
			case BlockFace::EAST:     result.m_impactNormal = Vec3(1.0f, 0.0f, 0.0f); break;
			case BlockFace::WEST:     result.m_impactNormal = Vec3(-1.0f, 0.0f, 0.0f); break;
			case BlockFace::NORTH:    result.m_impactNormal = Vec3(0.0f, 1.0f, 0.0f); break;
			case BlockFace::SOUTH:    result.m_impactNormal = Vec3(0.0f, -1.0f, 0.0f); break;
			case BlockFace::SKYWARD:  result.m_impactNormal = Vec3(0.0f, 0.0f, 1.0f); break;
			case BlockFace::DOWNWARD: result.m_impactNormal = Vec3(0.0f, 0.0f, -1.0f); break;
			}

			return result;
		}

		// Move to next block
		previousBlock = currentBlock;
	}

	return result;
}

void World::UpdateRayAndDoRaycast()
{
	if (g_theInput->WasKeyJustPressed(KEYCODE_R))
	{
		m_isRaycastLocked = !m_isRaycastLocked;
	}

	bool hasRay = true;
	if (!m_isRaycastLocked)
	{
		hasRay = m_gameCamera->GetRay(m_rayStart, m_rayFwdNormal, m_rayLength);

		//m_rayStart = g_theGame->GetPlayerCameraPosition();
		//m_rayFwdNormal = g_theGame->GetPlayerCameraOrientation().GetAsMatrix_IFwd_JLeft_KUp().GetIBasis3D();
	}

	// Do Raycast
	m_raycastResult = {};

	if (hasRay)
	{
		m_raycastResult = FastVoxelRaycast(m_rayStart, m_rayFwdNormal, m_rayLength);
	}

	// Draw Raycast
	if (m_raycastResult.m_didImpact)
	{
		// Draw Hollow Square
		{
			IntVec2 chunkCoords = m_raycastResult.m_impactedBlockIter.m_chunk->GetChunkCoords();
			IntVec3 localCoords = GetBlockLocalCoordsFromIndex(m_raycastResult.m_impactedBlockIter.m_blockIndex);
			IntVec3 globalCoords = GetBlockGlobalCoordsFromChunk(chunkCoords, localCoords);
			Vec3 blockWorldCenter = Vec3(globalCoords) + Vec3(0.5f, 0.5f, 0.5f);

			std::vector<Vertex_PCU> verts;
			verts.reserve(4 * 6 * 6);

			// +x Square, Rotate, Translate
			constexpr float THICKNESS = 0.05f;
			AddVertsForAABB3D(verts, AABB3(Vec3(0.5f, -0.5f, -0.5f), Vec3(0.5f, -0.5f, -0.5f) + Vec3(THICKNESS, THICKNESS, 1.f)));
			AddVertsForAABB3D(verts, AABB3(Vec3(0.5f, -0.5f, -0.5f), Vec3(0.5f, -0.5f, -0.5f) + Vec3(THICKNESS, 1.f, THICKNESS)));
			AddVertsForAABB3D(verts, AABB3(Vec3(0.5f, 0.5f - THICKNESS, -0.5f), Vec3(0.5f, 0.5f - THICKNESS, -0.5f) + Vec3(THICKNESS, THICKNESS, 1.f)));
			AddVertsForAABB3D(verts, AABB3(Vec3(0.5f, -0.5f, 0.5f - THICKNESS), Vec3(0.5f, -0.5f, 0.5f - THICKNESS) + Vec3(THICKNESS, 1.f, THICKNESS)));

			Mat44 squareTransform = Mat44::MakeFromX(m_raycastResult.m_impactNormal); // #ToDo Face is more accurate?
			squareTransform.SetTranslation3D(blockWorldCenter);

			TransformVertexArray3D(verts, squareTransform);

			DebugAddWorldTriangleList(verts, 0.f, Rgba8::CYAN, Rgba8::CYAN, DebugRenderMode::X_RAY);
		}

		if (m_isRaycastLocked)
		{
			DebugAddWorldArrow(m_rayStart, m_rayStart + m_rayFwdNormal * m_rayLength, 0.009f, 0.f, Rgba8(85, 85, 85));
			DebugAddWorldArrow(m_rayStart, m_rayStart + m_rayFwdNormal * m_raycastResult.m_impactDist, 0.01f, 0.f, Rgba8::RED);
		}
	}
	else
	{
		if (m_isRaycastLocked)
		{
			DebugAddWorldArrow(m_rayStart, m_rayStart + m_rayFwdNormal * m_rayLength, 0.01f, 0.f, Rgba8::GREEN);
		}
	}


}

Camera World::GetWorldCamera() const
{
	return m_gameCamera->m_camera;
}

std::string World::GetCameraModeName() const
{
	return m_gameCamera->GetCameraModeName();
}

std::string World::GetPlayerPhysicsModeName() const
{
	return m_player->GetPhysicsModeName();
}

void World::RefreshAspectRatio()
{
	m_gameCamera->RefreshAspectRatio();
}

Vec3 World::GetPlayerCameraPosition() const
{
	if (m_isPlayerCameraLocked)
	{
		return m_lockedPlayerCamera.GetPosition();
	}

	return m_gameCamera->m_position;
}

Rgba8 World::GetSkyColor() const
{
	return Rgba8(DenormalizeByte(m_worldConstants.m_skyColor[0]),
		DenormalizeByte(m_worldConstants.m_skyColor[1]),
		DenormalizeByte(m_worldConstants.m_skyColor[2]),
		DenormalizeByte(m_worldConstants.m_skyColor[3]));
}

Frustum World::GetPlayerCameraFrustum() const
{
	if (m_isPlayerCameraLocked)
	{
		return m_lockedPlayerCamera.GetFrustum();
	}
	return m_gameCamera->m_camera.GetFrustum();
}

EulerAngles World::GetPlayerCameraOrientation() const
{
	if (m_isPlayerCameraLocked)
	{
		return m_lockedPlayerCamera.GetOrientation();
	}
	return m_gameCamera->m_orientation;
}

void World::RenderEmissiveShapes() const
{
	std::vector<Vertex_PCU> verts;
	AddVertsForCylinder3D(verts, Vec3::ZERO, Vec3(16.f, 16.f, 128.f), 0.1f, Rgba8(252, 192, 30), AABB2::ZERO_TO_ONE, 3);

	UnlitEmissiveResources resources;
	resources.diffuseTextureIndex = g_theRenderer->GetSrvIndexFromLoadedTexture(nullptr, DefaultTexture::WhiteOpaque2D);
	resources.diffuseSamplerIndex = g_theRenderer->GetDefaultSamplerIndex(SamplerMode::POINT_CLAMP);
	resources.cameraConstantsIndex = g_theRenderer->GetCurrentCameraConstantsIndex();
	resources.modelConstantsIndex = g_theRenderer->GetCurrentModelConstantsIndex();

	resources.emissiveTextureIndex = g_theRenderer->GetSrvIndexFromLoadedTexture(nullptr, DefaultTexture::WhiteOpaque2D);


	resources.emissiveStrength = (SinDegrees(30.f * static_cast<float>(m_worldClock->GetTotalSeconds())) * 1.f + 1.5f);

	g_theRenderer->SetGraphicsBindlessResources(sizeof(UnlitEmissiveResources), &resources);

	g_theRenderer->BindShader(g_theGame->m_unlitEmissiveShader);
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);

	g_theRenderer->DrawVertexArray(verts);
}

void World::SpawnOneLightningStrike()
{
	constexpr float START_Z = 140.f;
	constexpr float END_Z = 64.5f;
	constexpr float MIN_DIST = 32.f;
	constexpr float MAX_DIST = 64.f;
	constexpr float HALF_APERTURE = 45.f;

	Vec3 camPos = m_gameCamera->m_position;
	float camYawDegrees = m_gameCamera->m_orientation.m_yawDegrees;

	RandomNumberGenerator rng;
	float radius = rng.RollRandomFloatInRange(MIN_DIST, MAX_DIST);
	float yawDegrees = camYawDegrees + rng.RollRandomFloatInRange(-HALF_APERTURE, HALF_APERTURE);

	Vec2 location = Vec2(camPos.x, camPos.y) + Vec2::MakeFromPolarDegrees(yawDegrees, radius);

	m_lightningStrikeSystem->SpawnLightningStrike(Vec3(location.x, location.y, START_Z), Vec3(location.x, location.y, END_Z));

}

void GenerateChunkJob::Execute()
{
	if (IsCancelled())
	{
		return;
	}

	m_chunk->m_state = ChunkState::ACTIVATING_GENERATING;

	m_chunk->GenerateBlocks();


	m_chunk->m_state = ChunkState::ACTIVATING_GENERATE_COMPLETE;
}

void LoadChunkJob::Execute()
{
	if (IsCancelled())
	{
		return;
	}

	m_chunk->m_state = ChunkState::ACTIVATING_LOADING;

	m_chunk->LoadFromDisk();

	m_chunk->m_state = ChunkState::ACTIVATING_LOAD_COMPLETE;
}

void SaveChunkJob::Execute()
{
	if (IsCancelled())
	{
		return;
	}

	m_chunk->m_state = ChunkState::DEACTIVATING_SAVING;

	m_chunk->SaveToDisk();

	m_chunk->m_state = ChunkState::DEACTIVATING_SAVE_COMPLETE;
}

WorldConstants::WorldConstants()
{
	// Default Settings
	Rgba8 const INDOOR_LIGHT(255, 230, 204);
	Rgba8 const OUTDOOR_LIGHT(255, 255, 255);
	Rgba8 const SKY_LIGHT(0, 0, 0);
	
	INDOOR_LIGHT.GetAsFloats(m_indoorLightColor);
	OUTDOOR_LIGHT.GetAsFloats(m_outdoorLightColor);
	SKY_LIGHT.GetAsFloats(m_skyColor);

	m_fogFarDistance = static_cast<float>(CHUNK_ACTIVATION_RANGE - 2 * CHUNK_SIZE_X); // 256
	m_fogNearDistance = m_fogFarDistance * 0.5f; // 128

	m_waterSurfaceHeight = 64.f;
	m_isUnderwater = 1.f;

	m_waterScatterColorShallow[0] = 0.2f;
	m_waterScatterColorShallow[1] = 0.6f;
	m_waterScatterColorShallow[2] = 0.9f;
	m_waterScatterColorShallow[3] = 1.0f;

	m_waterScatterColorDeep[0] = 0.0f;
	m_waterScatterColorDeep[1] = 0.15f;
	m_waterScatterColorDeep[2] = 0.35f;
	m_waterScatterColorDeep[3] = 1.0f;

	float visibility = 48.0f;
	float baseAbsorption = 2.0f / visibility;  // exp(-2) = 13.5%

	m_waterAbsorptionR = baseAbsorption * 4.0f;  // 0.5   Red Light (Quick)
	m_waterAbsorptionG = baseAbsorption * 2.0f;  // 0.25  Green Light
	m_waterAbsorptionB = baseAbsorption * 1.0f;  // 0.125 Blue Light (Slow)
	m_waterVisibilityRange = visibility;

	m_waterDepthTransitionStart = 8.f;	// Start Darkening
	m_waterDepthTransitionEnd = 32.f;	// Fully Dark Blue

	m_lightDirection = Vec3(1.f, 0.f, 0.f);
}

void RenderWorldConstantsUI(WorldConstants& worldConstants)
{
	ImGui::Text("Lighting");
	ImGui::Separator();
	ImGui::ColorEdit4("Indoor Light Color", worldConstants.m_indoorLightColor);
	ImGui::ColorEdit4("Outdoor Light Color", worldConstants.m_outdoorLightColor);

	ImGui::Spacing();

	//ImGui::Text("Sky & Background");
	//ImGui::Separator();
	//ImGui::ColorEdit4("Sky Color (Clear Color)", worldConstants.m_skyColor);

	ImGui::Spacing();

	ImGui::Text("Fog Settings");
	ImGui::Separator();

	ImGui::SliderFloat("Fog Strength", &worldConstants.m_skyColor[3], 0.0f, 1.0f);
	ImGui::SliderFloat("Fog Near Distance", &worldConstants.m_fogNearDistance, 0.0f, 500.0f);
	ImGui::SliderFloat("Fog Far Distance", &worldConstants.m_fogFarDistance, 0.0f, 500.0f);

	ImGui::Text("Fog Range: %.1f - %.1f", worldConstants.m_fogNearDistance, worldConstants.m_fogFarDistance);

	ImGui::Spacing();

	if (ImGui::Button("Reset to Default"))
	{
		worldConstants = WorldConstants();
	}
}
