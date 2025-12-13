#pragma once
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Renderer/RendererCommon.hpp"
#include <string>
//-----------------------------------------------------------------------------------------------
class AudioSystem;
class JobSystem;
class InputSystem;
class Renderer;
class Window;
class App;
class Game;
class Clock;
class Skybox;
class SpriteSheet;
class Texture;
class VertexBuffer;
class IndexBuffer;
class Shader;
class Camera;
struct AABB3;


struct Vertex_PCU;
struct IntVec2;
struct Frustum;

//-----------------------------------------------------------------------------------------------
class Chunk;
class SpectatorCamera;
class World;
class Block;

//-----------------------------------------------------------------------------------------------
extern AudioSystem*		g_theAudio;
extern JobSystem*		g_theJobSystem;
extern InputSystem*		g_theInput;
extern Renderer*		g_theRenderer;
extern Window*			g_theWindow;
extern App*				g_theApp;
extern Game*			g_theGame;

//-----------------------------------------------------------------------------------------------
extern bool g_isDebugDraw;

//-----------------------------------------------------------------------------------------------
// Gameplay Globals
extern float g_screenWidth;
extern float g_screenHeight;
extern SpriteSheet* g_blockSpriteSheet;
extern Texture* g_blockTexture;

//-----------------------------------------------------------------------------------------------
// Gameplay Constants

constexpr float FADE_IN_SECONDS = 2.f;
constexpr float FADE_OUT_SECONDS = 1.f;

struct DitherFadeRenderResources
{
	uint32_t diffuseTextureIndex = INVALID_INDEX_U32;
	uint32_t diffuseSamplerIndex = INVALID_INDEX_U32;

	uint32_t cameraConstantsIndex = INVALID_INDEX_U32;
	uint32_t modelConstantsIndex = INVALID_INDEX_U32;

	float fadeAmount = 1.f;

	uint32_t worldConstantsIndex = INVALID_INDEX_U32;
	uint32_t skyQuadSRVIndex = INVALID_INDEX_U32;
};


constexpr float WORLD_TIME_RATIO = 200.f;

constexpr float ONE_DAY_SECONDS = 24.f * 3600.f / WORLD_TIME_RATIO;
constexpr float ONE_HOUR_SECONDS = ONE_DAY_SECONDS / 24.f;
constexpr float ONE_MINUTE_SECONDS = ONE_HOUR_SECONDS / 60.f;

constexpr double TIME_ACCELERATION_FACTOR = 60.0;

constexpr float RAY_LENGTH = 8.f;

enum class BlockFace
{
	EAST,
	WEST,
	NORTH,
	SOUTH,
	SKYWARD,
	DOWNWARD,
};

constexpr float PLAYER_HALF_HEIGHT = 0.9f;
constexpr float PLAYER_BOX_RADIUS = 0.3f;
constexpr float PLAYER_EYE_OFFSET = 0.75f; // From the center of the player

//-----------------------------------------------------------------------------------------------
struct FullScreenQuadResources
{
	uint32_t textureIndex = INVALID_INDEX_U32;
	uint32_t samplerIndex = INVALID_INDEX_U32;
};

struct FullScreenQuadWithDepthResources
{
	uint32_t textureIndex = INVALID_INDEX_U32;
	uint32_t depthTexIndex = INVALID_INDEX_U32;
	uint32_t samplerIndex = INVALID_INDEX_U32;
};

struct UnlitEmissiveResources
{
	uint32_t diffuseTextureIndex = INVALID_INDEX_U32;
	uint32_t diffuseSamplerIndex = INVALID_INDEX_U32;

	uint32_t cameraConstantsIndex = INVALID_INDEX_U32;
	uint32_t modelConstantsIndex = INVALID_INDEX_U32;

	uint32_t emissiveTextureIndex = INVALID_INDEX_U32;
	float emissiveStrength = 1.0f;
};

struct DepthOnlyResources
{
	uint32_t cameraConstantsIndex = INVALID_INDEX_U32;
	uint32_t modelConstantsIndex = INVALID_INDEX_U32;
};

struct SkyResources
{
	uint32_t cameraConstantsIndex = INVALID_INDEX_U32;
	uint32_t worldConstantsIndex = INVALID_INDEX_U32;
};

//-----------------------------------------------------------------------------------------------
void AddVertsForColoredCube3D(std::vector<Vertex_PCU>& verts);
void UpdateScreenDimensions();

std::string FormatWorldTime(float totalSeconds);
float GetDayProgress(float totalSeconds);

Camera CreateOrthoCameraForAABB3(AABB3 const& box, Vec3 const& direction);
