#include "Game/SpectatorCamera.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/DebugRender.hpp"
#include "Engine/Window/Window.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/MathUtils.hpp"

constexpr float SPECTATOR_CAMERA_MOVE_SPEED = 4.f;
constexpr float SPECTATOR_CAMERA_YAW_TURN_RATE = 180.f;
constexpr float SPECTATOR_CAMERA_PITCH_TURN_RATE = 180.f;
constexpr float SPECTATOR_CAMERA_ROLL_TURN_RATE = 90.f;
constexpr float SPECTATOR_CAMERA_SPEED_FACTOR = 20.f;

constexpr float SPECTATOR_CAMERA_MAX_PITCH = 85.f;
constexpr float SPECTATOR_CAMERA_MAX_ROLL = 45.f;

constexpr float SPECTATOR_CAMERA_FOV = 60.f;
constexpr float SPECTATOR_CAMERA_NEAR = 0.1f;
constexpr float SPECTATOR_CAMERA_FAR = 850.f;

const Vec3 SPECTATOR_START_POSITION(-50.f, -50.f, 150.f);
const EulerAngles SPECTATOR_START_ORIENTATION(45.f, 45.f, 0.f);


SpectatorCamera::SpectatorCamera()
{
	m_position = SPECTATOR_START_POSITION;
	m_orientation = SPECTATOR_START_ORIENTATION;
	float aspect = Window::s_mainWindow->GetAspectRatio();
	m_camera.SetPerspectiveView(aspect, SPECTATOR_CAMERA_FOV, SPECTATOR_CAMERA_NEAR, SPECTATOR_CAMERA_FAR);
	m_camera.SetCameraToRenderTransform(Mat44::DIRECTX_C2R);
}

void SpectatorCamera::Update()
{
	float unscaledDeltaSeconds = static_cast<float>(Clock::GetSystemClock().GetDeltaSeconds());
	UpdateOrientation(unscaledDeltaSeconds);
	UpdatePosition(unscaledDeltaSeconds);

	XboxController const& controller = g_theInput->GetController(0);
	if (g_theInput->WasKeyJustPressed(KEYCODE_H) ||
		controller.WasButtonJustPressed(XboxButtonId::XBOX_BUTTON_START))
	{
		m_position = SPECTATOR_START_POSITION;
		m_orientation = SPECTATOR_START_ORIENTATION;
	}

	if (g_theInput->WasKeyJustPressed(KEYCODE_C) ||
		controller.WasButtonJustPressed(XboxButtonId::XBOX_BUTTON_UP))
	{
		m_isMovingOnXYPlane = !m_isMovingOnXYPlane;
	}

	UpdateCamera();

	// Add camera center axis
	Vec3 forwardIBasis, leftJBasis, upKBasis;
	m_orientation.GetAsVectors_IFwd_JLeft_KUp(forwardIBasis, leftJBasis, upKBasis);
	Mat44 cameraCenterAxisTransform;
	//cameraCenterAxisTransform.SetTranslation3D(m_position + 50.f * forwardIBasis);
	//DebugAddBasis(cameraCenterAxisTransform, 0.f, 1.f, 0.075f, 1.f, 1.f, DebugRenderMode::ALWAYS);

	cameraCenterAxisTransform.SetTranslation3D(m_position + 0.2f * forwardIBasis);
	DebugAddBasis(cameraCenterAxisTransform, 0.f, 0.004f, 0.0003f, 1.f, 1.f, DebugRenderMode::ALWAYS);
}

void SpectatorCamera::RefreshAspectRatio()
{
	float aspect = Window::s_mainWindow->GetAspectRatio();
	m_camera.SetPerspectiveView(aspect, SPECTATOR_CAMERA_FOV, SPECTATOR_CAMERA_NEAR, SPECTATOR_CAMERA_FAR);
}

void SpectatorCamera::UpdateOrientation(float deltaSeconds)
{
	XboxController const& controller = g_theInput->GetController(0);

	// Yaw & Pitch
	Vec2 cursorPositionDelta = g_theInput->GetCursorClientDelta();
	float deltaYaw = -cursorPositionDelta.x * 0.075f;
	float deltaPitch = cursorPositionDelta.y * 0.075f;

	m_orientation.m_yawDegrees += deltaYaw;
	m_orientation.m_pitchDegrees += deltaPitch;

	Vec2 rightStick = controller.GetRightStick().GetPosition();
	deltaYaw = -deltaSeconds * SPECTATOR_CAMERA_YAW_TURN_RATE * rightStick.x;
	deltaPitch = -deltaSeconds * SPECTATOR_CAMERA_PITCH_TURN_RATE * rightStick.y;

	m_orientation.m_yawDegrees += deltaYaw;
	m_orientation.m_pitchDegrees += deltaPitch;

	m_orientation.m_pitchDegrees = GetClamped(m_orientation.m_pitchDegrees, -SPECTATOR_CAMERA_MAX_PITCH, SPECTATOR_CAMERA_MAX_PITCH);

	// Roll
	//float posRoll = (g_theInput->IsKeyDown(KEYCODE_C) || controller.GetRightTrigger() > 0.f) ? 1.f : 0.f;
	//float negRoll = (g_theInput->IsKeyDown(KEYCODE_Z) || controller.GetLeftTrigger() > 0.f) ? -1.f : 0.f;
	//float deltaRoll = (posRoll + negRoll) * deltaSeconds * SPECTATOR_CAMERA_ROLL_TURN_RATE;
	//m_orientation.m_rollDegrees += deltaRoll;
	//m_orientation.m_rollDegrees = GetClamped(m_orientation.m_rollDegrees, -SPECTATOR_CAMERA_MAX_ROLL, SPECTATOR_CAMERA_MAX_ROLL);
}

void SpectatorCamera::UpdatePosition(float deltaSeconds)
{
	XboxController const& controller = g_theInput->GetController(0);

	float const speedMultiplier = (g_theInput->IsKeyDown(KEYCODE_SHIFT) || controller.GetLeftTrigger() > 0.f || controller.GetRightTrigger() > 0.f) ? SPECTATOR_CAMERA_SPEED_FACTOR : 1.f;

	Vec3 moveIntention = Vec3(controller.GetLeftStick().GetPosition().GetRotatedMinus90Degrees());

	if (g_theInput->IsKeyDown(KEYCODE_W))
	{
		moveIntention += Vec3(1.f, 0.f, 0.f);
	}
	if (g_theInput->IsKeyDown(KEYCODE_S))
	{
		moveIntention += Vec3(-1.f, 0.f, 0.f);
	}
	if (g_theInput->IsKeyDown(KEYCODE_A))
	{
		moveIntention += Vec3(0.f, 1.f, 0.f);
	}
	if (g_theInput->IsKeyDown(KEYCODE_D))
	{
		moveIntention += Vec3(0.f, -1.f, 0.f);
	}

	moveIntention.ClampLength(1.f);

	if (!m_isMovingOnXYPlane)
	{
		Vec3 forwardIBasis, leftJBasis, upKBasis;
		m_orientation.GetAsVectors_IFwd_JLeft_KUp(forwardIBasis, leftJBasis, upKBasis);
		m_position += (forwardIBasis * moveIntention.x + leftJBasis * moveIntention.y + upKBasis * moveIntention.z) *
			SPECTATOR_CAMERA_MOVE_SPEED * deltaSeconds * speedMultiplier;
	}
	else
	{
		Vec3 forward = Vec3(Vec2::MakeFromPolarDegrees(m_orientation.m_yawDegrees));
		Vec3 left = Vec3(-forward.y, forward.x, 0.f);
		m_position += (forward * moveIntention.x + left * moveIntention.y) *
			SPECTATOR_CAMERA_MOVE_SPEED * deltaSeconds * speedMultiplier;
	}



	Vec3 elevateIntention;
	if (g_theInput->IsKeyDown(KEYCODE_Q))
	{
		elevateIntention += Vec3(0.f, 0.f, -1.f);
	}
	if (g_theInput->IsKeyDown(KEYCODE_E))
	{
		elevateIntention += Vec3(0.f, 0.f, 1.f);
	}
	if (controller.IsButtonDown(XboxButtonId::XBOX_BUTTON_LEFT_SHOULDER))
	{
		elevateIntention += Vec3(0.f, 0.f, -1.f);
	}
	if (controller.IsButtonDown(XboxButtonId::XBOX_BUTTON_RIGHT_SHOULDER))
	{
		elevateIntention += Vec3(0.f, 0.f, 1.f);
	}
	elevateIntention.ClampLength(1.f);

	m_position += elevateIntention * SPECTATOR_CAMERA_MOVE_SPEED * deltaSeconds * speedMultiplier;
}

void SpectatorCamera::UpdateCamera()
{
	m_camera.SetPositionAndOrientation(m_position, m_orientation);
}

