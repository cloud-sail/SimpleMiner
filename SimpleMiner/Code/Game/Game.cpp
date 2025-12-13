#include "Game/Game.hpp"
#include "Game/BloomEffect.hpp"
#include "Game/App.hpp"
#include "Game/World.hpp"
#include "Game/Chunk.hpp"
#include "Game/BlockDefinition.hpp"
#include "Game/SpectatorCamera.hpp"
#include "Game/ChunkUtils.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/DebugRender.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/Frustum.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/SpriteSheet.hpp"
#include "Engine/Window/Window.hpp"
#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/implot/implot.h"

Texture* g_blockTexture = nullptr;
SpriteSheet* g_blockSpriteSheet = nullptr;

//-----------------------------------------------------------------------------------------------
Game::Game()
{
	m_clock = new Clock();

	//m_spectator = new SpectatorCamera();
	m_cursorMode = CursorMode::POINTER;

	DebugDrawStartup();
	// Initialization
	InitializeBlockTexture();
	BlockDefinition::InitializeDefinitions(g_gameConfigBlackboard.GetValue("blockDefinitionsPath", "UNKNOWN_PATH").c_str());
	TreeGenerator::InitializeTreeStamps();
	m_world = new World(this);

	m_ditherShader = g_theRenderer->CreateOrGetShader(ShaderConfig("Data/Shaders/DitherFade"), VertexType::VERTEX_PCU);
	m_unlitEmissiveShader = g_theRenderer->CreateOrGetShader(ShaderConfig("Data/Shaders/UnlitEmissive"), VertexType::VERTEX_PCU);

	Spline1D initialCurve;
	initialCurve.SetFromCatmullRomAlgorithm({ 0.f, 0.5f, 1.0f, 0.8f, 0.2f });
	m_curveEditor.SetSpline(initialCurve);

	m_skyGradient = Gradient::MakeSkyGradient();
}

Game::~Game()
{
	delete m_clock;
	m_clock = nullptr;

	//delete m_spectator;
	//m_spectator = nullptr;

	delete g_blockSpriteSheet;
	g_blockSpriteSheet = nullptr;

	delete m_world;
	m_world = nullptr;

	DebugRenderClear();
}

void Game::Update()
{
	UpdateScreenDimensions();
	UpdateDeveloperCheats();

	ShowMainImGuiWindow();

	// Attract Mode
	if (m_isAttractMode)
	{
		UpdateAttractMode();
		m_cursorMode = CursorMode::POINTER;
		return;
	}
	m_cursorMode = CursorMode::FPS; // Default Settings
	// FIXED_ANGLE_TRACKING is using pointer

	// Game Mode
	m_world->Update();


	UpdateCameras();
	DebugDrawUpdate();

	// return to attract mode
	XboxController const& controller = g_theInput->GetController(0);
	if (g_theInput->WasKeyJustPressed(KEYCODE_ESCAPE) || 
		controller.WasButtonJustPressed(XboxButtonId::XBOX_BUTTON_BACK))
	{
		m_isAttractMode = true;
	}
}

void Game::Render() const
{
	if (m_isAttractMode)
	{
		RenderAttractMode();
		return;
	}

	m_world->Render();

	DebugRenderWorld(m_world->GetWorldCamera());

	// Render Screen
	g_theRenderer->BeginCamera(m_screenCamera);
	// RenderUI();
	g_theRenderer->EndCamera(m_screenCamera);
	DebugRenderScreen(m_screenCamera);


	//// Render World (Spectator Camera, need to be replaced by "Gameplay" camera)
	//g_theRenderer->BeginCamera(m_world->GetWorldCamera());
	//m_world->Render();
	//g_theRenderer->EndCamera(m_world->GetWorldCamera());
	//DebugRenderWorld(m_world->GetWorldCamera());

	//// Render Screen
	//g_theRenderer->BeginCamera(m_screenCamera);
	//// RenderUI();
	//g_theRenderer->EndCamera(m_screenCamera);
	//DebugRenderScreen(m_screenCamera);
}

void Game::OnWindowResized()
{
	m_world->OnWindowResized();
}

void Game::UpdateDeveloperCheats()
{
	if (g_theInput->WasKeyJustPressed(KEYCODE_F1))
	{
		g_isDebugDraw = !g_isDebugDraw;
	}
	if (g_theInput->WasKeyJustPressed(KEYCODE_F2))
	{
		m_debugDrawChunk = !m_debugDrawChunk;
	}
	if (g_theInput->WasKeyJustPressed(KEYCODE_F3))
	{
		m_debugDrawJobSystem = !m_debugDrawJobSystem;
	}

	bool isSlowMo = g_theInput->IsKeyDown(KEYCODE_T);
	m_clock->SetTimeScale(isSlowMo ? 0.1 : 1.0);

	if (g_theInput->WasKeyJustPressed(KEYCODE_P))
	{
		m_clock->TogglePause();
	}
	if (g_theInput->WasKeyJustPressed(KEYCODE_O))
	{
		m_clock->StepSingleFrame();
	}
	if (g_theInput->WasKeyJustPressed(KEYCODE_F11))
	{
		Window::s_mainWindow->ToggleFullscreen();
	}
}

float Game::GetDeltaSeconds() const
{
	return (float)m_clock->GetDeltaSeconds();
}

void Game::UpdateCameras()
{
	// Screen camera (for UI, HUD, Attract, etc.)
	m_screenCamera.SetOrthographicView(Vec2(0.f, 0.f), Vec2(g_screenWidth, g_screenHeight));
}

void Game::InitializeBlockTexture()
{
	g_blockTexture = g_theRenderer->CreateOrGetTextureFromFile(g_gameConfigBlackboard.GetValue("blockSpriteSheetPath", "UNKNOWN_PATH").c_str());
	g_blockSpriteSheet = new SpriteSheet(*g_blockTexture, g_gameConfigBlackboard.GetValue("blockSpriteSheetCellCount", IntVec2(8, 8)));
}

void Game::ShowMainImGuiWindow()
{
	if (ImGui::Begin("World Generation Settings"))
	{
		
		if (ImGui::CollapsingHeader("Debug"))
		{
			RenderDebugCombo();
		}
	}
	ImGui::End();
}

void Game::RenderDebugCombo()
{
	const char* items[] = {
		"No Debug",
		"Continent",
		"Erosion",
		"Peak and Valley",
		"Temperature",
		"Humidity"
	};

	if (ImGui::BeginCombo("Debug Layer", items[m_debugLayerMode]))
	{
		if (ImGui::Selectable("No Debug", m_debugLayerMode == Chunk::NO_DEBUG_LAYER))
			m_debugLayerMode = Chunk::NO_DEBUG_LAYER;
		if (ImGui::Selectable("Continent", m_debugLayerMode == Chunk::CONTINENT))
			m_debugLayerMode = Chunk::CONTINENT;
		if (ImGui::Selectable("Erosion", m_debugLayerMode == Chunk::EROSION))
			m_debugLayerMode = Chunk::EROSION;
		if (ImGui::Selectable("Peak and Valley", m_debugLayerMode == Chunk::PEAK_AND_VALLEY))
			m_debugLayerMode = Chunk::PEAK_AND_VALLEY;
		if (ImGui::Selectable("Temperature", m_debugLayerMode == Chunk::TEMPERATURE))
			m_debugLayerMode = Chunk::TEMPERATURE;
		if (ImGui::Selectable("Humidity", m_debugLayerMode == Chunk::HUMIDITY))
			m_debugLayerMode = Chunk::HUMIDITY;

		ImGui::EndCombo();
	}
}

void Game::UpdateAttractMode()
{
	// Update screenCamera in attract mode

	m_screenCamera.SetOrthographicView(Vec2(0.f, 0.f), Vec2(g_screenWidth, g_screenHeight));

	XboxController const& controller = g_theInput->GetController(0);
	if (g_theInput->WasKeyJustPressed(KEYCODE_ESCAPE) ||
		controller.WasButtonJustPressed(XboxButtonId::XBOX_BUTTON_BACK))
	{
		g_theApp->HandleQuitRequested();
	}

	if (g_theInput->WasKeyJustPressed(KEYCODE_SPACE) ||
		controller.WasButtonJustPressed(XboxButtonId::XBOX_BUTTON_A) ||
		controller.WasButtonJustPressed(XboxButtonId::XBOX_BUTTON_START))
	{
		m_isAttractMode = false;
		return;
	}


#pragma region TestPlot

	m_curveEditor.RenderUI("My Curve Editor");

	if (ImGui::Begin("Curve Visualization"))
	{
		m_curveEditor.RenderPlot("Animation Curve", Vec2(-1.f, 400.f));
	}
	ImGui::End();
#pragma endregion
}

void Game::RenderAttractMode() const
{
	g_theRenderer->BeginCamera(m_screenCamera);

	Vec2 camBottomLeft = m_screenCamera.GetOrthographicBottomLeft();
	Vec2 camTopLeft = m_screenCamera.GetOrthographicTopRight();

	std::vector<Vertex_PCU> verts;
	// Ring
	Vec2 const center = 0.5f * (camBottomLeft + camTopLeft);

	AddVertsForRing2D(verts, center, 300.f, 10.f, Rgba8(255, 127, 0));

	float totalSeconds = (float)m_clock->GetTotalSeconds();
	constexpr float PERIOD = 12.f;
	float t = fmodf(totalSeconds, PERIOD) / PERIOD;

	AddVertsForDisc2D(verts, center, 250.f, m_skyGradient.Evaluate(t));


	DebugAddScreenText(Stringf("%.2f", t * 24.f), AABB2(center - Vec2(100.f, 100.f), center + Vec2(100.f, 100.f)),
		100.f, Vec2(0.5f, 0.5f), 0.f, 0.5f);


	// resource settings
	UnlitRenderResources resources;
	resources.diffuseTextureIndex = g_theRenderer->GetSrvIndexFromLoadedTexture(nullptr, DefaultTexture::WhiteOpaque2D);
	resources.diffuseSamplerIndex = g_theRenderer->GetDefaultSamplerIndex(SamplerMode::POINT_CLAMP);
	resources.cameraConstantsIndex = g_theRenderer->GetCurrentCameraConstantsIndex();
	resources.modelConstantsIndex = g_theRenderer->GetCurrentModelConstantsIndex();

	g_theRenderer->SetGraphicsBindlessResources(sizeof(UnlitRenderResources), &resources);

	g_theRenderer->BindShader(nullptr);
	g_theRenderer->SetBlendMode(BlendMode::ALPHA);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetDepthMode(DepthMode::DISABLED);
	g_theRenderer->SetRenderTargetFormats();

	g_theRenderer->DrawVertexArray(verts);

	g_theRenderer->EndCamera(m_screenCamera);


	DebugRenderScreen(m_screenCamera);
}

void Game::DebugDrawStartup()
{
	constexpr float CELL_ASPECT = 0.9f;
	constexpr float TEXT_HEIGHT = 0.2f;
	constexpr float ORIGIN_OFFSET = 0.15f;
	DebugAddWorldBasis(Mat44(), -1.f);
	DebugAddWorldText("x - forward", Mat44(Vec3(0.f, -1.f, 0.f), Vec3(1.f, 0.f, 0.f), Vec3(0.f, 0.f, 1.f), Vec3(ORIGIN_OFFSET, 0.f, ORIGIN_OFFSET)), TEXT_HEIGHT, -1.f, CELL_ASPECT, Vec2::ZERO, Rgba8::RED);
	DebugAddWorldText("y - left", Mat44(Vec3(-1.f, 0.f, 0.f), Vec3(0.f, -1.f, 0.f), Vec3(0.f, 0.f, 1.f), Vec3(0.f, ORIGIN_OFFSET, ORIGIN_OFFSET)), TEXT_HEIGHT, -1.f, CELL_ASPECT, Vec2(1.f, 0.f), Rgba8::GREEN);
	DebugAddWorldText("z - up", Mat44(Vec3(-1.f, 0.f, 0.f), Vec3(0.f, 0.f, 1.f), Vec3(0.f, 1.f, 0.f), Vec3(0.f, -ORIGIN_OFFSET, ORIGIN_OFFSET)), TEXT_HEIGHT, -1.f, CELL_ASPECT, Vec2(0.f, 1.f), Rgba8::BLUE);
}

void Game::DebugDrawUpdate()
{
	// Camera Frustum
	//if (m_isPlayerCameraLocked)
	//{
	//	m_lockedPlayerCamera.DebugDrawFrustum();
	//}


	// Game Clock Data
	float totalSeconds = (float)m_clock->GetTotalSeconds();
	float frameRate = (float)m_clock->GetFrameRate();

	Vec2 camBottomLeft = m_screenCamera.GetOrthographicBottomLeft();
	Vec2 camTopLeft = m_screenCamera.GetOrthographicTopRight();

	AABB2 screenBox = AABB2(camBottomLeft, camTopLeft);

	// Relative Position
	AABB2 topRightRegion = AABB2(0.85f, 0.8f, 0.99f, 0.99f); // top-right
	AABB2 topLeftRegion = AABB2(0.01f, 0.95f, 0.3f, 0.99f); // top-left
	AABB2 topMiddleLeftRegion = AABB2(0.31f, 0.8f, 0.57f, 0.99f); // top middle-left
	AABB2 topMiddleRightRegion = AABB2(0.58f, 0.8f, 0.84f, 0.99f); // top middle-right
	AABB2 debugChunkRegion = AABB2(0.01f, 0.80f, 0.49f, 0.94f);
	AABB2 debugJobSystemRegion = AABB2(0.51f, 0.80f, 0.74f, 0.94f);
	AABB2 debugWorldJobRegion = AABB2(0.76f, 0.80f, 0.99f, 0.94f);

	auto makeSubRegion = [](const AABB2& box, const AABB2& uvRegion) -> AABB2 {
		return AABB2(
			box.GetPointAtUV(uvRegion.m_mins),
			box.GetPointAtUV(uvRegion.m_maxs)
		);
		};

	constexpr float fontAspectRatio = 0.6f;
	constexpr float fontSize = 20.f;

	// Top-right
	if (frameRate == 0.f)
	{
		DebugAddScreenText(Stringf("Time: %.2f FPS: inf", totalSeconds),
			makeSubRegion(screenBox, topRightRegion),
			fontSize, Vec2(1.f, 1.f), 0.f, fontAspectRatio);
	}
	else
	{
		DebugAddScreenText(Stringf("Time: %.2f FPS: %6.02f", totalSeconds, frameRate),
			makeSubRegion(screenBox, topRightRegion),
			fontSize, Vec2(1.f, 1.f), 0.f, fontAspectRatio);
	}

	// Top-left: instructions for digging
	DebugAddScreenText(Stringf("[LMB] Dig [RMB] Add %s\n[1] %s [2] %s [3] %s", 
		BlockDefinition::GetByType(m_world->m_blockToBePlaced)->m_name.c_str(), m_world->m_blockLists[0].c_str(), m_world->m_blockLists[1].c_str(), m_world->m_blockLists[2].c_str()),
		makeSubRegion(screenBox, topLeftRegion),
		fontSize, Vec2(0.f, 1.f), 0.f, fontAspectRatio);


	// Top middle-left: current camera mode 
	DebugAddScreenText(Stringf("[C] Camera: %s\n[V] Player Physics Mode: %s\n[F] %s the camera", 
		m_world->GetCameraModeName().c_str(), m_world->GetPlayerPhysicsModeName().c_str(), m_world->IsPlayerCameraLocked() ? "Unlock" : "Lock"),
		makeSubRegion(screenBox, topMiddleLeftRegion),
		fontSize, Vec2(0.f, 1.f), 0.f, fontAspectRatio);

	// Top middle-right: current selected block type 
	BlockDebugInfo blockInfo = m_world->GetSelectedBlockInfo();
	if (blockInfo.m_isValid)
	{
		BlockDefinition const* blockDef = BlockDefinition::GetByType(blockInfo.m_type);
		DebugAddScreenText(Stringf("Block Type: %s\nIndoor Light: %d\nOutdoor Light: %d", blockDef->m_name.c_str(), blockInfo.m_indoorLight, blockInfo.m_outdoorLight), 
			makeSubRegion(screenBox, topMiddleRightRegion),
			fontSize, Vec2(0.f, 1.f), 0.f, fontAspectRatio);
	}
	else
	{
		DebugAddScreenText("No block selected",
			makeSubRegion(screenBox, topMiddleRightRegion),
			fontSize, Vec2(0.f, 1.f), 0.f, fontAspectRatio);
	}



	if (m_debugDrawChunk)
	{
		Vec3 playerPos = m_world->GetPlayerCameraPosition();
		IntVec3 playerGlobalCoords = GetBlockGlobalCoordsFromWorld(playerPos);

		IntVec2 playerChunkCoords = GetChunkCoordsFromBlockGlobal(playerGlobalCoords);
		IntVec3 playerLocalCoords = GetBlockLocalCoordsFromGlobal(playerGlobalCoords);

		DebugAddScreenText(Stringf("Position: ( %.2f, %.2f, %.2f )\nChunkCoords: ( %d, %d )\nLocalCoords: ( %d, %d, %d )\nActive Chunks: %d/%d Vertices:%d Indices:%d", 
			playerPos.x, playerPos.y, playerPos.z, playerChunkCoords.x, playerChunkCoords.y, playerLocalCoords.x, playerLocalCoords.y, playerLocalCoords.z,
			m_world->GetActiveChunkCount(), MAX_ACTIVE_CHUNKS, m_world->GetTotalVertexNum(), m_world->GetTotalIndexNum()),
			makeSubRegion(screenBox, debugChunkRegion),
			fontSize, Vec2(0.f, 1.f), 0.f, fontAspectRatio, Rgba8::YELLOW);
	}



	if (m_debugDrawJobSystem)
	{
		JobSystemStatus jobSystemStatus = g_theJobSystem->GetJobSystemStatus();
		
		DebugAddScreenText(Stringf("[JobSystem]\npendingJobs: %12d\nexecutingJobs: %10d\ncompletedJobs: %10d\nprocessedJobs: %10d", 
			jobSystemStatus.m_pendingJobCount, jobSystemStatus.m_executingJobCount, jobSystemStatus.m_completedJobCount, jobSystemStatus.m_totalProcessedJobCount),
			makeSubRegion(screenBox, debugJobSystemRegion),
			fontSize, Vec2(0.f, 1.f), 0.f, fontAspectRatio, Rgba8::CYAN);

		DebugAddScreenText(Stringf("[World]\nchunkGenerating: %8d\nchunkLoading: %11d\nchunkSaving: %12d",
			m_world->GetGeneratingChunkCount(), m_world->GetLoadingChunkCount(), m_world->GetSavingChunkCount()),
			makeSubRegion(screenBox, debugWorldJobRegion),
			fontSize, Vec2(0.f, 1.f), 0.f, fontAspectRatio, Rgba8(200, 0, 200));

	}

}
