#include "Game/GameCamera.hpp"
#include "Game/Player.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Game.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/DebugRender.hpp"
#include "Engine/Window/Window.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/MathUtils.hpp"

constexpr float CAMERA_MOVE_SPEED = 4.f;
constexpr float CAMERA_YAW_TURN_RATE = 180.f;
constexpr float CAMERA_PITCH_TURN_RATE = 180.f;
constexpr float CAMERA_ROLL_TURN_RATE = 90.f;
constexpr float CAMERA_SPEED_FACTOR = 20.f;

constexpr float CAMERA_MAX_PITCH = 85.f;
constexpr float CAMERA_MAX_ROLL = 45.f;

constexpr float CAMERA_FOV = 60.f;
constexpr float CAMERA_NEAR = 0.1f;
constexpr float CAMERA_FAR = 2000.f;

constexpr float MOUSE_DELTA_RATIO = 0.075f;

const Vec3 START_POSITION(-50.f, -50.f, 150.f);
const EulerAngles START_ORIENTATION(45.f, 45.f, 0.f);

const EulerAngles FIXED_CAMERA_ORIENTATION(40.f, 30.f, 0.f);
constexpr float FIXED_CAMERA_DISTANCE = 10.f;
constexpr float OVER_SHOULDER_CAMERA_DISTANCE = 4.f;

GameCamera::GameCamera(Player* player)
	: m_player(player)
{
	m_position = START_POSITION;
	m_orientation = START_ORIENTATION;
	float aspect = Window::s_mainWindow->GetAspectRatio();
	m_camera.SetPerspectiveView(aspect, CAMERA_FOV, CAMERA_NEAR, CAMERA_FAR);
	m_camera.SetCameraToRenderTransform(Mat44::DIRECTX_C2R);

	m_player->SetPosition(m_position);
}

void GameCamera::Update()
{
	//float unscaledDeltaSeconds = static_cast<float>(Clock::GetSystemClock().GetDeltaSeconds());
	//UpdateOrientation(unscaledDeltaSeconds);
	//UpdatePosition(unscaledDeltaSeconds);

	//XboxController const& controller = g_theInput->GetController(0);
	//if (g_theInput->WasKeyJustPressed(KEYCODE_H) ||
	//	controller.WasButtonJustPressed(XboxButtonId::XBOX_BUTTON_START))
	//{
	//	m_position = SPECTATOR_START_POSITION;
	//	m_orientation = SPECTATOR_START_ORIENTATION;
	//}

	//if (g_theInput->WasKeyJustPressed(KEYCODE_C) ||
	//	controller.WasButtonJustPressed(XboxButtonId::XBOX_BUTTON_UP))
	//{
	//	m_isMovingOnXYPlane = !m_isMovingOnXYPlane;
	//}
	XboxController const& controller = g_theInput->GetController(0);

	if (g_theInput->WasKeyJustPressed(KEYCODE_C) ||
		controller.WasButtonJustPressed(XboxButtonId::XBOX_BUTTON_UP))
	{
		int currentMode = static_cast<int>(m_cameraMode);
		currentMode = (currentMode + 1) % static_cast<int>(GameCameraMode::NUM);
		m_cameraMode = static_cast<GameCameraMode>(currentMode);
	}


	switch (m_cameraMode)
	{
	case GameCameraMode::FIRST_PERSON:
		UpdateFirstPerson();
		break;
	case GameCameraMode::FIXED_ANGLE_TRACKING:
		UpdateFixedAngleTracking();
		break;
	case GameCameraMode::OVER_SHOULDER:
		UpdateOverShoulder();
		break;
	case GameCameraMode::SPECTATOR:
		UpdateSpectator();
		break;
	case GameCameraMode::SPECTATOR_XY:
		UpdateSpectatorXY();
		break;
	case GameCameraMode::INDEPENDENT:
		UpdateIndependent();
		break;
	default:
		break;
	}


}

void GameCamera::LateUpdate()
{
	if (m_cameraMode == GameCameraMode::FIRST_PERSON)
	{
		m_position = m_player->GetEyePosition();
	}
	else if (m_cameraMode == GameCameraMode::FIXED_ANGLE_TRACKING)
	{
		Vec3 lookDirection = Vec3::MakeFromPolarDegrees(m_orientation.m_pitchDegrees, m_orientation.m_yawDegrees);
		m_position = m_player->GetEyePosition() - FIXED_CAMERA_DISTANCE * lookDirection;
	}
	else if (m_cameraMode == GameCameraMode::OVER_SHOULDER)
	{
		Vec3 lookDirection = Vec3::MakeFromPolarDegrees(m_orientation.m_pitchDegrees, m_orientation.m_yawDegrees);
		m_position = m_player->GetEyePosition() - OVER_SHOULDER_CAMERA_DISTANCE * lookDirection;
	}

	// Update Camera
	m_camera.SetPositionAndOrientation(m_position, m_orientation);

	// Add camera center axis
	Vec3 forwardIBasis, leftJBasis, upKBasis;
	m_orientation.GetAsVectors_IFwd_JLeft_KUp(forwardIBasis, leftJBasis, upKBasis);
	Mat44 cameraCenterAxisTransform;

	cameraCenterAxisTransform.SetTranslation3D(m_position + 0.2f * forwardIBasis);
	DebugAddBasis(cameraCenterAxisTransform, 0.f, 0.004f, 0.0003f, 1.f, 1.f, DebugRenderMode::ALWAYS);

	if (m_cameraMode != GameCameraMode::FIRST_PERSON)
	{
		m_player->DebugDrawCollider();
	}
}

void GameCamera::RefreshAspectRatio()
{
	float aspect = Window::s_mainWindow->GetAspectRatio();
	m_camera.SetPerspectiveView(aspect, CAMERA_FOV, CAMERA_NEAR, CAMERA_FAR);
}

std::string GameCamera::GetCameraModeName() const
{
	switch (m_cameraMode)
	{
	case GameCameraMode::FIRST_PERSON:
		return "FirstPerson";
		break;
	case GameCameraMode::FIXED_ANGLE_TRACKING:
		return "FixedAngleTracking";
		break;
	case GameCameraMode::OVER_SHOULDER:
		return "OverShoulder";
		break;
	case GameCameraMode::SPECTATOR:
		return "Spectator";
		break;
	case GameCameraMode::SPECTATOR_XY:
		return "SpectatorXY";
		break;
	case GameCameraMode::INDEPENDENT:
		return "Independent";
		break;
	default:
		return "";
		break;
	}
}

bool GameCamera::GetRay(Vec3& out_rayStart, Vec3& out_rayFwdNormal, float& out_rayLength) const
{
	constexpr float DEFAULT_RAY_LENGTH = 8.f;

	if (m_cameraMode == GameCameraMode::FIRST_PERSON || m_cameraMode == GameCameraMode::SPECTATOR || m_cameraMode == GameCameraMode::SPECTATOR_XY)
	{
		out_rayStart = m_position;
		out_rayFwdNormal = m_orientation.GetAsMatrix_IFwd_JLeft_KUp().GetIBasis3D();
		out_rayLength = DEFAULT_RAY_LENGTH;
		return true;
	}
	else if (m_cameraMode == GameCameraMode::OVER_SHOULDER)
	{
		out_rayStart = m_player->GetEyePosition();
		out_rayFwdNormal = m_orientation.GetAsMatrix_IFwd_JLeft_KUp().GetIBasis3D();
		out_rayLength = DEFAULT_RAY_LENGTH;
		return true;
	}
	else if (m_cameraMode == GameCameraMode::FIXED_ANGLE_TRACKING)
	{
		Vec2 clientUV = g_theInput->GetCursorNormalizedPosition();
		bool ok = m_camera.ScreenPointToRay(out_rayStart, out_rayFwdNormal, clientUV);
		if (ok)
		{
			out_rayLength = DEFAULT_RAY_LENGTH + FIXED_CAMERA_DISTANCE * 2.f;
			return true;
		}
		else
		{
			return false;
		}
	}
	else if (m_cameraMode == GameCameraMode::INDEPENDENT)
	{
		out_rayStart = m_position;
		out_rayFwdNormal = m_orientation.GetAsMatrix_IFwd_JLeft_KUp().GetIBasis3D();
		out_rayLength = DEFAULT_RAY_LENGTH * 3.f;
		return true;
	}


	return false;
}

void GameCamera::UpdateFirstPerson()
{
	float unscaledDeltaSeconds = static_cast<float>(Clock::GetSystemClock().GetDeltaSeconds());
	UpdateOrientation(unscaledDeltaSeconds);

	m_player->SetOrientation(m_orientation);
	m_player->HandleInput();

}

void GameCamera::UpdateFixedAngleTracking()
{
	m_orientation = FIXED_CAMERA_ORIENTATION;

	m_player->SetOrientation(m_orientation);
	m_player->HandleInput();

	g_theGame->SetCursorMode(CursorMode::POINTER);
	// #ToDo: can see the mouse, use mouse screen ray to ray cast
}

void GameCamera::UpdateOverShoulder()
{
	float unscaledDeltaSeconds = static_cast<float>(Clock::GetSystemClock().GetDeltaSeconds());
	UpdateOrientation(unscaledDeltaSeconds);

	m_player->SetOrientation(m_orientation);
	m_player->HandleInput();
}

void GameCamera::UpdateSpectator()
{
	float unscaledDeltaSeconds = static_cast<float>(Clock::GetSystemClock().GetDeltaSeconds());
	UpdateOrientation(unscaledDeltaSeconds);
	UpdatePosition(unscaledDeltaSeconds, false);
}

void GameCamera::UpdateSpectatorXY()
{
	float unscaledDeltaSeconds = static_cast<float>(Clock::GetSystemClock().GetDeltaSeconds());
	UpdateOrientation(unscaledDeltaSeconds);
	UpdatePosition(unscaledDeltaSeconds, true);
}

void GameCamera::UpdateIndependent()
{
	float unscaledDeltaSeconds = static_cast<float>(Clock::GetSystemClock().GetDeltaSeconds());
	UpdateOrientation(unscaledDeltaSeconds);
	
	m_player->SetOrientation(m_orientation);
	m_player->HandleInput();
}

void GameCamera::UpdateOrientation(float deltaSeconds)
{
	XboxController const& controller = g_theInput->GetController(0);

	// Yaw & Pitch
	Vec2 cursorPositionDelta = g_theInput->GetCursorClientDelta();
	float deltaYaw = -cursorPositionDelta.x * MOUSE_DELTA_RATIO;
	float deltaPitch = cursorPositionDelta.y * MOUSE_DELTA_RATIO;

	m_orientation.m_yawDegrees += deltaYaw;
	m_orientation.m_pitchDegrees += deltaPitch;

	Vec2 rightStick = controller.GetRightStick().GetPosition();
	deltaYaw = -deltaSeconds * CAMERA_YAW_TURN_RATE * rightStick.x;
	deltaPitch = -deltaSeconds * CAMERA_PITCH_TURN_RATE * rightStick.y;

	m_orientation.m_yawDegrees += deltaYaw;
	m_orientation.m_pitchDegrees += deltaPitch;

	m_orientation.m_pitchDegrees = GetClamped(m_orientation.m_pitchDegrees, -CAMERA_MAX_PITCH, CAMERA_MAX_PITCH);
}

void GameCamera::UpdatePosition(float deltaSeconds, bool isMovingOnXYPlane)
{
	XboxController const& controller = g_theInput->GetController(0);

	float const speedMultiplier = (g_theInput->IsKeyDown(KEYCODE_SHIFT) || controller.GetLeftTrigger() > 0.f || controller.GetRightTrigger() > 0.f) ? CAMERA_SPEED_FACTOR : 1.f;

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

	if (!isMovingOnXYPlane)
	{
		Vec3 forwardIBasis, leftJBasis, upKBasis;
		m_orientation.GetAsVectors_IFwd_JLeft_KUp(forwardIBasis, leftJBasis, upKBasis);
		m_position += (forwardIBasis * moveIntention.x + leftJBasis * moveIntention.y + upKBasis * moveIntention.z) *
			CAMERA_MOVE_SPEED * deltaSeconds * speedMultiplier;
	}
	else
	{
		Vec3 forward = Vec3(Vec2::MakeFromPolarDegrees(m_orientation.m_yawDegrees));
		Vec3 left = Vec3(-forward.y, forward.x, 0.f);
		m_position += (forward * moveIntention.x + left * moveIntention.y) *
			CAMERA_MOVE_SPEED * deltaSeconds * speedMultiplier;
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

	m_position += elevateIntention * CAMERA_MOVE_SPEED * deltaSeconds * speedMultiplier;
}
