#include "Game/LightningBolt.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include <algorithm>

#include "ThirdParty/Noise/SmoothNoise.hpp"

//-----------------------------------------------------------------------------------------------
// LightningBolt Implementation
//-----------------------------------------------------------------------------------------------
LightningBolt::LightningBolt(Vec3 const& startPoint, Vec3 const& endPoint, LightningConfig const& config)
	: m_startPoint(startPoint)
	, m_endPoint(endPoint)
	, m_config(config)
	, m_currentPhase(LightningPhase::LEADER)
	, m_animationTime(0.0f)
	, m_phaseStartTime(0.0f)
{
	m_totalDistance = GetDistance3D(m_startPoint, m_endPoint);
	m_segments.reserve(64); // Pre-allocate for typical lightning bolt
}

//-----------------------------------------------------------------------------------------------
void LightningBolt::Generate(RandomNumberGenerator& rng)
{
	m_segments.clear();
	m_animationTime = 0.0f;
	m_phaseStartTime = 0.0f;
	m_currentPhase = LightningPhase::LEADER;

	GenerateSegments(rng);
}

//-----------------------------------------------------------------------------------------------
void LightningBolt::Regenerate(RandomNumberGenerator& rng)
{
	Generate(rng);
}

//-----------------------------------------------------------------------------------------------
void LightningBolt::Reset()
{
	m_animationTime = 0.0f;
	m_phaseStartTime = 0.0f;
	m_currentPhase = LightningPhase::LEADER;
}

//-----------------------------------------------------------------------------------------------
void LightningBolt::GenerateSegments(RandomNumberGenerator& rng)
{
	// Start with a single segment from start to end
	std::vector<LightningSegment> currentSegments;
	currentSegments.push_back(LightningSegment(m_startPoint, m_endPoint, 1.0f, 0, true));

	//float offsetAmount = m_config.maxOffsetDistance; // #ToFix: Not Used?

	// Subdivide for each generation
	for (int gen = 1; gen <= m_config.numGenerations; ++gen)
	{
		std::vector<LightningSegment> newSegments;
		newSegments.reserve(currentSegments.size() * 3); // Worst case: every segment branches

		// Process each segment from the current generation
		for (LightningSegment const& segment : currentSegments)
		{
			SubdivideSegment(segment, newSegments, gen, rng);
		}

		// Move new segments to current for next iteration
		currentSegments = std::move(newSegments);

		// Reduce offset amount for next generation
		//offsetAmount *= 0.5f; // #ToFix: Not Used?
	}

	// Final segments are our lightning bolt
	m_segments = std::move(currentSegments);
}

//-----------------------------------------------------------------------------------------------
void LightningBolt::SubdivideSegment(LightningSegment const& segment, std::vector<LightningSegment>& newSegments,
	int currentGeneration, RandomNumberGenerator& rng)
{
	// Calculate midpoint
	Vec3 midpoint = segment.GetMidpoint();

	// Apply perpendicular offset
	float offsetAmount = m_config.maxOffsetDistance * powf(0.5f, static_cast<float>(currentGeneration - 1));
	Vec3 offset = CalculatePerpendicularOffset(segment.GetDirection(), offsetAmount, rng);
	midpoint += offset;

	// Create two new segments for the main path
	LightningSegment segment1(segment.start, midpoint, segment.brightness, currentGeneration, segment.isMainBranch);
	LightningSegment segment2(midpoint, segment.end, segment.brightness, currentGeneration, segment.isMainBranch);

	newSegments.push_back(segment1);
	newSegments.push_back(segment2);

	// Possibly create a branch
	if (ShouldCreateBranch(currentGeneration, rng))
	{
		// Calculate branch length based on distance to end point
		float branchLengthScale = CalculateBranchLengthScale(midpoint);

		// Direction from segment start to midpoint
		Vec3 branchDirection = (midpoint - segment.start);
		float branchBaseLength = branchDirection.GetLength();
		branchDirection = branchDirection.GetNormalized();

		// Add random rotation to branch direction
		float angleDeviation = rng.RollRandomFloatInRange(-m_config.branchAngleMaxDegrees, m_config.branchAngleMaxDegrees);


		// Create a random perpendicular vector for rotation axis
		Vec3 randomAxis = CalculatePerpendicularOffset(branchDirection, 1.0f, rng).GetNormalized();

		// Rotate branch direction around the random axis
		// Simplified rotation: just add perpendicular component
		Vec3 perpComponent = randomAxis * SinDegrees(angleDeviation);
		Vec3 parallelComponent = branchDirection * CosDegrees(angleDeviation);
		Vec3 rotatedDirection = (parallelComponent + perpComponent).GetNormalized();

		// Calculate branch endpoint
		float branchLength = branchBaseLength * branchLengthScale;
		Vec3 branchEnd = midpoint + rotatedDirection * branchLength;

		// Create branch segment with reduced brightness
		float branchBrightness = segment.brightness * m_config.branchDimmingFactor;
		LightningSegment branchSegment(midpoint, branchEnd, branchBrightness, currentGeneration, false);

		newSegments.push_back(branchSegment);
	}
}

//-----------------------------------------------------------------------------------------------
Vec3 LightningBolt::CalculatePerpendicularOffset(Vec3 const& direction, float offsetAmount, RandomNumberGenerator& rng) const
{
	// Find two perpendicular vectors to the direction
	Vec3 perpendicular1;
	Vec3 perpendicular2;

	// Choose a vector that's not parallel to direction
	Vec3 notParallel = (fabsf(direction.z) < 0.9f) ? Vec3::ZAXIS : Vec3::XAXIS;

	// Generate two perpendicular vectors using cross product
	perpendicular1 = CrossProduct3D(direction, notParallel).GetNormalized();
	perpendicular2 = CrossProduct3D(direction, perpendicular1).GetNormalized();

	// Random offset in the perpendicular plane
	float randomOffset1 = rng.RollRandomFloatInRange(-offsetAmount, offsetAmount);
	float randomOffset2 = rng.RollRandomFloatInRange(-offsetAmount, offsetAmount);

	return perpendicular1 * randomOffset1 + perpendicular2 * randomOffset2;
}

//-----------------------------------------------------------------------------------------------
float LightningBolt::CalculateBranchLengthScale(Vec3 const& branchPoint) const
{
	// Calculate distance from branch point to end point
	float distanceToEnd = GetDistance3D(branchPoint, m_endPoint);

	// Calculate ratio (0.0 at end, 1.0 at start)
	float distanceRatio = distanceToEnd / m_totalDistance;

	// Interpolate between min and max branch length
	float branchLengthScale = Interpolate(m_config.minBranchLengthScale,
		m_config.baseBranchLengthScale,
		distanceRatio);

	return branchLengthScale;
}

//-----------------------------------------------------------------------------------------------
bool LightningBolt::ShouldCreateBranch(int generation, RandomNumberGenerator& rng) const
{
	// Don't branch too early or too late
	if (generation < 2 || generation > m_config.numGenerations - 1)
	{
		return false;
	}

	// Random chance based on configured probability
	return rng.RollRandomWithProbability(m_config.branchProbability);
}

//-----------------------------------------------------------------------------------------------
void LightningBolt::Update(float deltaSeconds)
{
	if (m_currentPhase == LightningPhase::COMPLETE)
	{
		return;
	}

	m_animationTime += deltaSeconds;
	UpdateAnimationPhase(deltaSeconds);
}

//-----------------------------------------------------------------------------------------------
void LightningBolt::UpdateAnimationPhase(float deltaSeconds)
{
	UNUSED(deltaSeconds);
	float phaseTime = m_animationTime - m_phaseStartTime;

	switch (m_currentPhase)
	{
	case LightningPhase::LEADER:
		if (phaseTime >= m_config.leaderPhaseDuration)
		{
			m_currentPhase = LightningPhase::RETURN_STROKE_1;
			m_phaseStartTime = m_animationTime;
		}
		break;

	case LightningPhase::RETURN_STROKE_1:
		if (phaseTime >= m_config.returnStroke1Duration)
		{
			m_currentPhase = LightningPhase::RETURN_STROKE_2;
			m_phaseStartTime = m_animationTime;
		}
		break;

	case LightningPhase::RETURN_STROKE_2:
		if (phaseTime >= m_config.returnStroke2Duration)
		{
			m_currentPhase = LightningPhase::RETURN_STROKE_3;
			m_phaseStartTime = m_animationTime;
		}
		break;

	case LightningPhase::RETURN_STROKE_3:
		if (phaseTime >= m_config.returnStroke3Duration)
		{
			m_currentPhase = LightningPhase::FADE_OUT;
			m_phaseStartTime = m_animationTime;
		}
		break;

	case LightningPhase::FADE_OUT:
		if (phaseTime >= m_config.fadeOutDuration)
		{
			m_currentPhase = LightningPhase::COMPLETE;
		}
		break;

	case LightningPhase::COMPLETE:
		break;
	}
}

//-----------------------------------------------------------------------------------------------
float LightningBolt::GetSegmentBrightness(LightningSegment const& segment, float phaseProgress) const
{
	float baseBrightness = segment.brightness;

	switch (m_currentPhase)
	{
	case LightningPhase::LEADER:
		// Leader phase: gradually build up, branches visible
		// Use SmoothStart3 for organic growth feeling
		return baseBrightness * 0.4f * SmoothStart3(phaseProgress);

	case LightningPhase::RETURN_STROKE_1:
	{
		// First return stroke: FLASH - quick rise to peak, then fast decay
		// Peak brightness at 15% of phase, then rapid decay
		float peakPosition = 0.15f;
		float pulseBrightness;

		if (phaseProgress < peakPosition)
		{
			// Fast rise using SmoothStart2 (accelerating to peak)
			float riseProgress = phaseProgress / peakPosition;
			pulseBrightness = SmoothStart2(riseProgress);
		}
		else
		{
			// Very fast decay using inverse SmoothStart5
			float decayProgress = (phaseProgress - peakPosition) / (1.0f - peakPosition);
			pulseBrightness = 1.0f - SmoothStart5(decayProgress); // 1 - t^5 for sharp decay
		}

		if (segment.isMainBranch)
		{
			return baseBrightness * 2.0f * pulseBrightness; // Very bright flash!
		}
		else
		{
			return baseBrightness * 0.8f * pulseBrightness; // Branches flash too
		}
	}

	case LightningPhase::RETURN_STROKE_2:
	{
		// Second return stroke: medium flash with moderate decay
		float peakPosition = 0.15f;
		float pulseBrightness;

		if (phaseProgress < peakPosition)
		{
			// Fast rise using SmoothStart2
			float riseProgress = phaseProgress / peakPosition;
			pulseBrightness = SmoothStart2(riseProgress);
		}
		else
		{
			// Moderate decay using inverse SmoothStart4
			float decayProgress = (phaseProgress - peakPosition) / (1.0f - peakPosition);
			pulseBrightness = 1.0f - SmoothStart4(decayProgress); // 1 - t^4
		}

		if (segment.isMainBranch)
		{
			return baseBrightness * 1.2f * pulseBrightness; // Medium bright
		}
		else
		{
			return baseBrightness * 0.5f * pulseBrightness;
		}
	}

	case LightningPhase::RETURN_STROKE_3:
	{
		// Third return stroke: weakest flash with slower decay
		float peakPosition = 0.15f;
		float pulseBrightness;

		if (phaseProgress < peakPosition)
		{
			// Slower rise using SmoothStart2
			float riseProgress = phaseProgress / peakPosition;
			pulseBrightness = SmoothStart2(riseProgress);
		}
		else
		{
			// Slower decay using inverse SmoothStart3
			float decayProgress = (phaseProgress - peakPosition) / (1.0f - peakPosition);
			pulseBrightness = 1.0f - SmoothStart3(decayProgress); // 1 - t^3
		}

		if (segment.isMainBranch)
		{
			return baseBrightness * 0.8f * pulseBrightness; // Dim flash
		}
		else
		{
			return baseBrightness * 0.3f * pulseBrightness;
		}
	}

	case LightningPhase::FADE_OUT:
		// Fade out: smooth fade using SmoothStart3
		return baseBrightness * (1.0f - SmoothStart3(phaseProgress));

	case LightningPhase::COMPLETE:
		return 0.0f;
	}

	return baseBrightness;
}

//-----------------------------------------------------------------------------------------------
Vec3 LightningBolt::ApplyJitter(Vec3 const& point, LightningSegment const& segment, float time, unsigned int seed) const
{
	if (m_config.jitterAmount <= 0.f)
	{
		return point;
	}

	float temporalScale = 0.1f;

	// 100.f is a big number
	float jitterX = Compute1dFractalNoise(
		time / temporalScale + point.x * 100.0f,
		1.0f, 1, 0.5f, 2.0f, true, seed
	);

	float jitterY = Compute1dFractalNoise(
		time / temporalScale + point.y * 100.0f,
		1.0f, 1, 0.5f, 2.0f, true, seed + 1
	);

	float jitterZ = Compute1dFractalNoise(
		time / temporalScale + point.z * 100.0f,
		1.0f, 1, 0.5f, 2.0f, true, seed + 2
	);

	Vec3 jitter(jitterX, jitterY, jitterZ);

	// Scale jitter based on whether it's a branch endpoint
	float jitterScale = segment.isMainBranch ? 1.0f : m_config.branchJitterScale;

	return point + jitter * m_config.jitterAmount * jitterScale;
}

//-----------------------------------------------------------------------------------------------
void LightningBolt::AddRenderVertices(std::vector<Vertex_PCU>& outVerts, float currentTime) const
{
	if (m_currentPhase == LightningPhase::COMPLETE)
	{
		return;
	}

	float phaseTime = m_animationTime - m_phaseStartTime;
	float phaseProgress = 0.0f;

	// Calculate phase progress
	switch (m_currentPhase)
	{
	case LightningPhase::LEADER:
		phaseProgress = phaseTime / m_config.leaderPhaseDuration;
		break;
	case LightningPhase::RETURN_STROKE_1:
		phaseProgress = phaseTime / m_config.returnStroke1Duration;
		break;
	case LightningPhase::RETURN_STROKE_2:
		phaseProgress = phaseTime / m_config.returnStroke2Duration;
		break;
	case LightningPhase::RETURN_STROKE_3:
		phaseProgress = phaseTime / m_config.returnStroke3Duration;
		break;
	case LightningPhase::FADE_OUT:
		phaseProgress = phaseTime / m_config.fadeOutDuration;
		break;
	case LightningPhase::COMPLETE:
		return;
	}

	phaseProgress = GetClamped(phaseProgress, 0.0f, 1.0f);

	// Render each segment
	for (LightningSegment const& segment : m_segments)
	{
		float brightness = GetSegmentBrightness(segment, phaseProgress);
		if (brightness > 0.01f) // Skip nearly invisible segments
		{
			AddSegmentVertices(outVerts, segment, brightness, currentTime);
		}
	}
}

//-----------------------------------------------------------------------------------------------
void LightningBolt::AddSegmentVertices(std::vector<Vertex_PCU>& outVerts, LightningSegment const& segment,
	float brightness, float currentTime) const
{
	// Apply jitter to endpoints for alive effect
	Vec3 jitteredStart = ApplyJitter(segment.start, segment, currentTime, m_config.randomSeed);
	Vec3 jitteredEnd = ApplyJitter(segment.end, segment, currentTime, m_config.randomSeed + 1000);

	// Calculate radius
	float radius = CalculateSegmentRadius(segment);

	// Calculate color with brightness // #ToFix is it wrong? brightness range?
	// RGB: Keep base lightning color unchanged
	// Alpha: Encode brightness for shader (0.0~2.0+ maps to 0~255)
	Rgba8 color = m_config.lightningColor;

	// Encode brightness into alpha channel
	// brightness range: 0.0 ~ 2.0+ (can exceed 1.0 for bloom)
	float brightnessEncoded = brightness * 128.f;
	color.a = static_cast<unsigned char>(GetClamped(brightnessEncoded, 0.0f, 255.0f));

	// Use cylinder with 3 slices for efficient rendering
	AddVertsForSphere3D(outVerts, jitteredStart, radius, color, AABB2::ZERO_TO_ONE, 8, 4);
	AddVertsForCylinder3D(outVerts, jitteredStart, jitteredEnd, radius, color, AABB2::ZERO_TO_ONE, 4);
}

//-----------------------------------------------------------------------------------------------
float LightningBolt::CalculateSegmentRadius(LightningSegment const& segment) const
{
	if (segment.isMainBranch)
	{
		return m_config.mainTrunkRadius;
	}
	else
	{
		return m_config.mainTrunkRadius * m_config.branchRadiusScale;
	}
}

//-----------------------------------------------------------------------------------------------
// LightningManager Implementation
//-----------------------------------------------------------------------------------------------
int LightningManager::CreateLightning(Vec3 const& start, Vec3 const& end, LightningConfig const& config)
{
	LightningBolt newBolt(start, end, config);
	newBolt.Generate(m_rng);
	m_lightningBolts.push_back(newBolt);
	return static_cast<int>(m_lightningBolts.size()) - 1;
}

//-----------------------------------------------------------------------------------------------
void LightningManager::DestroyLightning(int boltIndex)
{
	if (boltIndex >= 0 && boltIndex < static_cast<int>(m_lightningBolts.size()))
	{
		m_lightningBolts.erase(m_lightningBolts.begin() + boltIndex);
	}
}

//-----------------------------------------------------------------------------------------------
void LightningManager::DestroyAllLightning()
{
	m_lightningBolts.clear();
}

//-----------------------------------------------------------------------------------------------
void LightningManager::DestroyCompletedLightning()
{
	m_lightningBolts.erase(
		std::remove_if(m_lightningBolts.begin(), m_lightningBolts.end(),
			[](LightningBolt const& bolt) { return bolt.IsComplete(); }),
		m_lightningBolts.end()
	);
}

//-----------------------------------------------------------------------------------------------
void LightningManager::Update(float deltaSeconds)
{
	for (LightningBolt& bolt : m_lightningBolts)
	{
		bolt.Update(deltaSeconds);
	}
}

//-----------------------------------------------------------------------------------------------
void LightningManager::Render(std::vector<Vertex_PCU>& outVerts, float currentTime) const
{
	for (LightningBolt const& bolt : m_lightningBolts)
	{
		bolt.AddRenderVertices(outVerts, currentTime);
	}
}

//-----------------------------------------------------------------------------------------------
LightningBolt* LightningManager::GetBolt(int index)
{
	if (index >= 0 && index < static_cast<int>(m_lightningBolts.size()))
	{
		return &m_lightningBolts[index];
	}
	return nullptr;
}

//-----------------------------------------------------------------------------------------------
LightningBolt const* LightningManager::GetBolt(int index) const
{
	if (index >= 0 && index < static_cast<int>(m_lightningBolts.size()))
	{
		return &m_lightningBolts[index];
	}
	return nullptr;
}
