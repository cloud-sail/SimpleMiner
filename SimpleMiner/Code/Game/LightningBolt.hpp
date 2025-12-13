#pragma once
#include "Engine/Math/Vec3.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include <vector>

//-----------------------------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------------------------
struct Vertex_PCU;

//-----------------------------------------------------------------------------------------------
// Lightning Segment - represents a single segment of the lightning bolt
//-----------------------------------------------------------------------------------------------
struct LightningSegment
{
	Vec3 start;
	Vec3 end;
	float brightness = 1.0f;           // 0.0 to 1.0, used for dimming branches
	int generation = 0;                // Which subdivision generation this segment belongs to
	bool isMainBranch = true;          // True if part of main trunk, false if side branch

	LightningSegment() = default;
	LightningSegment(Vec3 const& startPos, Vec3 const& endPos, float bright = 1.0f, int gen = 0, bool isMain = true)
		: start(startPos), end(endPos), brightness(bright), generation(gen), isMainBranch(isMain) {
	}

	Vec3 GetMidpoint() const { return (start + end) * 0.5f; }
	float GetLength() const { return (end - start).GetLength(); }
	Vec3 GetDirection() const { return (end - start).GetNormalized(); }
};

//-----------------------------------------------------------------------------------------------
// Lightning Animation Phase - represents different stages of natural lightning
//-----------------------------------------------------------------------------------------------
enum class LightningPhase
{
	LEADER,          // Tree-like branching formation phase (dim, building up)
	RETURN_STROKE_1, // First bright return stroke along main channel
	RETURN_STROKE_2, // Second dimmer return stroke
	RETURN_STROKE_3, // Third even dimmer return stroke
	FADE_OUT,        // Final fade to darkness
	COMPLETE         // Animation finished
};

//-----------------------------------------------------------------------------------------------
// Lightning Bolt Configuration
//-----------------------------------------------------------------------------------------------
// Notes: Try change probability, branch angle
struct LightningConfig
{
	// Generation parameters
	int numGenerations = 6;                  // Number of subdivision iterations (5-7 recommended)
	float maxOffsetDistance = 2.0f;          // Maximum perpendicular offset for first generation
	float branchProbability = 0.3f;          // Probability of creating a branch at each split (0.2-0.4)
	float branchAngleMaxDegrees = 35.0f;     // Maximum angle deviation for branches
	float branchDimmingFactor = 0.7f;        // Brightness multiplier for branches vs main trunk

	// Branch length scaling (key improvement for natural look)
	float baseBranchLengthScale = 0.7f;      // Base scale for branch length
	float minBranchLengthScale = 0.15f;      // Minimum branch length near the end point

	// Animation timing (in seconds)
	float leaderPhaseDuration = 0.15f;       // Leader phase builds up
	float returnStroke1Duration = 0.05f;     // First return stroke (brightest, fastest)
	float returnStroke2Duration = 0.08f;     // Second return stroke
	float returnStroke3Duration = 0.10f;     // Third return stroke
	float fadeOutDuration = 0.20f;           // Final fade out

	// Visual parameters
	float mainTrunkRadius = 0.08f;           // Radius of main lightning trunk
	float branchRadiusScale = 0.6f;          // Branch radius as fraction of trunk
	Rgba8 lightningColor = Rgba8(252, 192, 30);  // Yellow-orange lightning color
	float emissiveStrength = 3.0f;           // Glow strength

	// Jitter animation
	float jitterAmount = 0.02f;              // Per-frame jitter distance for alive effect
	float branchJitterScale = 2.0f;          // Branch endpoints jitter more

	unsigned int randomSeed = 0;             // Seed for deterministic generation
};


//-----------------------------------------------------------------------------------------------
// Lightning Bolt - Main class for generating and animating 3D lightning
//-----------------------------------------------------------------------------------------------
class LightningBolt
{
public:
	LightningBolt(Vec3 const& startPoint, Vec3 const& endPoint, LightningConfig const& config = LightningConfig());
	~LightningBolt() = default;

	// Generation
	void Generate(RandomNumberGenerator& rng);        // Generate the lightning structure
	void Regenerate(RandomNumberGenerator& rng);      // Regenerate with different randomness

	// Animation
	void Update(float deltaSeconds);                  // Update animation state
	void Reset();                                     // Reset to beginning of animation
	bool IsComplete() const { return m_currentPhase == LightningPhase::COMPLETE; }

	// Rendering
	void AddRenderVertices(std::vector<Vertex_PCU>& outVerts, float currentTime) const;

	// Accessors
	LightningPhase GetCurrentPhase() const { return m_currentPhase; }
	float GetAnimationProgress() const { return m_animationTime; }
	Vec3 const& GetStartPoint() const { return m_startPoint; }
	Vec3 const& GetEndPoint() const { return m_endPoint; }
	LightningConfig const& GetConfig() const { return m_config; }

	// Mutators
	void SetStartPoint(Vec3 const& start) { m_startPoint = start; }
	void SetEndPoint(Vec3 const& end) { m_endPoint = end; }
	void SetConfig(LightningConfig const& config) { m_config = config; }

private:
	// Internal generation methods
	void GenerateSegments(RandomNumberGenerator& rng);
	void SubdivideSegment(LightningSegment const& segment, std::vector<LightningSegment>& newSegments,
		int currentGeneration, RandomNumberGenerator& rng);
	Vec3 CalculatePerpendicularOffset(Vec3 const& direction, float offsetAmount, RandomNumberGenerator& rng) const;
	float CalculateBranchLengthScale(Vec3 const& branchPoint) const;
	bool ShouldCreateBranch(int generation, RandomNumberGenerator& rng) const; // #ToFix: Need Probability or don't branch too early or too late?

	// Animation helpers
	void UpdateAnimationPhase(float deltaSeconds); // It will not jump the stage, if game stuttering, the animation will be longer than expected
	float GetSegmentBrightness(LightningSegment const& segment, float phaseProgress) const;
	Vec3 ApplyJitter(Vec3 const& point, LightningSegment const& segment, float time, unsigned int seed) const;

	// Rendering helpers
	void AddSegmentVertices(std::vector<Vertex_PCU>& outVerts, LightningSegment const& segment,
		float brightness, float currentTime) const;
	float CalculateSegmentRadius(LightningSegment const& segment) const;

private:
	// Core data
	Vec3 m_startPoint;
	Vec3 m_endPoint;
	float m_totalDistance;                    // Distance from start to end
	LightningConfig m_config;

	// Generated structure
	std::vector<LightningSegment> m_segments;

	// Animation state
	LightningPhase m_currentPhase;
	float m_animationTime;                    // Total time since animation started
	float m_phaseStartTime;                   // Time when current phase started
};


//-----------------------------------------------------------------------------------------------
// Lightning Manager - Manages multiple lightning bolts (optional convenience class)
//-----------------------------------------------------------------------------------------------
class LightningManager
{
public:
	LightningManager() = default;
	~LightningManager() = default;

	// Bolt management
	int CreateLightning(Vec3 const& start, Vec3 const& end, LightningConfig const& config = LightningConfig());
	void DestroyLightning(int boltIndex);
	void DestroyAllLightning();
	void DestroyCompletedLightning();

	// Updates
	void Update(float deltaSeconds);

	// Rendering
	void Render(std::vector<Vertex_PCU>& outVerts, float currentTime) const;

	// Accessors
	int GetActiveBoltCount() const { return static_cast<int>(m_lightningBolts.size()); }
	LightningBolt* GetBolt(int index);
	LightningBolt const* GetBolt(int index) const;

private:
	std::vector<LightningBolt> m_lightningBolts;
	RandomNumberGenerator m_rng;
};


