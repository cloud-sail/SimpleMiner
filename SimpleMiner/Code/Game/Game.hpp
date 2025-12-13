#pragma once
#include "Game/GameCommon.hpp"
#include "Game/GameFloatCurve.hpp"
#include "Game/PCGParams.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/Gradient.hpp"

class BloomEffect;
class RainParticleSystem;

// ----------------------------------------------------------------------------------------------
class Game
{
public:
	Game();
	~Game();
	void Update();
	void Render() const;

	void OnWindowResized(); // Event WINDOW_RESIZE_EVENT, refresh the setting of the camera
	void UpdateDeveloperCheats();
	CursorMode GetCursorMode() const { return m_cursorMode; }
	void SetCursorMode(CursorMode mode) { m_cursorMode = mode; }
	float GetDeltaSeconds() const;
	Clock* GetClock() const { return m_clock; }

public:
	Shader* m_ditherShader = nullptr;
	Shader* m_unlitEmissiveShader = nullptr;

protected:
	Clock* m_clock = nullptr;
	CursorMode m_cursorMode = CursorMode::POINTER;

	Camera m_screenCamera;

#pragma region optional
// Attract Mode
private:
	void UpdateAttractMode();
	void RenderAttractMode() const;

public:
	bool m_isAttractMode = true;
	Gradient m_skyGradient;

private:
	void DebugDrawStartup();
	void DebugDrawUpdate();

	void UpdateCameras();

#pragma endregion


protected:
	void InitializeBlockTexture();

protected:
	World* m_world = nullptr;

	bool m_debugDrawChunk = false;
	bool m_debugDrawJobSystem = false;


	// PCG Settings
public:
	PCGParams GetPCGParams() const { return m_pcgParams; }
	int GetDebugLayerMode() const { return m_debugLayerMode; }

	Spline1DEditor m_curveEditor;
	
protected:
	PCGParams m_pcgParams; // Chunk use this. It is not directly linked to the imgui


protected:
	void ShowMainImGuiWindow();

	void RenderDebugCombo();
	int m_debugLayerMode = 0; // NO_DEBUG
};
