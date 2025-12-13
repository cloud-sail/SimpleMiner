#pragma once
#include "Engine/Renderer/Camera.hpp"

class SpectatorCamera
{
public:
	SpectatorCamera();
	~SpectatorCamera() = default;

	void Update();

	void RefreshAspectRatio();
	bool IsSpectatorXY() const { return m_isMovingOnXYPlane; }

public:
	Camera m_camera;
	Vec3 m_position;
	EulerAngles m_orientation;

protected:
	void UpdateOrientation(float deltaSeconds);
	void UpdatePosition(float deltaSeconds);
	void UpdateCamera();

protected:
	bool m_isMovingOnXYPlane = false;
};

