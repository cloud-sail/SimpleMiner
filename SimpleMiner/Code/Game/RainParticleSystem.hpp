#pragma once
#include "Engine/Renderer/RendererCommon.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Vec2.hpp"
#include <vector>

//-----------------------------------------------------------------------------------------------
// Rain Particle System with Depth Culling
// - Uses Compute Shader to update particles
// - Uses Compute Shader to cull particles based on top-down depth map
// - Uses Instance Drawing to render visible particles as quads
// - Implements relative coordinate system (particles move relative to player)
//-----------------------------------------------------------------------------------------------

// Must match HLSL struct
struct RainParticle
{
	Vec3 relativePosition;  // Position relative to box center
	float padding0;

	//Vec3 velocity; // #ToDo velocity affects the length of the quad? or directly set in constant buffer // #ToDo Remove Velocity
	//float padding1;
};

//-----------------------------------------------------------------------------------------------
// Resources for Compute Shader (Update)
struct RainUpdateResources
{
	uint32_t particleBufferIndex = INVALID_INDEX_U32;  // RWStructuredBuffer<RainParticle>
	uint32_t rainConstantsIndex = INVALID_INDEX_U32;   // ConstantBuffer<RainConstants>
};

//-----------------------------------------------------------------------------------------------
// Resources for Compute Shader (Cull)
struct RainCullResources
{
	uint32_t particleBufferIndex = INVALID_INDEX_U32;  // StructuredBuffer<RainParticle>
	uint32_t visibilityBufferIndex = INVALID_INDEX_U32; // RWStructuredBuffer<uint>
	uint32_t depthTextureIndex = INVALID_INDEX_U32;    // Texture2D<float>
	uint32_t samplerIndex = INVALID_INDEX_U32;         // SamplerState
	uint32_t rainConstantsIndex = INVALID_INDEX_U32;   // ConstantBuffer<RainConstants>
};

//-----------------------------------------------------------------------------------------------
// Resources for Vertex/Pixel Shader (Render)
struct RainRenderResources
{
	uint32_t particleBufferIndex = INVALID_INDEX_U32;  // StructuredBuffer<RainParticle>
	uint32_t visibilityBufferIndex = INVALID_INDEX_U32; // StructuredBuffer<uint>
	uint32_t cameraConstantsIndex = INVALID_INDEX_U32;
	uint32_t rainConstantsIndex = INVALID_INDEX_U32;
};

//-----------------------------------------------------------------------------------------------
// Constants for particle update and rendering
struct RainConstants
{
	Mat44 worldToClipTransform;   // Orthographic projection from top-down view (World -> Clip)

	float rainColor[4];		// Color of rain particles
	
	Vec2 particleSize;		// Size of rain quad
	uint32_t numParticles;
	float padding0;

	Vec3 velocity;			// the particle is move in uniform speed
	float padding1;			
	
	Vec3 boxCenter;			// Player position / box center (world space)
	float padding2;

	Vec3 boxExtents;		// Half-size of the simulation box
	float padding3;

	Vec3 boxCenterDelta;	// Movement of box center this frame
	float deltaTime;
};

//-----------------------------------------------------------------------------------------------
// Constants for depth culling
//struct RainCullConstants
//{
//	Mat44 worldToClipTransform;   // Orthographic projection from top-down view (World -> Clip)
//
//	Vec3 boxCenter;
//	float cullBias;          // Bias to avoid z-fighting (0.1f recommended) #ToFix: no need to have bias?
//
//	Vec3 boxExtents;
//	uint32_t numParticles;
//};

//-----------------------------------------------------------------------------------------------
// Notes: If the point is not in the 

class RainParticleSystem
{
public:
	RainParticleSystem();
	~RainParticleSystem();

	void Initialize(unsigned int numParticles = 16000);
	void Shutdown();

	void Update(Vec3 const& playerPosition, float deltaSeconds);

	void Render() const;

	uint32_t GetDepthBufferIndex() const { return m_depthDSV.m_index; }
	void PrepareDepthPrePass() const;

	Vec2 GetDepthBufferSize() const { return Vec2(static_cast<float>(m_depthMapResolution), static_cast<float>(m_depthMapResolution)); }

	Vec3 GetVelocity() const { return m_velocity; }
	void SetVelocity(Vec3 const& newVelocity) { m_velocity = newVelocity; }
public:
	Camera m_depthCamera;

private:
	void InitializeParticles();
	void CreateBuffers();
	void DestroyBuffers();

	void CreateDepthTextures();
	void DestroyDepthTextures();

	// Set Render Pipeline, World Renderxxx, EndRenderPipeline

private:
	bool m_isInitialized = false;
	uint32_t m_numParticles = 0;

	// Particle data
	std::vector<RainParticle> m_particlesCPU;  // Initial data on CPU

	// GPU Buffers
	Buffer* m_particleBuffer = nullptr;         // Structured Buffer (read/write)
	DescriptorHandle m_particleBufferSRV;       // For reading in VS/CS
	DescriptorHandle m_particleBufferUAV;       // For writing in CS

	Buffer* m_visibilityBuffer = nullptr;       // Per-particle visibility (uint)
	DescriptorHandle m_visibilityBufferSRV;     // For reading in VS
	DescriptorHandle m_visibilityBufferUAV;     // For writing in CS

	uint32_t m_tempRainConstantsIndex = INVALID_INDEX_U32;

	// #ToDo These Constant Buffer is updated every frame, use a dynamic constant buffer?
	//Buffer* m_constantBuffer = nullptr;
	//DescriptorHandle m_constantBufferCBV;

	//Buffer* m_cullConstantBuffer = nullptr;
	//DescriptorHandle m_cullConstantBufferCBV;

	// Top-down depth map for culling
	Texture* m_depthTexture = nullptr;
	DescriptorHandle m_depthDSV;         // For depth writing
	DescriptorHandle m_depthSRV;         // For reading in cull CS



	// Shaders
	Shader* m_updateShader = nullptr;           // Compute Shader (update physics)
	Shader* m_cullShader = nullptr;             // Compute Shader (depth culling)
	Shader* m_renderShader = nullptr;           // Vertex + Pixel Shader (instance draw)

	// Simulation parameters
	Vec3 m_boxExtents = Vec3(32.0f, 32.0f, 80.0f);		// 96m = 3 chunks width, 160m vertical
	Vec3 m_boxCenterOffset = Vec3(0.0f, 0.0f, 12.0f);	// Offset from player position
	Vec3 m_velocity = Vec3(1.0f, 1.f, -4.0f);         // m/s

	Vec2 m_particleSize = Vec2(0.15f, 0.15f);			// 1cm * 4cm quad

	float m_rainColor[4] = { 0.9f, 0.95f, 1.0f, 0.85f }; // Light blue, semi-transparent, need to be drawn after opaque/lightning, not write the depth

	// Depth culling
	//bool m_enableDepthCulling = true;
	//float m_cullBias = 0.1f;
	unsigned int m_depthMapResolution = 512;  // 512x512 depth map

	// Tracking for relative coordinate system
	Vec3 m_previousBoxCenter = Vec3::ZERO;
};

