#include "Game/GameCommon.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Window/Window.hpp"

float g_screenWidth = 1600.0f; 
float g_screenHeight = 800.0f;

void AddVertsForColoredCube3D(std::vector<Vertex_PCU>& verts)
{
	// p-positive n-negative
	Vec3 nnn(-0.5f, -0.5f, -0.5f);
	Vec3 nnp(-0.5f, -0.5f, +0.5f);
	Vec3 npn(-0.5f, +0.5f, -0.5f);
	Vec3 npp(-0.5f, +0.5f, +0.5f);
	Vec3 pnn(+0.5f, -0.5f, -0.5f);
	Vec3 pnp(+0.5f, -0.5f, +0.5f);
	Vec3 ppn(+0.5f, +0.5f, -0.5f);
	Vec3 ppp(+0.5f, +0.5f, +0.5f);

	AddVertsForQuad3D(verts, pnn, ppn, ppp, pnp, Rgba8::RED); // +x
	AddVertsForQuad3D(verts, npn, nnn, nnp, npp, Rgba8::CYAN); // -x
	AddVertsForQuad3D(verts, ppn, npn, npp, ppp, Rgba8::GREEN); // +y
	AddVertsForQuad3D(verts, nnn, pnn, pnp, nnp, Rgba8::MAGENTA); // -y
	AddVertsForQuad3D(verts, npp, nnp, pnp, ppp, Rgba8::BLUE); // +z
	AddVertsForQuad3D(verts, ppn, pnn, nnn, npn, Rgba8::YELLOW); // -z
}

void UpdateScreenDimensions()
{
	IntVec2 clientDimensions = Window::s_mainWindow->GetClientDimensions();
	Vec2 dimensions = Vec2(clientDimensions);
	g_screenWidth = dimensions.x;
	g_screenHeight = dimensions.y;
}

std::string FormatWorldTime(float totalSeconds)
{
	int days = static_cast<int>(totalSeconds / ONE_DAY_SECONDS);
	float remainingSeconds = totalSeconds - (days * ONE_DAY_SECONDS);

	int hours = static_cast<int>(remainingSeconds / ONE_HOUR_SECONDS);
	remainingSeconds -= (hours * ONE_HOUR_SECONDS);

	int minutes = static_cast<int>(remainingSeconds / ONE_MINUTE_SECONDS);
	remainingSeconds -= (minutes * ONE_MINUTE_SECONDS);

	int seconds = static_cast<int>(remainingSeconds);

	return Stringf("%d days %02d:%02d:%02d",
		days, hours, minutes, seconds);

}

float GetDayProgress(float totalSeconds)
{
	return fmodf(totalSeconds, ONE_DAY_SECONDS) / ONE_DAY_SECONDS;
}

Camera CreateOrthoCameraForAABB3(AABB3 const& box, Vec3 const& direction)
{
	Camera camera;
	camera.SetCameraToRenderTransform(Mat44::DIRECTX_C2R);

	Mat44 rotationMatrix = Mat44::MakeFromX(direction);

	Vec3 forward = rotationMatrix.GetIBasis3D();
	Vec3 left = rotationMatrix.GetJBasis3D();
	Vec3 up = rotationMatrix.GetKBasis3D();

	camera.SetOrientation(rotationMatrix.GetEulerAngles());


	Vec3 halfDims = 0.5f * box.GetDimensions();
	Vec3 center = box.GetCenter();

	camera.SetPosition(center);

	const float r = halfDims.x * fabsf(forward.x) +
		halfDims.y * fabsf(forward.y) +
		halfDims.z * fabsf(forward.z);

	const float halfWidth = halfDims.x * fabsf(left.x) +
		halfDims.y * fabsf(left.y) +
		halfDims.z * fabsf(left.z);

	const float halfHeight = halfDims.x * fabsf(up.x) +
		halfDims.y * fabsf(up.y) +
		halfDims.z * fabsf(up.z);

	const float margin = 1.05f; // Padding 5%
	const float actualHalfWidth = halfWidth * margin;
	const float actualHalfHeight = halfHeight * margin;

	Vec2 bottomLeft(-actualHalfWidth, -actualHalfHeight);
	Vec2 topRight(actualHalfWidth, actualHalfHeight);

	camera.SetOrthographicView(bottomLeft, topRight, -r, r);

	return camera;
}

