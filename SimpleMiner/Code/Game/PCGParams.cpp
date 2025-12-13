#include "Game/PCGParams.hpp"
#include "Game/ChunkUtils.hpp"
#include "Game/BlockDefinition.hpp"

#include "ThirdParty/Noise/SmoothNoise.hpp"

// Continentalness Mapper
GameRangeMapper<Continentalness> NoiseRangeMappers::m_continentalnessMapper = []() {
	GameRangeMapper<Continentalness> mapper;
	mapper.AddRange(-1.20f, -0.455f, Continentalness::DEEP_OCEAN);
	mapper.AddRange(-0.455f, -0.19f, Continentalness::OCEAN);
	mapper.AddRange(-0.19f, -0.11f, Continentalness::COAST);
	mapper.AddRange(-0.11f, 0.03f, Continentalness::NEAR_INLAND);
	mapper.AddRange(0.03f, 0.30f, Continentalness::MID_INLAND);
	mapper.AddRange(0.30f, 1.00f, Continentalness::FAR_INLAND);
	return mapper;
	}();

// Erosion Mapper
GameRangeMapper<Erosion> NoiseRangeMappers::m_erosionMapper = []() {
	GameRangeMapper<Erosion> mapper;
	mapper.AddRange(-1.00f, -0.78f, Erosion::E0);
	mapper.AddRange(-0.78f, -0.375f, Erosion::E1);
	mapper.AddRange(-0.375f, -0.2225f, Erosion::E2);
	mapper.AddRange(-0.2225f, 0.05f, Erosion::E3);
	mapper.AddRange(0.05f, 0.45f, Erosion::E4);
	mapper.AddRange(0.45f, 0.55f, Erosion::E5);
	mapper.AddRange(0.55f, 1.00f, Erosion::E6);
	return mapper;
	}();

// Peaks and Valleys Mapper
GameRangeMapper<PeaksValleys> NoiseRangeMappers::m_peaksValleysMapper = []() {
	GameRangeMapper<PeaksValleys> mapper;
	mapper.AddRange(-1.00f, -0.85f, PeaksValleys::VALLEYS);
	mapper.AddRange(-0.85f, -0.2f, PeaksValleys::LOW);
	mapper.AddRange(-0.2f, 0.2f, PeaksValleys::MID);
	mapper.AddRange(0.2f, 0.7f, PeaksValleys::HIGH);
	mapper.AddRange(0.7f, 1.0f, PeaksValleys::PEAKS);
	return mapper;
	}();

// Temperature Mapper
GameRangeMapper<Temperature> NoiseRangeMappers::m_temperatureMapper = []() {
	GameRangeMapper<Temperature> mapper;
	mapper.AddRange(-1.00f, -0.45f, Temperature::T0);
	mapper.AddRange(-0.45f, -0.15f, Temperature::T1);
	mapper.AddRange(-0.15f, 0.20f, Temperature::T2);
	mapper.AddRange(0.20f, 0.55f, Temperature::T3);
	mapper.AddRange(0.55f, 1.00f, Temperature::T4);
	return mapper;
	}();

// Humidity Mapper
GameRangeMapper<Humidity> NoiseRangeMappers::m_humidityMapper = []() {
	GameRangeMapper<Humidity> mapper;
	mapper.AddRange(-1.00f, -0.35f, Humidity::H0);
	mapper.AddRange(-0.35f, -0.10f, Humidity::H1);
	mapper.AddRange(-0.10f, 0.10f, Humidity::H2);
	mapper.AddRange(0.10f, 0.30f, Humidity::H3);
	mapper.AddRange(0.30f, 1.00f, Humidity::H4);
	return mapper;
	}();

//-----------------------------------------------------------------------------------------------
BiomeType BiomeSelector::SelectBiome(Continentalness cont, Erosion ero, PeaksValleys pv, Temperature temp, Humidity hum)
{
	// First check if it's non-inland (ocean or deep ocean)
	if (cont == Continentalness::DEEP_OCEAN)
	{
		return SelectDeepOceanBiome(temp);
	}

	if (cont == Continentalness::OCEAN)
	{
		return SelectOceanBiome(temp);
	}

	// Otherwise it's inland
	return SelectInlandBiome(cont, ero, pv, temp, hum);
}

BiomeType BiomeSelector::SelectDeepOceanBiome(Temperature temp)
{
	return (temp == Temperature::T0) ? BiomeType::FROZEN_OCEAN : BiomeType::DEEP_OCEAN;
}

BiomeType BiomeSelector::SelectOceanBiome(Temperature temp)
{
	return (temp == Temperature::T0) ? BiomeType::FROZEN_OCEAN : BiomeType::OCEAN;
}

BiomeType BiomeSelector::SelectBeachBiome(Temperature temp)
{
	if (temp == Temperature::T0)
		return BiomeType::SNOWY_BEACH;
	else if (temp == Temperature::T4)
		return BiomeType::DESERT;
	else // T1, T2, T3
		return BiomeType::BEACH;
}

BiomeType BiomeSelector::SelectBadlandBiome(Humidity hum)
{
	if (hum == Humidity::H0 || hum == Humidity::H1 || hum == Humidity::H2)
		return BiomeType::DESERT;
	else // H3, H4
		return BiomeType::SAVANNA;
}

BiomeType BiomeSelector::SelectMiddleBiome(Temperature temp, Humidity hum)
{
	if (temp == Temperature::T0)
	{
		if (hum == Humidity::H0 || hum == Humidity::H1)
			return BiomeType::SNOWY_PLAINS;
		else if (hum == Humidity::H2 || hum == Humidity::H3)
			return BiomeType::SNOWY_TAIGA;
		else // H4
			return BiomeType::TAIGA;
	}
	else if (temp == Temperature::T1)
	{
		if (hum == Humidity::H0 || hum == Humidity::H1)
			return BiomeType::PLAINS;
		else if (hum == Humidity::H2)
			return BiomeType::FOREST;
		else // H3, H4
			return BiomeType::TAIGA;
	}
	else if (temp == Temperature::T2)
	{
		if (hum == Humidity::H0 || hum == Humidity::H1)
			return BiomeType::PLAINS;
		else if (hum == Humidity::H2 || hum == Humidity::H3)
			return BiomeType::FOREST;
		else // H4
			return BiomeType::TAIGA;
	}
	else if (temp == Temperature::T3)
	{
		if (hum == Humidity::H0 || hum == Humidity::H1)
			return BiomeType::SAVANNA;
		else if (hum == Humidity::H2)
			return BiomeType::PLAINS;
		else // H3, H4
			return BiomeType::JUNGLE;
	}
	else // T4
	{
		return BiomeType::DESERT;
	}
}

BiomeType BiomeSelector::SelectInlandBiome(Continentalness cont, Erosion ero, PeaksValleys pv, Temperature temp, Humidity hum)
{
	// Valleys
	if (pv == PeaksValleys::VALLEYS)
	{
		if (cont == Continentalness::COAST)
			return SelectBeachBiome(temp);
		else // NEAR_INLAND, MID_INLAND, FAR_INLAND
		{
			if (temp == Temperature::T4)
				return SelectBadlandBiome(hum);
			else // T < 4
				return SelectMiddleBiome(temp, hum);
		}
	}

	// Low
	else if (pv == PeaksValleys::LOW)
	{
		if (ero == Erosion::E0 || ero == Erosion::E1)
		{
			if (cont == Continentalness::COAST)
				return SelectBeachBiome(temp);
			else // NEAR_INLAND, MID_INLAND, FAR_INLAND
			{
				if (temp == Temperature::T4)
					return SelectBadlandBiome(hum);
				else // T < 4
					return SelectMiddleBiome(temp, hum);
			}
		}
		else if (ero == Erosion::E2 || ero == Erosion::E3)
		{
			if (cont == Continentalness::COAST)
				return SelectBeachBiome(temp);
			else if (cont == Continentalness::NEAR_INLAND)
				return SelectMiddleBiome(temp, hum);
			else // MID_INLAND, FAR_INLAND
			{
				if (temp == Temperature::T4)
					return SelectBadlandBiome(hum);
				else // T < 4
					return SelectMiddleBiome(temp, hum);
			}
		}
		else // E4, E5, E6
		{
			if (cont == Continentalness::COAST)
				return SelectBeachBiome(temp);
			else
				return SelectMiddleBiome(temp, hum);
		}
	}

	// Mid
	else if (pv == PeaksValleys::MID)
	{
		if (ero == Erosion::E0 || ero == Erosion::E1)
		{
			if (cont == Continentalness::COAST)
				return SelectBeachBiome(temp);
			else // NEAR_INLAND, MID_INLAND, FAR_INLAND
			{
				if (temp == Temperature::T4)
					return SelectBadlandBiome(hum);
				else // T < 4
					return SelectMiddleBiome(temp, hum);
			}
		}
		else if (ero == Erosion::E2)
		{
			if (cont == Continentalness::COAST)
				return SelectBeachBiome(temp);
			else if (cont == Continentalness::MID_INLAND)
			{
				if (temp == Temperature::T4)
					return SelectBadlandBiome(hum);
				else
					return SelectMiddleBiome(temp, hum);
			}
			else // NEAR_INLAND, FAR_INLAND
				return SelectMiddleBiome(temp, hum);
		}
		else if (ero == Erosion::E3)
		{
			if (cont == Continentalness::COAST || cont == Continentalness::NEAR_INLAND)
				return SelectMiddleBiome(temp, hum);
			else // MID_INLAND, FAR_INLAND
			{
				if (temp == Temperature::T4)
					return SelectBadlandBiome(hum);
				else
					return SelectMiddleBiome(temp, hum);
			}
		}
		else if (ero == Erosion::E4)
		{
			return SelectMiddleBiome(temp, hum);
		}
		else // E5, E6
		{
			if (cont == Continentalness::COAST)
				return SelectBeachBiome(temp);
			else // NEAR_INLAND, MID_INLAND, FAR_INLAND
				return SelectMiddleBiome(temp, hum);
		}
	}

	// High
	else if (pv == PeaksValleys::HIGH)
	{
		if (ero == Erosion::E0)
		{
			if (cont == Continentalness::COAST || cont == Continentalness::NEAR_INLAND)
				return SelectMiddleBiome(temp, hum);
			else // MID_INLAND, FAR_INLAND
			{
				if (temp == Temperature::T0 || temp == Temperature::T1 || temp == Temperature::T2)
					return BiomeType::SNOWY_PEAKS;
				else // T3, T4
					return BiomeType::STONY_PEAKS;
			}
		}
		else if (ero == Erosion::E1)
		{
			if (cont == Continentalness::COAST)
				return SelectMiddleBiome(temp, hum);
			else if (cont == Continentalness::NEAR_INLAND)
			{
				if (temp == Temperature::T4)
					return SelectBadlandBiome(hum);
				else
					return SelectMiddleBiome(temp, hum);
			}
			else // MID_INLAND, FAR_INLAND
				return SelectMiddleBiome(temp, hum);
		}
		else if (ero == Erosion::E2)
		{
			return SelectMiddleBiome(temp, hum);
		}
		else if (ero == Erosion::E3)
		{
			if (cont == Continentalness::COAST || cont == Continentalness::NEAR_INLAND || cont == Continentalness::FAR_INLAND)
				return SelectMiddleBiome(temp, hum);
			else // MID_INLAND
			{
				if (temp == Temperature::T4)
					return SelectBadlandBiome(hum);
				else
					return SelectMiddleBiome(temp, hum);
			}
		}
		else // E4, E5, E6
		{
			return SelectMiddleBiome(temp, hum);
		}
	}

	// Peaks
	else // pv == PeaksValleys::PEAKS
	{
		if (ero == Erosion::E0)
		{
			if (temp == Temperature::T0 || temp == Temperature::T1 || temp == Temperature::T2)
				return BiomeType::SNOWY_PEAKS;
			else // T3, T4
				return BiomeType::STONY_PEAKS;
		}
		else if (ero == Erosion::E1)
		{
			if (cont == Continentalness::COAST || cont == Continentalness::NEAR_INLAND)
			{
				if (temp == Temperature::T4)
					return SelectBadlandBiome(hum);
				else
					return SelectMiddleBiome(temp, hum);
			}
			else
			{
				if (temp == Temperature::T0 || temp == Temperature::T1 || temp == Temperature::T2)
					return BiomeType::SNOWY_PEAKS;
				else // T3, T4
					return BiomeType::STONY_PEAKS;
			}
		}
		else if (ero == Erosion::E2)
		{
			return SelectMiddleBiome(temp, hum);
		}
		else if (ero == Erosion::E3)
		{
			if (temp == Temperature::T4 && cont == Continentalness::MID_INLAND)
				return SelectBadlandBiome(hum);
			else
				return SelectMiddleBiome(temp, hum);
		}
		else // E4, E5, E6
		{
			return SelectMiddleBiome(temp, hum);
		}
	}
}

void PCGParams::InitializeDefaultParams()
{
	// ===== 2D Noise Parameters =====
	m_continentNoise = PerlinNoiseParams(
		1024.f,							// Scale
		4,								// Octaves
		DEFAULT_PERSISTANCE,			// Persistence
		DEFAULT_OCTAVE_SCALE,			// Octave Scale
		10								// Seed
	);

	m_erosionNoise = PerlinNoiseParams(
		256.f,							// Scale
		4,								// Octaves
		DEFAULT_PERSISTANCE,			// Persistence
		DEFAULT_OCTAVE_SCALE,			// Octave Scale
		20								// Seed
	);

	m_peaksValleysNoise = PerlinNoiseParams(
		256.f,							// Scale
		4,								// Octaves
		DEFAULT_PERSISTANCE,			// Persistence
		DEFAULT_OCTAVE_SCALE,			// Octave Scale
		30								// Seed
	);

	m_temperatureNoise = PerlinNoiseParams(
		256.f,							// Scale
		2,								// Octaves
		DEFAULT_PERSISTANCE,			// Persistence
		DEFAULT_OCTAVE_SCALE,			// Octave Scale
		40								// Seed
	);

	m_humidityNoise = PerlinNoiseParams(
		256.f,							// Scale
		3,								// Octaves
		DEFAULT_PERSISTANCE,			// Persistence
		DEFAULT_OCTAVE_SCALE,			// Octave Scale
		50								// Seed
	);

	// ===== 3D Noise Parameters =====
	m_densityNoise = PerlinNoiseParams(
		64.f,							// Scale
		4,								// Octaves
		DEFAULT_PERSISTANCE,			// Persistence
		DEFAULT_OCTAVE_SCALE,			// Octave Scale
		100								// Seed
	);

	// ===== Height Offset Spline =====
	// Input: Continentalness [-1, 1]
	// Output: Height Offset [-1, 1]
	m_heightOffsetSpline.ClearAllSplinePoints();
	m_heightOffsetSpline.AddPoint(SplinePoint1D(-1.f, 1.f, 0.f, -20.f, CurveMode::CURVE));
	m_heightOffsetSpline.AddPoint(SplinePoint1D(-0.9f, -0.95f, -10.f, 0.f, CurveMode::CURVE));
	m_heightOffsetSpline.AddPoint(SplinePoint1D(-0.455f, -0.95f, 0.f, 0.f, CurveMode::CURVE));
	m_heightOffsetSpline.AddPoint(SplinePoint1D(-0.4f, -0.33f, 0.f, 0.f, CurveMode::CURVE));
	m_heightOffsetSpline.AddPoint(SplinePoint1D(-0.25f, -0.33f, 0.f, 0.f, CurveMode::CURVE));
	m_heightOffsetSpline.AddPoint(SplinePoint1D(-0.19f, 0.f, 0.f, 0.f, CurveMode::CURVE));
	m_heightOffsetSpline.AddPoint(SplinePoint1D(-0.15f, 0.f, 0.f, 0.f, CurveMode::CURVE));
	m_heightOffsetSpline.AddPoint(SplinePoint1D(-0.11f, 0.1f, 0.f, 0.f, CurveMode::CURVE));
	m_heightOffsetSpline.AddPoint(SplinePoint1D(0.05f, 0.3f, 0.f, 0.f, CurveMode::CURVE));
	m_heightOffsetSpline.AddPoint(SplinePoint1D(0.35f, 0.4f, 0.f, 0.f, CurveMode::CURVE));
	m_heightOffsetSpline.AddPoint(SplinePoint1D(0.65f, 0.5f, 0.f, 0.f, CurveMode::CURVE));
	m_heightOffsetSpline.AddPoint(SplinePoint1D(0.85f, 0.85f, 0.f, 0.f, CurveMode::CURVE));
	m_heightOffsetSpline.AddPoint(SplinePoint1D(1.f, 1.f, 0.f, 0.f, CurveMode::CURVE));


	// ===== Squashing Spline =====
	// Input: Continentalness [-1, 1]
	// Output: squashing factor [0, 1] greater -> flatter
	m_squashingSpline.ClearAllSplinePoints();
	m_squashingSpline.AddPoint(SplinePoint1D(-1.f, 0.f, 0.f, 0.f, CurveMode::CURVE));
	m_squashingSpline.AddPoint(SplinePoint1D(-0.455f, 0.1f, 0.f, 0.f, CurveMode::CURVE));
	m_squashingSpline.AddPoint(SplinePoint1D(0.f, 0.3f, 0.f, 0.f, CurveMode::CURVE));
	m_squashingSpline.AddPoint(SplinePoint1D(0.4f, 0.7f, 0.f, 0.f, CurveMode::CURVE));
	m_squashingSpline.AddPoint(SplinePoint1D(0.7f, 0.4f, 0.f, 0.f, CurveMode::CURVE));
	m_squashingSpline.AddPoint(SplinePoint1D(1.0f, 0.0f, 0.f, 0.f, CurveMode::CURVE));

	// ===== Erosion Factor Spline =====
	// Input: Erosion [-1, 1]
	// Output: Erosion Factor/Intensity [0, 1]
	m_erosionFactorSpline.ClearAllSplinePoints();
	m_erosionFactorSpline.AddPoint(SplinePoint1D(-1.0f, 0.0f)); 
	m_erosionFactorSpline.AddPoint(SplinePoint1D(-0.5f, 0.15f));
	m_erosionFactorSpline.AddPoint(SplinePoint1D(0.0f, 0.35f)); 
	m_erosionFactorSpline.AddPoint(SplinePoint1D(0.5f, 0.65f)); 
	m_erosionFactorSpline.AddPoint(SplinePoint1D(1.0f, 1.0f));  

	// ===== Bias Parameters =====
	m_baseHeight = static_cast<float>(CHUNK_SIZE_Z) / 2.f;
	m_biasStrengthMin = 0.015f;
	m_biasStrengthMax = 0.025f;

	m_biasStrengthMin = 0.012f;
	m_biasStrengthMax = 0.024f;

	// ===== Peaks/Valleys Parameters =====
	m_peaksValleysStrength = 1.0f;
}

float PerlinNoiseParams::SamplePerlinNoise2D(int globalX, int globalY) const
{
	return Compute2dPerlinNoise(
		static_cast<float>(globalX),
		static_cast<float>(globalY),
		m_scale,
		m_numOctaves,
		m_octavePersistence,
		m_octaveScale,
		true,
		m_seed
	);
}

float PerlinNoiseParams::SamplePerlinNoise3D(int globalX, int globalY, int globalZ) const
{
	return Compute3dPerlinNoise(
		static_cast<float>(globalX),
		static_cast<float>(globalY),
		static_cast<float>(globalZ),
		m_scale,
		m_numOctaves,
		m_octavePersistence,
		m_octaveScale,
		true,
		m_seed
	);
}

float CalculatePV(float weirdness)
{
	return 1.0f - fabsf(3.0f * fabsf(weirdness) - 2.0f);
}

//-----------------------------------------------------------------------------------------------
std::map<TreeType, TreeStamp> TreeGenerator::s_treeStamps;

void TreeGenerator::InitializeTreeStamps()
{
	// Get block type IDs
	const uint8_t oakLogType = BlockDefinition::GetBlockTypeIDByName("OakLog");
	const uint8_t oakLeavesType = BlockDefinition::GetBlockTypeIDByName("OakLeaves");
	const uint8_t birchLogType = BlockDefinition::GetBlockTypeIDByName("BirchLog");
	const uint8_t birchLeavesType = BlockDefinition::GetBlockTypeIDByName("BirchLeaves");
	const uint8_t spruceLogType = BlockDefinition::GetBlockTypeIDByName("SpruceLog");
	const uint8_t spruceLeavesType = BlockDefinition::GetBlockTypeIDByName("SpruceLeaves");
	const uint8_t spruceLeavesSnowType = BlockDefinition::GetBlockTypeIDByName("SpruceLeavesSnow");
	const uint8_t jungleLogType = BlockDefinition::GetBlockTypeIDByName("JungleLog");
	const uint8_t jungleLeavesType = BlockDefinition::GetBlockTypeIDByName("JungleLeaves");
	const uint8_t acaciaLogType = BlockDefinition::GetBlockTypeIDByName("AcaciaLog");
	const uint8_t acaciaLeavesType = BlockDefinition::GetBlockTypeIDByName("AcaciaLeaves");
	const uint8_t cactusLogType = BlockDefinition::GetBlockTypeIDByName("CactusLog");

	s_treeStamps[TreeType::OAK] = CreateOakTree(oakLogType, oakLeavesType);
	s_treeStamps[TreeType::BIRCH] = CreateBirchTree(birchLogType, birchLeavesType);
	s_treeStamps[TreeType::SPRUCE] = CreateSpruceTree(spruceLogType, spruceLeavesType);
	s_treeStamps[TreeType::SPRUCE_SNOWY] = CreateSpruceTree(spruceLogType, spruceLeavesSnowType);
	s_treeStamps[TreeType::JUNGLE] = CreateJungleTree(jungleLogType, jungleLeavesType);
	s_treeStamps[TreeType::ACACIA] = CreateAcaciaTree(acaciaLogType, acaciaLeavesType);
	s_treeStamps[TreeType::CACTUS] = CreateCactus(cactusLogType);
}


TreeStamp TreeGenerator::CreateOakTree(uint8_t logType, uint8_t leavesType)
{
	TreeStamp stamp;

	// Trunk (5 blocks tall)
	for (int z = 0; z < 5; ++z)
	{
		stamp.blocks.push_back(TreeBlockOffset(0, 0, z, logType));
	}

	// Leaves (3 layers)
	// Layer 1 (z=3): 5x5 square
	for (int x = -2; x <= 2; ++x)
	{
		for (int y = -2; y <= 2; ++y)
		{
			if (x == 0 && y == 0) continue; // Skip center (trunk)
			stamp.blocks.push_back(TreeBlockOffset(x, y, 3, leavesType));
		}
	}

	// Layer 2 (z=4): 5x5 square
	for (int x = -2; x <= 2; ++x)
	{
		for (int y = -2; y <= 2; ++y)
		{
			if (x == 0 && y == 0) continue; // Skip center (trunk)
			stamp.blocks.push_back(TreeBlockOffset(x, y, 4, leavesType));
		}
	}

	// Layer 3 (z=5): 3x3 square
	for (int x = -1; x <= 1; ++x)
	{
		for (int y = -1; y <= 1; ++y)
		{
			stamp.blocks.push_back(TreeBlockOffset(x, y, 5, leavesType));
		}
	}

	stamp.maxRadius = 2;
	stamp.height = 6;
	return stamp;
}

TreeStamp TreeGenerator::CreateBirchTree(uint8_t logType, uint8_t leavesType)
{
	TreeStamp stamp;

	// Trunk (6 blocks tall)
	for (int z = 0; z < 6; ++z)
	{
		stamp.blocks.push_back(TreeBlockOffset(0, 0, z, logType));
	}

	// Leaves (similar to oak but slightly taller)
	// Layer 1 (z=4): 5x5
	for (int x = -2; x <= 2; ++x)
	{
		for (int y = -2; y <= 2; ++y)
		{
			if (x == 0 && y == 0) continue;
			stamp.blocks.push_back(TreeBlockOffset(x, y, 4, leavesType));
		}
	}

	// Layer 2 (z=5): 5x5
	for (int x = -2; x <= 2; ++x)
	{
		for (int y = -2; y <= 2; ++y)
		{
			if (x == 0 && y == 0) continue;
			stamp.blocks.push_back(TreeBlockOffset(x, y, 5, leavesType));
		}
	}

	// Layer 3 (z=6): 3x3
	for (int x = -1; x <= 1; ++x)
	{
		for (int y = -1; y <= 1; ++y)
		{
			stamp.blocks.push_back(TreeBlockOffset(x, y, 6, leavesType));
		}
	}

	stamp.maxRadius = 2;
	stamp.height = 7;
	return stamp;
}

TreeStamp TreeGenerator::CreateSpruceTree(uint8_t logType, uint8_t leavesType)
{
	TreeStamp stamp;

	// Trunk (7 blocks tall)
	for (int z = 0; z < 7; ++z)
	{
		stamp.blocks.push_back(TreeBlockOffset(0, 0, z, logType));
	}

	// Conical shape
	// Layer 1 (z=3): 5x5
	for (int x = -2; x <= 2; ++x)
	{
		for (int y = -2; y <= 2; ++y)
		{
			if (x == 0 && y == 0) continue;
			stamp.blocks.push_back(TreeBlockOffset(x, y, 3, leavesType));
		}
	}

	// Layer 2 (z=4): 5x5
	for (int x = -2; x <= 2; ++x)
	{
		for (int y = -2; y <= 2; ++y)
		{
			if (x == 0 && y == 0) continue;
			stamp.blocks.push_back(TreeBlockOffset(x, y, 4, leavesType));
		}
	}

	// Layer 3 (z=5): 3x3
	for (int x = -1; x <= 1; ++x)
	{
		for (int y = -1; y <= 1; ++y)
		{
			if (x == 0 && y == 0) continue;
			stamp.blocks.push_back(TreeBlockOffset(x, y, 5, leavesType));
		}
	}

	// Layer 4 (z=6): 3x3
	for (int x = -1; x <= 1; ++x)
	{
		for (int y = -1; y <= 1; ++y)
		{
			if (x == 0 && y == 0) continue;
			stamp.blocks.push_back(TreeBlockOffset(x, y, 6, leavesType));
		}
	}

	// Top (z=7): single block
	stamp.blocks.push_back(TreeBlockOffset(0, 0, 7, leavesType));

	stamp.maxRadius = 2;
	stamp.height = 8;
	return stamp;
}

TreeStamp TreeGenerator::CreateJungleTree(uint8_t logType, uint8_t leavesType)
{
	TreeStamp stamp;

	// Trunk (8 blocks tall - taller than others)
	for (int z = 0; z < 8; ++z)
	{
		stamp.blocks.push_back(TreeBlockOffset(0, 0, z, logType));
	}

	// Large canopy
	// Layer 1 (z=5): 5x5
	for (int x = -2; x <= 2; ++x)
	{
		for (int y = -2; y <= 2; ++y)
		{
			if (x == 0 && y == 0) continue;
			stamp.blocks.push_back(TreeBlockOffset(x, y, 5, leavesType));
		}
	}

	// Layer 2 (z=6): 5x5
	for (int x = -2; x <= 2; ++x)
	{
		for (int y = -2; y <= 2; ++y)
		{
			if (x == 0 && y == 0) continue;
			stamp.blocks.push_back(TreeBlockOffset(x, y, 6, leavesType));
		}
	}

	// Layer 3 (z=7): 5x5
	for (int x = -2; x <= 2; ++x)
	{
		for (int y = -2; y <= 2; ++y)
		{
			if (x == 0 && y == 0) continue;
			stamp.blocks.push_back(TreeBlockOffset(x, y, 7, leavesType));
		}
	}

	// Layer 4 (z=8): 3x3
	for (int x = -1; x <= 1; ++x)
	{
		for (int y = -1; y <= 1; ++y)
		{
			stamp.blocks.push_back(TreeBlockOffset(x, y, 8, leavesType));
		}
	}

	stamp.maxRadius = 2;
	stamp.height = 9;
	return stamp;
}

TreeStamp TreeGenerator::CreateAcaciaTree(uint8_t logType, uint8_t leavesType)
{
	TreeStamp stamp;

	// Trunk (5 blocks tall)
	for (int z = 0; z < 5; ++z)
	{
		stamp.blocks.push_back(TreeBlockOffset(0, 0, z, logType));
	}

	// Flat canopy
	// Layer 1 (z=5): 5x5
	for (int x = -2; x <= 2; ++x)
	{
		for (int y = -2; y <= 2; ++y)
		{
			stamp.blocks.push_back(TreeBlockOffset(x, y, 5, leavesType));
		}
	}

	// Layer 2 (z=6): 3x3
	for (int x = -1; x <= 1; ++x)
	{
		for (int y = -1; y <= 1; ++y)
		{
			stamp.blocks.push_back(TreeBlockOffset(x, y, 6, leavesType));
		}
	}

	stamp.maxRadius = 2;
	stamp.height = 7;
	return stamp;
}

TreeStamp TreeGenerator::CreateCactus(uint8_t cactusType)
{
	TreeStamp stamp;

	// Simple cactus (3 blocks tall)
	for (int z = 0; z < 3; ++z)
	{
		stamp.blocks.push_back(TreeBlockOffset(0, 0, z, cactusType));
	}

	stamp.maxRadius = 0;
	stamp.height = 3;
	return stamp;
}

const TreeStamp& TreeGenerator::GetTreeStamp(TreeType type)
{
	return s_treeStamps[type];
}
