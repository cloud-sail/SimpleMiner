#include "Game/App.hpp"
#include "Game/GameCommon.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

//-----------------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE applicationInstanceHandle, HINSTANCE, LPSTR commandLineString, int)
{
	UNUSED(applicationInstanceHandle);
	UNUSED(commandLineString);

	g_theApp = new App();
	g_theApp->Startup();
	g_theApp->RunMainLoop();
	g_theApp->Shutdown();
	delete g_theApp;
	g_theApp = nullptr;

	return 0;
}
