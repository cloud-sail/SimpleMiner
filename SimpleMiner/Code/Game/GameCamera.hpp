#pragma once
#include "Engine/Renderer/Camera.hpp"
#include <string>

class Player;

enum class GameCameraMode
{
	FIRST_PERSON,
	FIXED_ANGLE_TRACKING,
	OVER_SHOULDER,
	SPECTATOR,				// Not processed the player
	SPECTATOR_XY,
	INDEPENDENT,
	NUM
};



class GameCamera
{
public:
	GameCamera(Player* player);
	~GameCamera() = default;

	void Update();
	void LateUpdate(); // After player updates their physics

	void RefreshAspectRatio();

	std::string GetCameraModeName() const;

	bool GetRay(Vec3& out_rayStart, Vec3& out_rayFwdNormal, float& out_rayLength) const; 
	// Update Ray if not locked
	// But If the player camera is locked 

	void SetCameraMode(GameCameraMode mode) { m_cameraMode = mode; }

public:
	Camera m_camera;
	Vec3 m_position;
	EulerAngles m_orientation;

protected:
	void UpdateFirstPerson();
	void UpdateFixedAngleTracking();
	void UpdateOverShoulder();
	void UpdateSpectator();
	void UpdateSpectatorXY();
	void UpdateIndependent();


protected:
	void UpdateOrientation(float deltaSeconds);
	void UpdatePosition(float deltaSeconds, bool isMovingOnXYPlane);


protected:
	// Similar to a controller, it will update the orientation of the player entity, 
	// and call Handle Input on the player
	Player* m_player = nullptr;

	GameCameraMode m_cameraMode = GameCameraMode::FIRST_PERSON;
};

