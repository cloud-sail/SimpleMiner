#pragma once
#include "Engine/Math/Spline.hpp"
#include <map>


inline constexpr float DEFAULT_PERSISTANCE = 0.5f;
inline constexpr float DEFAULT_OCTAVE_SCALE = 2.0f;

struct PerlinNoiseParams
{
	float m_scale = 256.f;
	unsigned int m_numOctaves = 4;
	float m_octavePersistence = 0.5f;
	float m_octaveScale = 2.f;
	unsigned int m_seed = 0;

	PerlinNoiseParams() = default;
	PerlinNoiseParams(float scale, unsigned int numOctaves, float persistence, float octaveScale, unsigned int seed)
		: m_scale(scale), m_numOctaves(numOctaves), m_octavePersistence(persistence), m_octaveScale(octaveScale), m_seed(seed) {}

	float SamplePerlinNoise2D(int globalX, int globalY) const;
	float SamplePerlinNoise3D(int globalX, int globalY, int globalZ) const;
};

float CalculatePV(float weirdness);


struct PCGParams
{
	// 2D Perlin Noise Parameters
	PerlinNoiseParams m_continentNoise;
	PerlinNoiseParams m_erosionNoise;
	PerlinNoiseParams m_peaksValleysNoise; // weirdness: PV = 1 - abs(3 * abs(weirdness) - 2)
	PerlinNoiseParams m_temperatureNoise;
	PerlinNoiseParams m_humidityNoise;

	// 3D Perlin Noise Parameters
	PerlinNoiseParams m_densityNoise;

	// Splines
	Spline1D m_heightOffsetSpline;	// Continentalness -> height offset (h)
	Spline1D m_squashingSpline;     // Continentalness -> squashing factor (s)
	Spline1D m_erosionFactorSpline;	// Erosion -> erosion factor (e)

	// Bias Parameters
	float m_baseHeight = 64.f;
	float m_biasStrengthMin = 0.015f;
	float m_biasStrengthMax = 0.025f; 

	// Peaks/Valleys Parameters
	float m_peaksValleysStrength = 1.0f;


	unsigned int m_seaLevel = 63;

	unsigned int m_minDirtOffsetZ = 3;
	unsigned int m_maxDirtOffsetZ = 4;

	float m_coalChance = 0.05f;
	float m_ironChance = 0.02f;
	float m_goldChance = 0.005f;
	float m_diamondChance = 0.0001f;

	// The bottom is lava, and above lava is obsidian
	unsigned int minLavaDepth = 0;
	unsigned int maxLavaDepth = 1;

	unsigned int minObsidianDepth = 1;
	unsigned int maxObsidianDepth = 2;


	// Tree generation parameters
	unsigned int m_treeSeed = 5000;
	float m_treeNoiseScale = 4.f;

	// Biome-specific tree thresholds [0, 1]
	float m_desertCactusThreshold = 0.85f;      // Sparse (was 0.97)
	float m_savannaTreeThreshold = 0.80f;       // Sparse (was 0.95)
	float m_forestTreeThreshold = 0.70f;        // Medium (was 0.90)
	float m_jungleTreeThreshold = 0.65f;        // Dense (was 0.88)
	float m_taigaTreeThreshold = 0.75f;         // Medium (was 0.92)



	PCGParams()
	{
		InitializeDefaultParams();
	}

	void InitializeDefaultParams();

};

enum class BiomeType
{
	OCEAN,
	DEEP_OCEAN,
	FROZEN_OCEAN,
	BEACH,
	SNOWY_BEACH,
	DESERT,
	SAVANNA,
	PLAINS,
	SNOWY_PLAINS,
	FOREST,
	JUNGLE,
	TAIGA,
	SNOWY_TAIGA,
	STONY_PEAKS,
	SNOWY_PEAKS,
	COUNT
};

//-----------------------------------------------------------------------------------------------
enum class Continentalness
{
	DEEP_OCEAN,
	OCEAN,
	COAST,
	NEAR_INLAND,
	MID_INLAND,
	FAR_INLAND,
	COUNT
};

enum class Erosion {
	E0, E1, E2, E3, E4, E5, E6,
	COUNT
};

enum class PeaksValleys {
	VALLEYS,
	LOW,
	MID,
	HIGH,
	PEAKS,
	COUNT
};

enum class Temperature {
	T0, T1, T2, T3, T4,
	COUNT
};

enum class Humidity {
	H0, H1, H2, H3, H4,
	COUNT
};

template<typename T>
struct GameRange 
{
	float min;
	float max;
	T value;

	bool Contains(float val) const 
	{
		return val >= min && val < max;
	}
};

template<typename T>
class GameRangeMapper {
private:
	std::vector<GameRange<T>> ranges;

public:
	void AddRange(float min, float max, T value) {
		ranges.push_back({ min, max, value });
	}

	// Please ensure insert by order
	T Map(float value) const 
	{
		if (value < ranges.front().min) 
		{
			return ranges.front().value;
		}

		if (value >= ranges.back().max) 
		{
			return ranges.back().value;
		}


		for (const auto& range : ranges) 
		{
			if (range.Contains(value)) {
				return range.value;
			}
		}

		// default
		return ranges.front().value;
	}
};

class NoiseRangeMappers
{
public:
	static GameRangeMapper<Continentalness>		m_continentalnessMapper;
	static GameRangeMapper<Erosion>				m_erosionMapper;
	static GameRangeMapper<PeaksValleys>		m_peaksValleysMapper;
	static GameRangeMapper<Temperature>			m_temperatureMapper;
	static GameRangeMapper<Humidity>			m_humidityMapper;
};

class BiomeSelector
{
public:
	static BiomeType SelectBiome(Continentalness cont, Erosion ero, PeaksValleys pv, Temperature temp, Humidity hum);

private:
	static BiomeType SelectDeepOceanBiome(Temperature temp);
	static BiomeType SelectOceanBiome(Temperature temp);


	// Beach biomes lookup
	static BiomeType SelectBeachBiome(Temperature temp);


	// Badland biomes lookup
	static BiomeType SelectBadlandBiome(Humidity hum);

	// Middle biomes lookup
	static BiomeType SelectMiddleBiome(Temperature temp, Humidity hum);

	// Main inland biome selection logic
	static BiomeType SelectInlandBiome(Continentalness cont, Erosion ero, PeaksValleys pv, Temperature temp, Humidity hum);
		
};


struct TreeBlockOffset
{
	int offsetX;
	int offsetY;
	int offsetZ;
	uint8_t blockType;

	TreeBlockOffset(int x, int y, int z, uint8_t type)
		: offsetX(x), offsetY(y), offsetZ(z), blockType(type) {
	}
};

enum class TreeType
{
	OAK,			// FOREST
	BIRCH,			// FOREST
	SPRUCE,			// TAIGA
	SPRUCE_SNOWY,	// SNOWT_TAIGA
	JUNGLE,			// JUNGLE
	ACACIA,			// SAVANNA
	CACTUS,			// DESERT 
	COUNT
};


struct TreeStamp
{
	std::vector<TreeBlockOffset> blocks;
	int maxRadius; // Maximum horizontal distance from center
	int height;    // Maximum vertical height

	TreeStamp() : maxRadius(0), height(0) {}
};

class TreeGenerator
{
public:
	static void InitializeTreeStamps();
	static const TreeStamp& GetTreeStamp(TreeType type);

private:
	static std::map<TreeType, TreeStamp> s_treeStamps; // only one stamp for each type

	static TreeStamp CreateOakTree(uint8_t logType, uint8_t leavesType);
	static TreeStamp CreateBirchTree(uint8_t logType, uint8_t leavesType);
	static TreeStamp CreateSpruceTree(uint8_t logType, uint8_t leavesType);
	static TreeStamp CreateJungleTree(uint8_t logType, uint8_t leavesType);
	static TreeStamp CreateAcaciaTree(uint8_t logType, uint8_t leavesType);
	static TreeStamp CreateCactus(uint8_t cactusType);
};
