#pragma once

#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include <vector>
#include <string>

class World;

enum class PhysicsMode
{
	WALKING,
	FLYING,
	NOCLIP,
	NUM
};

class Entity
{

public:
	Entity(World* world);
	virtual ~Entity() = default;

	virtual void Update(float deltaSeconds);
	virtual void UpdatePhysics(float fixedDeltaSeconds);

	void DebugDrawCollider() const;

	// Movement
	void MoveInDirection(Vec3 const& direction, float speedMultiplier);
	void AddForce(Vec3 const& force);
	void AddImpulse(Vec3 const& impulse);
	void Jump();

	// Getters
	Vec3 GetPosition() const { return m_position; }
	Vec3 GetVelocity() const { return m_velocity; }
	Vec3 GetEyePosition() const { return m_position + Vec3(0.f, 0.f, m_eyeHeight); }
	EulerAngles GetOrientation() const { return m_orientation; }
	bool IsOnGround() const { return m_isOnGround; }
	PhysicsMode GetPhysicsMode() const { return m_physicsMode; }
	std::string GetPhysicsModeName() const;


	// Setters
	void SetPosition(Vec3 const& position) { m_position = position; }
	void SetOrientation(EulerAngles const& orientation) { m_orientation = orientation; }
	void SetPhysicsMode(PhysicsMode mode) { m_physicsMode = mode; }

protected:
	// Physics update methods #ToDo: Vertical Drag is same as horizontal?
	void UpdatePhysicsWalking(float fixedDeltaSeconds);
	void UpdatePhysicsFlying(float fixedDeltaSeconds);
	void UpdatePhysicsNoclip(float fixedDeltaSeconds);

	// Collision detection helpers
	bool IsGrounded() const;
	Vec3 SweepBox(Vec3 const& displacement); // Will Change Velocity
	void GetBoxCorners(std::vector<Vec3>& corners, float shrinkAmount = 0.f) const;

protected:
	World* m_world = nullptr;

	// Transform
	Vec3 m_position = Vec3::ZERO;
	Vec3 m_velocity = Vec3::ZERO;
	Vec3 m_acceleration = Vec3::ZERO;
	EulerAngles m_orientation; // Controlled Rotation, need to be updated by GameCamera

	// Physics mode
	PhysicsMode m_physicsMode = PhysicsMode::NOCLIP;

	// Collision box (half extents)
	Vec3 m_boxHalfExtents = Vec3(0.3f, 0.3f, 0.9f);  // 0.6m wide, 1.8m tall
	float m_eyeHeight = 0.75f;

	// Physics constants
	float m_groundDrag = 10.0f;         // High drag when on ground
	float m_airDrag = 1.0f;             // Low drag when in air
	float m_gravity = 20.0f;            // Gravity acceleration
	float m_maxWalkSpeed = 5.0f;        // Max horizontal speed when walking
	float m_maxFlySpeed = 10.0f;        // Max speed when flying
	float m_jumpImpulse = 8.0f;         // Jump velocity boost

	// State
	bool m_isOnGround = false;

};

