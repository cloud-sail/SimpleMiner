#pragma once
#include "Game/Entity.hpp"

class Player : public Entity
{
public:
	Player(World* world);

	void Update(float deltaSeconds) override;
	void UpdatePhysics(float fixedDeltaSeconds) override;

	void HandleInput(); // GameCamera will call it

private:

	// Input state (accumulated between physics updates)
	Vec3 m_moveInput = Vec3::ZERO;
	bool m_wantsToJump = false;
	bool m_isSprinting = false;

	float m_normalSpeedMultiplier = 1.0f;
	float m_sprintSpeedMultiplier = 1.5f;

	// Simple Solution: Game camera control the player, do not handle Input
	// Having a controller class to handle input is better.
};