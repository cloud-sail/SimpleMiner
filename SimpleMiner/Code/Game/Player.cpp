#include "Game/Player.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/MathUtils.hpp"

Player::Player(World* world)
	: Entity(world)
{

}

void Player::Update(float deltaSeconds)
{
	UNUSED(deltaSeconds);

	// Handle input every frame
	// It is called by GameCamera
	// HandleInput();


	// Non-physics updates
	Entity::Update(deltaSeconds);
}

void Player::UpdatePhysics(float fixedDeltaSeconds)
{
	// Apply movement with speed multiplier
	if (m_moveInput.GetLengthSquared() > 0.f)
	{
		float speedMultiplier = m_normalSpeedMultiplier;

		// Apply sprint multiplier
		if (m_isSprinting)
		{
			speedMultiplier = m_sprintSpeedMultiplier;
		}

		MoveInDirection(m_moveInput, speedMultiplier);
	}

	// Apply jump
	if (m_wantsToJump)
	{
		Jump();
		m_wantsToJump = false;
	}

	// Call base physics update
	Entity::UpdatePhysics(fixedDeltaSeconds);
}

void Player::HandleInput()
{
	// Reset
	m_moveInput = Vec3::ZERO;

	// Calculate movement directions
	Vec3 forward = Vec3(CosDegrees(m_orientation.m_yawDegrees), SinDegrees(m_orientation.m_yawDegrees), 0.f);
	Vec3 left = Vec3(-forward.y, forward.x, 0.f);


	XboxController const& controller = g_theInput->GetController(0);
	Vec2 controllerMove = controller.GetLeftStick().GetPosition().GetRotatedMinus90Degrees();
	m_moveInput += (controllerMove.x * forward + controllerMove.y * left);

	if (g_theInput->IsKeyDown(KEYCODE_W))
	{
		m_moveInput += forward;
	}
	if (g_theInput->IsKeyDown(KEYCODE_S))
	{
		m_moveInput -= forward;
	}
	if (g_theInput->IsKeyDown(KEYCODE_A))
	{
		m_moveInput += left;
	}
	if (g_theInput->IsKeyDown(KEYCODE_D))
	{
		m_moveInput -= left;
	}

	m_moveInput.ClampLength(1.f);

	bool isSprintPressed = (g_theInput->IsKeyDown(KEYCODE_SHIFT) || controller.GetLeftTrigger() > 0.f || controller.GetRightTrigger() > 0.f);

	// Sprint (only when on ground and moving forward)
	if (m_physicsMode == PhysicsMode::WALKING)
	{
		m_isSprinting = m_isOnGround && isSprintPressed;
	}
	else
	{
		m_isSprinting = isSprintPressed;
	}

	if (g_theInput->WasKeyJustPressed(KEYCODE_SPACE) || controller.WasButtonJustPressed(XboxButtonId::XBOX_BUTTON_A))
	{
		m_wantsToJump = true;
	}

	if (g_theInput->WasKeyJustPressed(KEYCODE_V) || controller.WasButtonJustPressed(XboxButtonId::XBOX_BUTTON_B))
	{
		int currentMode = static_cast<int>(m_physicsMode);
		currentMode = (currentMode + 1) % static_cast<int>(PhysicsMode::NUM);
		m_physicsMode = static_cast<PhysicsMode>(currentMode);
	}

	if (m_physicsMode == PhysicsMode::FLYING || m_physicsMode == PhysicsMode::NOCLIP)
	{
		if (g_theInput->IsKeyDown(KEYCODE_E))
		{
			m_moveInput.z += 1.f;
		}
		if (g_theInput->IsKeyDown(KEYCODE_Q))
		{
			m_moveInput.z -= 1.f;
		}
		if (controller.IsButtonDown(XboxButtonId::XBOX_BUTTON_RIGHT_SHOULDER))
		{
			m_moveInput.z += 1.f;
		}
		if (controller.IsButtonDown(XboxButtonId::XBOX_BUTTON_LEFT_SHOULDER))
		{
			m_moveInput.z -= 1.f;
		}

		// Normalize m_moveInput?
	}

}
