#include "Game/App.hpp"
#include "Game/Game.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Core/DevConsole.hpp"
#include "Engine/Core/EventSystem.hpp"
#include "Engine/Core/JobSystem.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Core/DebugRender.hpp"
#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Window/Window.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Renderer/DX12Renderer.hpp"


//-----------------------------------------------------------------------------------------------
App*			g_theApp		= nullptr;		// Created and owned by Main_Windows.cpp
Window*			g_theWindow		= nullptr;		// Created and owned by the App
Renderer*		g_theRenderer	= nullptr;		// Created and owned by the App
AudioSystem*	g_theAudio		= nullptr;		// Created and owned by the App
JobSystem*		g_theJobSystem	= nullptr;		// Created and owned by the App
Game*			g_theGame		= nullptr;		// Created and owned by the App
bool			g_isDebugDraw	= false;

//-----------------------------------------------------------------------------------------------
bool OnQuitEvent(EventArgs& args)
{
	UNUSED(args);
	g_theApp->HandleQuitRequested();
	return false;
}

bool OnWindowResized(EventArgs& args)
{
	UNUSED(args);
	g_theApp->HandleWindowResized();
	return true;
}

//-----------------------------------------------------------------------------------------------
App::App()
{
}

App::~App()
{
}

void App::Startup()
{
	// Parse Data/GameConfig.xml
	LoadGameConfig("Data/GameConfig.xml");

	// Create all Engine subsystems
	EventSystemConfig eventSystemConfig;
	g_theEventSystem = new EventSystem(eventSystemConfig);

	InputConfig inputConfig;
	g_theInput = new InputSystem(inputConfig);

	WindowConfig windowConfig;
	windowConfig.m_aspectRatio = g_gameConfigBlackboard.GetValue("windowAspect", 1.777f);
	windowConfig.m_windowTitle = g_gameConfigBlackboard.GetValue("windowTitle", "Simple Miner");
	windowConfig.m_isInitialFullscreen = g_gameConfigBlackboard.GetValue("windowFullscreen", false);
	g_theWindow = new Window(windowConfig);

	RendererConfig rendererConfig;
	rendererConfig.m_window = g_theWindow;
	g_theRenderer = new DX12Renderer(rendererConfig);

	DevConsoleConfig devConsoleConfig;
	devConsoleConfig.m_defaultRenderer = g_theRenderer;
	devConsoleConfig.m_fontName = g_gameConfigBlackboard.GetValue("fontName", devConsoleConfig.m_fontName);
	devConsoleConfig.m_fontAspectScale = 0.6f;
	g_theDevConsole = new DevConsole(devConsoleConfig);

	AudioConfig audioConfig;
	g_theAudio = new AudioSystem(audioConfig);

	JobSystemConfig jobSystemConfig;
	jobSystemConfig.numGenericWorkers = 14;
	jobSystemConfig.numFileIOWorkers = 2;
	g_theJobSystem = new JobSystem(jobSystemConfig);

	DebugRenderConfig debugRenderConfig;
	debugRenderConfig.m_renderer = g_theRenderer;
	debugRenderConfig.m_fontName = g_gameConfigBlackboard.GetValue("fontName", debugRenderConfig.m_fontName);
	debugRenderConfig.m_messageCellHeight = 15.f;
	debugRenderConfig.m_messageAspectRatio = 0.75f;

	// Start up all Engine subsystems
	g_theEventSystem->Startup();
	g_theDevConsole->Startup();
	g_theInput->Startup();
	g_theWindow->Startup();
	g_theRenderer->Startup();
	g_theAudio->Startup();
	g_theJobSystem->Startup();
	DebugRenderSystemStartup(debugRenderConfig);

	// Initialize game-related stuff: create and start the game
	g_theEventSystem->SubscribeEventCallbackFunction("Quit", OnQuitEvent);
	g_theEventSystem->SubscribeEventCallbackFunction(WINDOW_RESIZE_EVENT, OnWindowResized);
	g_theGame = new Game();

	Clock::GetSystemClock().SetMinDeltaSeconds(1.0 / 240.0);
	g_theDevConsole->AddText(DevConsole::INFO_MAJOR, "Controls");
	g_theDevConsole->AddText(DevConsole::INFO_MINOR, R"(Mouse and KeyBoard:
Mouse  - Aim
W / S  - Move
A / D  - Strafe
Q / E  - Elevate
Shift  - Sprint
H      - Reset Camera
C      - Cycle Camera Mode
V      - Cycle player physics mode
LMB    - Dig a block
RMB    - Place a block
1/2/3  - Choose the placed block
F2     - Toggle Chunk Debug Draw
F3     - Toggle JobSystem Debug Draw
F8     - Reload Game
Y      - Accelerate world time
R      - Toggle raycast locking
Space  - Jump
L      - Spawn a lightning bolt


GamePad:
Right Stick   - Aim
Left Stick    - Move
L/R Shoulder  - Elevate
L/R Trigger   - Sprint
Start         - Reset Camera
Button X      - Dig a block
Button Y      - Place a block
Button A      - Jump
Button B      - Cycle player physics mode
Dpad Up       - Cycle Camera Mode
Dpad L/Down/R - Choose the placed block


F      - Lock/Unlock Player Camera
F11    - Toggle Fullscreen
Escape - Exit Game
Space  - Start Game
)");

}

void App::Shutdown()
{
	// Destroy game-related stuff
	delete g_theGame;
	g_theGame = nullptr;

	// Shut down all Engine subsystems
	DebugRenderSystemShutdown();
	g_theJobSystem->Shutdown();
	g_theAudio->Shutdown();
	g_theRenderer->Shutdown();
	g_theWindow->Shutdown();
	g_theInput->Shutdown();
	g_theDevConsole->Shutdown();
	g_theEventSystem->Shutdown();

	// Destroy all engine subsystems
	delete g_theAudio;
	g_theAudio = nullptr;
	delete g_theJobSystem;
	g_theJobSystem = nullptr;
	delete g_theDevConsole;
	g_theDevConsole = nullptr;
	delete g_theRenderer;
	g_theRenderer = nullptr;
	delete g_theWindow;
	g_theWindow = nullptr;
	delete g_theInput;
	g_theInput = nullptr;
	delete g_theEventSystem;
	g_theEventSystem = nullptr;
}

void App::RunMainLoop()
{
	while (!IsQuitting())
	{
		RunFrame();
	}
}

void App::RunFrame()
{
	Clock::TickSystemClock();

	BeginFrame();			// Engine pre-frame stuff
	Update();				// Game updates / moves / spawns / hurts / kills stuffs
	Render();				// Game draws current state of things
	EndFrame();				// Engine post-frame stuff
}

void App::HandleQuitRequested()
{
	m_isQuitting = true;
}

void App::HandleWindowResized()
{
	g_theGame->OnWindowResized();
}

void App::BeginFrame()
{
	g_theEventSystem->BeginFrame();
	g_theInput->BeginFrame();
	g_theWindow->BeginFrame();
	g_theRenderer->BeginFrame();
	g_theDevConsole->BeginFrame();
	g_theAudio->BeginFrame();
	g_theJobSystem->BeginFrame();
	DebugRenderBeginFrame();
}

void App::Update()
{
	// F8 - Reset the Game
	if (g_theInput->WasKeyJustPressed(KEYCODE_F8))
	{
		delete g_theGame;
		g_theGame = new Game();
		g_theGame->m_isAttractMode = false; // skip attract mode
	}

	if (!g_theWindow->IsFocused() || g_theDevConsole->GetMode() != DevConsoleMode::HIDDEN)
	{
		g_theInput->SetCursorMode(CursorMode::POINTER);
	}
	else
	{
		g_theInput->SetCursorMode(g_theGame->GetCursorMode());
	}

	g_theGame->Update();
}

void App::Render() const
{
	g_theRenderer->ClearScreen(Rgba8(100, 100, 100));
	g_theGame->Render();

	// Render DevConsole
	Camera devConsoleCamera;
	AABB2 bounds = AABB2(0.f, 0.f, g_theWindow->GetAspectRatio(), 1.f);
	devConsoleCamera.SetOrthographicView(bounds.m_mins, bounds.m_maxs);
	g_theRenderer->BeginCamera(devConsoleCamera);
	g_theDevConsole->Render(bounds);
	g_theRenderer->EndCamera(devConsoleCamera);
}

void App::EndFrame()
{
	DebugRenderEndFrame();
	g_theJobSystem->EndFrame();
	g_theAudio->EndFrame();
	g_theDevConsole->EndFrame();
	g_theRenderer->EndFrame();
	g_theWindow->EndFrame();
	g_theInput->EndFrame();
	g_theEventSystem->EndFrame();
}

void App::LoadGameConfig(char const* gameConfigXmlFilePath)
{
	XmlDocument gameConfigXml;
	XmlResult result = gameConfigXml.LoadFile(gameConfigXmlFilePath);
	if (result != tinyxml2::XML_SUCCESS)
	{
		DebuggerPrintf("WARNING: failed to load game config from file \"%s\"\n", gameConfigXmlFilePath);
		return;
	}
	XmlElement* rootElement = gameConfigXml.RootElement();
	if (rootElement == nullptr)
	{
		DebuggerPrintf("WARNING: game config from file \"%s\" was invalid (missing root element)\n", gameConfigXmlFilePath);
		return;
	}
	g_gameConfigBlackboard.PopulateFromXmlElementAttributes(*rootElement);
}
