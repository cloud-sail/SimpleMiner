#include "Game/Chunk.hpp"
#include "Game/Game.hpp"
#include "Game/World.hpp"
#include "Game/ChunkUtils.hpp"
#include "Game/BlockDefinition.hpp"
#include "Game/BlockIterator.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Core/DebugRender.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/Renderer/IndexBuffer.hpp"

#include "ThirdParty/Noise/RawNoise.hpp"
#include "ThirdParty/Noise/SmoothNoise.hpp"

#include <filesystem>
#include <algorithm>

//-----------------------------------------------------------------------------------------------
constexpr unsigned int GAME_SEED = 1u;

constexpr float DEFAULT_OCTAVE_PERSISTANCE = 0.5f;
constexpr float DEFAULT_NOISE_OCTAVE_SCALE = 2.0f;

constexpr float DEFAULT_TERRAIN_HEIGHT = 64.0f;
constexpr float RIVER_DEPTH = 8.0f;
constexpr float TERRAIN_NOISE_SCALE = 200.0f;
constexpr unsigned int TERRAIN_NOISE_OCTAVES = 5u;

constexpr float HUMIDITY_NOISE_SCALE = 800.0f;
constexpr unsigned int HUMIDITY_NOISE_OCTAVES = 4u;

constexpr float TEMPERATURE_RAW_NOISE_SCALE = 0.0075f;
constexpr float TEMPERATURE_NOISE_SCALE = 400.0f;
constexpr unsigned int TEMPERATURE_NOISE_OCTAVES = 4u;

constexpr float HILLINESS_NOISE_SCALE = 250.0f;
constexpr unsigned int HILLINESS_NOISE_OCTAVES = 4u;

constexpr float OCEAN_START_THRESHOLD = 0.0f;
constexpr float OCEAN_END_THRESHOLD = 0.5f;
constexpr float OCEAN_DEPTH = 30.0f;

constexpr float OCEANESS_NOISE_SCALE = 600.0f;
constexpr unsigned int OCEANESS_NOISE_OCTAVES = 3u;

constexpr int MIN_DIRT_OFFSET_Z = 3;
constexpr int MAX_DIRT_OFFSET_Z = 4;
constexpr float MIN_SAND_HUMIDITY = 0.4f;
constexpr float MAX_SAND_HUMIDITY = 0.7f;
constexpr int SEA_LEVEL_Z = CHUNK_SIZE_Z / 2;

constexpr float ICE_TEMPERATURE_MAX = 0.37f;
constexpr float ICE_TEMPERATURE_MIN = 0.0f;
constexpr float ICE_DEPTH_MIN = 0.0f;
constexpr float ICE_DEPTH_MAX = 8.0f;

constexpr float MIN_SAND_DEPTH_HUMIDITY = 0.4f;
constexpr float MAX_SAND_DEPTH_HUMIDITY = 0.0f;
constexpr float SAND_DEPTH_MIN = 0.0f;
constexpr float SAND_DEPTH_MAX = 6.0f;

constexpr float COAL_CHANCE = 0.05f;
constexpr float IRON_CHANCE = 0.02f;
constexpr float GOLD_CHANCE = 0.005f;
constexpr float DIAMOND_CHANCE = 0.0001f;
constexpr int OBSIDIAN_Z = 1;
constexpr int LAVA_Z = 0;

//-----------------------------------------------------------------------------------------------

constexpr uint8_t CHUNK_FILE_VERSION = 1;

struct ChunkFileHeader
{
	char    m_fourCC[4];	// 'G','C','H','K'
	uint8_t m_version;		// 1
	uint8_t m_bitsX;		// CHUNK_BITS_X (4)
	uint8_t m_bitsY;		// CHUNK_BITS_Y (4)
	uint8_t m_bitsZ;		// CHUNK_BITS_Z (7)
};
static_assert(sizeof(ChunkFileHeader) == 8, "ChunkFileHeader must be 8 bytes");

struct ChunkRun
{
	uint8_t m_type;      // block type [0, 255]
	uint8_t m_length;    // run length [1, 255]
};
static_assert(sizeof(ChunkRun) == 2, "ChunkRun must be 2 bytes");

















//-----------------------------------------------------------------------------------------------
Chunk::~Chunk()
{
	delete m_vertexBuffer;
	m_vertexBuffer = nullptr;

	delete m_indexBuffer;
	m_indexBuffer = nullptr;
}

Chunk::Chunk(World* world, IntVec2 chunkCoords)
	: m_world(world)
	, m_chunkCoords(chunkCoords)
{
	m_worldBounds = GetChunkAABBWorld(m_chunkCoords);
	m_blocks.resize(BLOCKS_PER_CHUNK);

	m_isMeshDirty = true;
	m_needsSaving = false;
	m_needsUploadToGpu = false;
}

void Chunk::Update(float deltaSeconds)
{
	if (m_hasMeshGenerated)
	{
		m_startFadeInElapsedSeconds += deltaSeconds;
	}
	//UploadToGpuIfNeeded();
}

void Chunk::Render() const
{
	RenderBlocks();

	//if (g_isDebugDraw)
	//{
	//	//DebugRenderChunkOutline();
	//	DebugRenderChunkGrid();
	//}
}

void Chunk::RenderDepth() const
{
	if (m_vertexBuffer != nullptr && m_indexBuffer != nullptr)
	{
		DepthOnlyResources resources;
		resources.cameraConstantsIndex = g_theRenderer->GetCurrentCameraConstantsIndex();
		resources.modelConstantsIndex = g_theRenderer->GetCurrentModelConstantsIndex();

		g_theRenderer->SetGraphicsBindlessResources(sizeof(DepthOnlyResources), &resources);

		g_theRenderer->BindShader(m_world->GetDepthOnlyShader());
		g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
		g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
		g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);

		g_theRenderer->DrawIndexedVertexBuffer(m_vertexBuffer, m_indexBuffer, m_indexBuffer->GetCount());
	}
}

void Chunk::RegenerateMeshIfDirty()
{
	if (!m_isMeshDirty)
	{
		return;
	}
	InitializeMeshes();
	m_isMeshDirty = false;
	m_needsUploadToGpu = true;
}

bool Chunk::LoadFromDiskIfExists()
{
	// Notes: return false will let the program override the chunk file later!
	std::string const filePath = GetChunkFilePath();

	if (!FileExists(filePath))
	{
		return false;
	}

	std::vector<uint8_t> buffer;
	FileReadToBuffer(buffer, filePath);
	if (buffer.size() < sizeof(ChunkFileHeader))
	{
		return false;
	}

	// Check Header
	ChunkFileHeader const* header = reinterpret_cast<ChunkFileHeader const*>(buffer.data());
	if (header->m_fourCC[0] != 'G' || header->m_fourCC[1] != 'C' || header->m_fourCC[2] != 'H' || header->m_fourCC[3] != 'K') return false;
	if (header->m_version != CHUNK_FILE_VERSION) return false;
	if (header->m_bitsX != CHUNK_BITS_X || header->m_bitsY != CHUNK_BITS_Y || header->m_bitsZ != CHUNK_BITS_Z) return false;

	// Check 2-byte run
	const size_t runsBytes = buffer.size() - sizeof(ChunkFileHeader);
	if ((runsBytes % sizeof(ChunkRun)) != 0)
	{
		return false;
	}
	const size_t runsCount = runsBytes / sizeof(ChunkRun);


	size_t outIndex = 0;
	const uint8_t* runsPtr = buffer.data() + sizeof(ChunkFileHeader);

	for (size_t i = 0; i < runsCount; ++i)
	{
		ChunkRun const* run = reinterpret_cast<ChunkRun const*>(runsPtr + i * sizeof(ChunkRun));
		const uint8_t type = run->m_type;
		const uint8_t len = run->m_length;

		// check run length
		if (len == 0)
		{
			return false;
		}

		// check if data exceeded
		if (outIndex + len > static_cast<size_t>(BLOCKS_PER_CHUNK))
		{
			return false;
		}
		for (uint8_t k = 0; k < len; ++k)
		{
			m_blocks[outIndex].SetTypeID(type);
			++outIndex;
		}
	}

	// Wrong Block Count, but already write in m_blocks
	if (outIndex != static_cast<size_t>(BLOCKS_PER_CHUNK))
	{
		return false;
	}

	return true;
}

void Chunk::SaveToDiskIfNeeded()
{
	if (!m_needsSaving)
	{
		return;
	}

	EnsureDirectoryExists(GetSavesDirectory());

	std::string const filename = GetChunkFilePath();

	std::vector<uint8_t> out;
	out.reserve(BLOCKS_PER_CHUNK / 4); // Predicted number

	ChunkFileHeader header{};
	header.m_fourCC[0] = 'G';
	header.m_fourCC[1] = 'C';
	header.m_fourCC[2] = 'H';
	header.m_fourCC[3] = 'K';
	header.m_version = 1;
	header.m_bitsX = static_cast<uint8_t>(CHUNK_BITS_X);
	header.m_bitsY = static_cast<uint8_t>(CHUNK_BITS_Y);
	header.m_bitsZ = static_cast<uint8_t>(CHUNK_BITS_Z);

	const uint8_t* headerBytes = reinterpret_cast<const uint8_t*>(&header);
	out.insert(out.end(), headerBytes, headerBytes + sizeof(ChunkFileHeader));

	// RLE Encoding
	uint8_t currentType = m_blocks[0].GetTypeID();
	uint8_t runLen = 0;

	for (int i = 0; i < BLOCKS_PER_CHUNK; ++i)
	{
		const uint8_t t = m_blocks[i].GetTypeID();

		if (t != currentType || runLen == 255)
		{
			out.push_back(currentType);
			out.push_back(runLen);

			// New run
			currentType = t;
			runLen = 0;
		}

		++runLen;
	}

	// Last Run
	if (runLen > 0)
	{
		out.push_back(currentType);
		out.push_back(runLen);
	}

	const int written = FileWriteFromBuffer(out, filename);
	if (written >= static_cast<int>(sizeof(ChunkFileHeader)))
	{
		// Written Success
		m_needsSaving = false; 
	}
}

void Chunk::InitializeRandomBlocks()
{
	// Take ID from block definitions
	const uint8_t airType = BlockDefinition::GetBlockTypeIDByName("Air");
	const uint8_t waterType = BlockDefinition::GetBlockTypeIDByName("Water");
	const uint8_t iceType = BlockDefinition::GetBlockTypeIDByName("Ice");
	const uint8_t grassType = BlockDefinition::GetBlockTypeIDByName("Grass");
	const uint8_t sandType = BlockDefinition::GetBlockTypeIDByName("Sand");
	const uint8_t dirtType = BlockDefinition::GetBlockTypeIDByName("Dirt");
	const uint8_t stoneType = BlockDefinition::GetBlockTypeIDByName("Stone");
	const uint8_t coalType = BlockDefinition::GetBlockTypeIDByName("Coal");
	const uint8_t ironType = BlockDefinition::GetBlockTypeIDByName("Iron");
	const uint8_t goldType = BlockDefinition::GetBlockTypeIDByName("Gold");
	const uint8_t diamondType = BlockDefinition::GetBlockTypeIDByName("Diamond");
	const uint8_t obsidianType = BlockDefinition::GetBlockTypeIDByName("Obsidian");
	const uint8_t lavaType = BlockDefinition::GetBlockTypeIDByName("Lava");

	// Derive deterministic seeds for each noise channel
	const unsigned int terrainSeed		= GAME_SEED + 0u;
	const unsigned int humiditySeed		= GAME_SEED + 1u;
	const unsigned int temperatureSeed	= GAME_SEED + 2u;
	const unsigned int hillSeed			= GAME_SEED + 3u;
	const unsigned int oceanSeed		= GAME_SEED + 4u;
	const unsigned int dirtSeed			= GAME_SEED + 5u;
	const unsigned int oreSeed			= GAME_SEED + 6u;

	const int chunkMinX = m_chunkCoords.x * CHUNK_SIZE_X;
	const int chunkMinY = m_chunkCoords.y * CHUNK_SIZE_Y;


	int heightMapXY[CHUNK_SIZE_X * CHUNK_SIZE_Y] = {};
	int dirtDepthXY[CHUNK_SIZE_X * CHUNK_SIZE_Y] = {};
	float humidityMapXY[CHUNK_SIZE_X * CHUNK_SIZE_Y] = {};
	float temperatureXY[CHUNK_SIZE_X * CHUNK_SIZE_Y] = {};


	//const int XY_COUNT = CHUNK_SIZE_X * CHUNK_SIZE_Y;
	//std::vector<int>   heightMapXY;    heightMapXY.resize(XY_COUNT, 0);
	//std::vector<int>   dirtDepthXY;    dirtDepthXY.resize(XY_COUNT, MIN_DIRT_OFFSET_Z);
	//std::vector<float> humidityMapXY;  humidityMapXY.resize(XY_COUNT, 0.5f);
	//std::vector<float> temperatureXY;  temperatureXY.resize(XY_COUNT, 0.5f);

	// Humidity: Perlin noise remapped to [0, 1]. Used to choose grass vs sand and how deep sand goes.
	// Temperature map: Perlin noise remapped to [0, 1] plus a base Perlin noise in the range [-1, 1]. Determine whether water freezes and how deep the ice cap extends.
	// New Temperature Map: Small Raw Noise + Perlin [0, 1]



	// --- Pass 1: compute surface & biome fields per (x,y) pillar ---
	for (int localY = 0; localY < CHUNK_SIZE_Y; ++localY)
	{
		const int globalY = chunkMinY + localY;
		for (int localX = 0; localX < CHUNK_SIZE_X; ++localX)
		{
			const int globalX = chunkMinX + localX;

			const float humidity =
				0.5f + 0.5f *
				Compute2dPerlinNoise((float)globalX, (float)globalY, HUMIDITY_NOISE_SCALE, HUMIDITY_NOISE_OCTAVES, DEFAULT_OCTAVE_PERSISTANCE, DEFAULT_NOISE_OCTAVE_SCALE, true, humiditySeed);

			float temperature =
				Get2dNoiseNegOneToOne(globalX, globalY, temperatureSeed) * TEMPERATURE_RAW_NOISE_SCALE;
			temperature +=
				0.5f + 0.5f *
				Compute2dPerlinNoise((float)globalX, (float)globalY, TEMPERATURE_NOISE_SCALE, TEMPERATURE_NOISE_OCTAVES, DEFAULT_OCTAVE_PERSISTANCE, DEFAULT_NOISE_OCTAVE_SCALE, true, temperatureSeed);

			const float rawHill = Compute2dPerlinNoise((float)globalX, (float)globalY, HILLINESS_NOISE_SCALE, HILLINESS_NOISE_OCTAVES, DEFAULT_OCTAVE_PERSISTANCE, DEFAULT_NOISE_OCTAVE_SCALE, true, hillSeed);
			const float hill = SmoothStep3(RangeMap(rawHill, -1.0f, 1.0f, 0.0f, 1.0f));

			const float ocean = Compute2dPerlinNoise((float)globalX, (float)globalY, OCEANESS_NOISE_SCALE, OCEANESS_NOISE_OCTAVES, DEFAULT_OCTAVE_PERSISTANCE, DEFAULT_NOISE_OCTAVE_SCALE, true, oceanSeed); 

			const float rawTerrain = Compute2dPerlinNoise((float)globalX, (float)globalY, TERRAIN_NOISE_SCALE, TERRAIN_NOISE_OCTAVES, DEFAULT_OCTAVE_PERSISTANCE, DEFAULT_NOISE_OCTAVE_SCALE, true, terrainSeed);

			// Base terrain height with river/hill shaping
			float terrainHeightF =
				DEFAULT_TERRAIN_HEIGHT
				+ hill * RangeMap(fabsf(rawTerrain), 0.0f, 1.0f, -RIVER_DEPTH, DEFAULT_TERRAIN_HEIGHT);

			// Ocean depressions
			if (ocean > OCEAN_START_THRESHOLD)
			{
				const float oceanBlend = RangeMapClamped(ocean, OCEAN_START_THRESHOLD, OCEAN_END_THRESHOLD, 0.0f, 1.0f);
				terrainHeightF -= Interpolate(0.0f, OCEAN_DEPTH, oceanBlend);
			}

			// Dirt layer thickness driven by noise
			const float dirtDepthPct = Get2dNoiseZeroToOne(globalX, globalY, dirtSeed);
			const int dirtDepth = MIN_DIRT_OFFSET_Z + (int)std::round(dirtDepthPct * float(MAX_DIRT_OFFSET_Z - MIN_DIRT_OFFSET_Z));

			const int idxXY = localY * CHUNK_SIZE_X + localX;
			humidityMapXY[idxXY] = humidity;
			temperatureXY[idxXY] = temperature;
			heightMapXY[idxXY] = RoundDownToInt(terrainHeightF); // need to check smaller than 0 ?
			dirtDepthXY[idxXY] = dirtDepth;
		}
	}

	// --- Pass 2: assign block types for every (x,y,z) ---
	for (int localZ = 0; localZ < CHUNK_SIZE_Z; ++localZ)
	{
		for (int localY = 0; localY < CHUNK_SIZE_Y; ++localY)
		{
			for (int localX = 0; localX < CHUNK_SIZE_X; ++localX)
			{
				const int idx = GetBlockIndexInChunk(localX, localY, localZ);
				const int idxXY = localY * CHUNK_SIZE_X + localX;
				const int globalX = chunkMinX + localX;
				const int globalY = chunkMinY + localY;
				const int globalZ = localZ;

				const int   terrainHeight = heightMapXY[idxXY];
				const int   dirtDepth = dirtDepthXY[idxXY];
				const float humidity = humidityMapXY[idxXY];
				const float temperature = temperatureXY[idxXY];

				uint8_t outType = airType;

				// Temperature-driven ice ceiling depth
				const float iceThickness = RangeMapClamped(
					temperature,
					ICE_TEMPERATURE_MAX, ICE_TEMPERATURE_MIN, // Hot -> Cold
					ICE_DEPTH_MIN, ICE_DEPTH_MAX              // Thin -> Thick
				);
				const int iceDepthZ = (int)DEFAULT_TERRAIN_HEIGHT - RoundDownToInt(iceThickness);

				// Water and ice above terrain surface and under sea level
				if (globalZ > terrainHeight && globalZ < SEA_LEVEL_Z)
				{
					outType = waterType;
					if (temperature <= ICE_TEMPERATURE_MAX && globalZ > iceDepthZ)
					{
						outType = iceType;
					}
				}
				else if (globalZ == terrainHeight)
				{
					uint8_t surface = grassType;
					if (humidity < MIN_SAND_HUMIDITY)
					{
						surface = sandType;
					}
					if (humidity < MAX_SAND_HUMIDITY && terrainHeight <= (int)DEFAULT_TERRAIN_HEIGHT)
					{
						surface = sandType;
					}
					outType = surface;
				}
				else
				{
					// Subsurface: dirt or sand cap above stone/ores
					const int dirtTopZ = terrainHeight - dirtDepth;
					const int sandTopZ = terrainHeight - (int)(RangeMapClamped(
						humidity,
						MIN_SAND_DEPTH_HUMIDITY, MAX_SAND_DEPTH_HUMIDITY,	// Wet -> Drt
						SAND_DEPTH_MIN, SAND_DEPTH_MAX						// Thin -> Thick
					));

					if (globalZ < terrainHeight && globalZ >= dirtTopZ)
					{
						outType = dirtType;
						if (globalZ >= sandTopZ)
						{
							outType = sandType;
						}
					}
					// Deep underground: special layers, lava/obsidian, ores, stone
					else if (globalZ < dirtTopZ)
					{
						if (globalZ == OBSIDIAN_Z)
						{
							outType = obsidianType;
						}
						else if (globalZ == LAVA_Z)
						{
							outType = lavaType;
						}
						else
						{
							const float oreNoise = Get3dNoiseZeroToOne(globalX, globalY, globalZ, oreSeed);
							if (oreNoise < DIAMOND_CHANCE)
								outType = diamondType;
							else if (oreNoise < GOLD_CHANCE)
								outType = goldType;
							else if (oreNoise < IRON_CHANCE)
								outType = ironType;
							else if (oreNoise < COAL_CHANCE)
								outType = coalType;
							else
								outType = stoneType;
						}
					}

				}

				m_blocks[idx].SetTypeID(outType);
			}
		}
	}

}

void Chunk::InitializePCGBlocks()
{
	// Take ID from block definitions
	const uint8_t airType = BlockDefinition::GetBlockTypeIDByName("Air");
	const uint8_t waterType = BlockDefinition::GetBlockTypeIDByName("Water");
	const uint8_t sandType = BlockDefinition::GetBlockTypeIDByName("Sand");
	const uint8_t snowType = BlockDefinition::GetBlockTypeIDByName("Snow");
	const uint8_t iceType = BlockDefinition::GetBlockTypeIDByName("Ice");
	const uint8_t dirtType = BlockDefinition::GetBlockTypeIDByName("Dirt");
	const uint8_t stoneType = BlockDefinition::GetBlockTypeIDByName("Stone");
	const uint8_t coalType = BlockDefinition::GetBlockTypeIDByName("Coal");
	const uint8_t ironType = BlockDefinition::GetBlockTypeIDByName("Iron");
	const uint8_t goldType = BlockDefinition::GetBlockTypeIDByName("Gold");
	const uint8_t diamondType = BlockDefinition::GetBlockTypeIDByName("Diamond");
	const uint8_t obsidianType = BlockDefinition::GetBlockTypeIDByName("Obsidian");
	const uint8_t lavaType = BlockDefinition::GetBlockTypeIDByName("Lava");
	//const uint8_t glowstoneType = BlockDefinition::GetBlockTypeIDByName("Glowstone");
	//const uint8_t cobblestoneType = BlockDefinition::GetBlockTypeIDByName("Cobblestone");
	//const uint8_t chiseledBrickType = BlockDefinition::GetBlockTypeIDByName("ChiseledBrick");
	const uint8_t grassType = BlockDefinition::GetBlockTypeIDByName("Grass");
	const uint8_t grassLightType = BlockDefinition::GetBlockTypeIDByName("GrassLight");
	const uint8_t grassDarkType = BlockDefinition::GetBlockTypeIDByName("GrassDark");
	const uint8_t grassYellowType = BlockDefinition::GetBlockTypeIDByName("GrassYellow");
	//const uint8_t acaciaLogType = BlockDefinition::GetBlockTypeIDByName("AcaciaLog");
	//const uint8_t acaciaPlanksType = BlockDefinition::GetBlockTypeIDByName("AcaciaPlanks");
	//const uint8_t acaciaLeavesType = BlockDefinition::GetBlockTypeIDByName("AcaciaLeaves");
	//const uint8_t cactusLogType = BlockDefinition::GetBlockTypeIDByName("CactusLog");
	//const uint8_t oakLogType = BlockDefinition::GetBlockTypeIDByName("OakLog");
	//const uint8_t oakPlanksType = BlockDefinition::GetBlockTypeIDByName("OakPlanks");
	//const uint8_t oakLeavesType = BlockDefinition::GetBlockTypeIDByName("OakLeaves");
	//const uint8_t birchLogType = BlockDefinition::GetBlockTypeIDByName("BirchLog");
	//const uint8_t birchPlanksType = BlockDefinition::GetBlockTypeIDByName("BirchPlanks");
	//const uint8_t birchLeavesType = BlockDefinition::GetBlockTypeIDByName("BirchLeaves");
	//const uint8_t jungleLogType = BlockDefinition::GetBlockTypeIDByName("JungleLog");
	//const uint8_t junglePlanksType = BlockDefinition::GetBlockTypeIDByName("JunglePlanks");
	//const uint8_t jungleLeavesType = BlockDefinition::GetBlockTypeIDByName("JungleLeaves");
	//const uint8_t spruceLogType = BlockDefinition::GetBlockTypeIDByName("SpruceLog");
	//const uint8_t sprucePlanksType = BlockDefinition::GetBlockTypeIDByName("SprucePlanks");
	//const uint8_t spruceLeavesType = BlockDefinition::GetBlockTypeIDByName("SpruceLeaves");
	//const uint8_t spruceLeavesSnowType = BlockDefinition::GetBlockTypeIDByName("SpruceLeavesSnow");

	PCGParams params = g_theGame->GetPCGParams();

	const int chunkMinX = m_chunkCoords.x * CHUNK_SIZE_X;
	const int chunkMinY = m_chunkCoords.y * CHUNK_SIZE_Y;

	// --- Pass 1: compute surface & biome fields per (x,y) pillar ---
	for (int localY = 0; localY < CHUNK_SIZE_Y; ++localY)
	{
		const int globalY = chunkMinY + localY;
		for (int localX = 0; localX < CHUNK_SIZE_X; ++localX)
		{
			const int globalX = chunkMinX + localX;

			const int idxXY = localY * CHUNK_SIZE_X + localX;

			m_continent[idxXY]		= params.m_continentNoise.SamplePerlinNoise2D(globalX, globalY);
			m_erosion[idxXY]		= params.m_erosionNoise.SamplePerlinNoise2D(globalX, globalY);
			m_peaksValleys[idxXY]	= CalculatePV(params.m_peaksValleysNoise.SamplePerlinNoise2D(globalX, globalY));
			m_temperature[idxXY]	= params.m_temperatureNoise.SamplePerlinNoise2D(globalX, globalY);
			m_humidity[idxXY]		= params.m_humidityNoise.SamplePerlinNoise2D(globalX, globalY);
		}
	}

	// --- Pass 2: Terrain Density for every (x,y,z) ---
	for (int localZ = 0; localZ < CHUNK_SIZE_Z; ++localZ)
	{
		for (int localY = 0; localY < CHUNK_SIZE_Y; ++localY)
		{
			for (int localX = 0; localX < CHUNK_SIZE_X; ++localX)
			{
				const int idx = GetBlockIndexInChunk(localX, localY, localZ);
				const int idxXY = localY * CHUNK_SIZE_X + localX;
				const int globalX = chunkMinX + localX;
				const int globalY = chunkMinY + localY;
				const int globalZ = localZ;

				const float continent = m_continent[idxXY];
				const float erosion = m_erosion[idxXY];
				const float peaksValleys = m_peaksValleys[idxXY];
				//const float temperature = m_temperature[idxXY];
				//const float humidity = m_humidity[idxXY];

				uint8_t outType = airType;


				float h = params.m_heightOffsetSpline.GetValueAtInputKey(continent);	// [-1, 1]
				float s = params.m_squashingSpline.GetValueAtInputKey(continent);		// [0, 1]
				float e = params.m_erosionFactorSpline.GetValueAtInputKey(erosion);		// [0, 1]

				float density = params.m_densityNoise.SamplePerlinNoise3D(globalX, globalY, globalZ);

				float base = params.m_baseHeight;
				float t = (static_cast<float>(globalZ) - base) / base; // relative height [-1,)

				// Dynamic Bias
				float continentalityFactor = (continent + 1.0f) * 0.5f;  // [0, 1]
				float biasStrength = params.m_biasStrengthMin +
					(params.m_biasStrengthMax - params.m_biasStrengthMin) * continentalityFactor;
				float bias = (static_cast<float>(globalZ) - base) * (-biasStrength);
				density += bias;

				// Height Offset
				density += h;

				density -= s * t;

				density -= e * t;

				float peaksValleysEffect = peaksValleys * continentalityFactor * t * params.m_peaksValleysStrength;
				density -= peaksValleysEffect;

				if (density > 0.f)
				{
					outType = stoneType;
				}

				m_blocks[idx].SetTypeID(outType, false);
			}
		}
	}

	// Pass 3: Biomes - Surface and Block Replacement
	for (int localY = 0; localY < CHUNK_SIZE_Y; ++localY)
	{
		const int globalY = chunkMinY + localY;
		for (int localX = 0; localX < CHUNK_SIZE_X; ++localX)
		{
			const int globalX = chunkMinX + localX;
			const int idxXY = localY * CHUNK_SIZE_X + localX;

			// Get noise values for this column
			const float continent = m_continent[idxXY];
			const float erosion = m_erosion[idxXY];
			const float peaksValleys = m_peaksValleys[idxXY];
			const float temperature = m_temperature[idxXY];
			const float humidity = m_humidity[idxXY];

			// Map to discrete categories
			Continentalness cont = NoiseRangeMappers::m_continentalnessMapper.Map(continent);
			Erosion ero = NoiseRangeMappers::m_erosionMapper.Map(erosion);
			PeaksValleys pv = NoiseRangeMappers::m_peaksValleysMapper.Map(peaksValleys);
			Temperature temp = NoiseRangeMappers::m_temperatureMapper.Map(temperature);
			Humidity hum = NoiseRangeMappers::m_humidityMapper.Map(humidity);

			// Select biome for this column
			BiomeType biome = BiomeSelector::SelectBiome(cont, ero, pv, temp, hum);

			// Find surface height (first non-air block from top)
			int surfaceZ = -1;
			for (int localZ = CHUNK_SIZE_Z - 1; localZ >= 0; --localZ)
			{
				const int idx = GetBlockIndexInChunk(localX, localY, localZ);
				if (m_blocks[idx].GetTypeID() != airType)
				{
					surfaceZ = localZ;
					break;
				}
			}

			// Determine dirt depth for this column using noise
			const float dirtDepthNoise = Get2dNoiseZeroToOne(globalX, globalY, 2000);
			const int dirtDepth = params.m_minDirtOffsetZ +
				static_cast<int>(dirtDepthNoise * (params.m_maxDirtOffsetZ - params.m_minDirtOffsetZ + 1));

			// Check if this biome has dirt layers
			const bool hasDirtLayers = !(
				biome == BiomeType::DEEP_OCEAN ||
				biome == BiomeType::FROZEN_OCEAN ||
				biome == BiomeType::STONY_PEAKS ||
				biome == BiomeType::SNOWY_PEAKS
				);

			// Determine sand layer depth for desert
			const float sandDepthNoise = Get2dNoiseZeroToOne(globalX, globalY, 3000);
			const int desertSandDepth = 2 + static_cast<int>(sandDepthNoise * 3); // 2-4 layers

			// Process each block in this column from bottom to top
			for (int localZ = 0; localZ < CHUNK_SIZE_Z; ++localZ)
			{
				const int globalZ = localZ;
				const int idx = GetBlockIndexInChunk(localX, localY, localZ);

				// Step 1: Handle lava at the bottom
				if (localZ >= (int)params.minLavaDepth && localZ <= (int)params.maxLavaDepth)
				{
					m_blocks[idx].SetTypeID(lavaType, false);
					continue;
				}

				// Step 2: Handle obsidian above lava
				if (localZ >= (int)params.minObsidianDepth && localZ <= (int)params.maxObsidianDepth)
				{
					m_blocks[idx].SetTypeID(obsidianType, false);
					continue;
				}

				// Calculate depth from surface
				int depthFromSurface = -1;
				if (surfaceZ != -1)
				{
					depthFromSurface = surfaceZ - localZ;
				}

				// Step 3: Replace stone blocks with ores
				if (m_blocks[idx].GetTypeID() == stoneType && surfaceZ != -1)
				{
					// Only place ores below dirt layers
					if (depthFromSurface > dirtDepth)
					{
						const float oreNoise = Get3dNoiseZeroToOne(globalX, globalY, globalZ, 1000);

						if (oreNoise < params.m_diamondChance)
							m_blocks[idx].SetTypeID(diamondType, false);
						else if (oreNoise < params.m_goldChance)
							m_blocks[idx].SetTypeID(goldType, false);
						else if (oreNoise < params.m_ironChance)
							m_blocks[idx].SetTypeID(ironType, false);
						else if (oreNoise < params.m_coalChance)
							m_blocks[idx].SetTypeID(coalType, false);
						// else keep as stone
					}
				}

				// Step 4: Replace surface and subsurface blocks
				if (surfaceZ != -1 && depthFromSurface >= 0)
				{
					const bool isAboveSeaLevel = localZ > static_cast<int>(params.m_seaLevel);
					const bool isAtSeaLevel = localZ == static_cast<int>(params.m_seaLevel);
					UNUSED(isAtSeaLevel);
					// Surface block (depth == 0)
					if (depthFromSurface == 0)
					{
						switch (biome)
						{
						case BiomeType::OCEAN:
							m_blocks[idx].SetTypeID(isAboveSeaLevel ? sandType : dirtType, false);
							break;

						case BiomeType::DEEP_OCEAN:
							if (isAboveSeaLevel)
							{
								m_blocks[idx].SetTypeID((temp == Temperature::T0) ? snowType : sandType, false);
							}
							// Below sea level: keep as stone/ore
							break;

						case BiomeType::FROZEN_OCEAN:
							if (isAboveSeaLevel)
							{
								m_blocks[idx].SetTypeID(snowType, false);
							}
							// Below sea level: keep as stone/ore
							break;

						case BiomeType::BEACH:
							m_blocks[idx].SetTypeID(sandType, false);
							break;

						case BiomeType::SNOWY_BEACH:
							m_blocks[idx].SetTypeID(snowType, false);
							break;

						case BiomeType::DESERT:
							m_blocks[idx].SetTypeID(sandType, false);
							break;

						case BiomeType::SAVANNA:
							m_blocks[idx].SetTypeID(grassYellowType, false);
							break;

						case BiomeType::PLAINS:
							m_blocks[idx].SetTypeID(grassLightType, false);
							break;

						case BiomeType::SNOWY_PLAINS:
							m_blocks[idx].SetTypeID(snowType, false);
							break;

						case BiomeType::FOREST:
							m_blocks[idx].SetTypeID(grassType, false);
							break;

						case BiomeType::JUNGLE:
							m_blocks[idx].SetTypeID(grassDarkType, false);
							break;

						case BiomeType::TAIGA:
							m_blocks[idx].SetTypeID(grassLightType, false);
							break;

						case BiomeType::SNOWY_TAIGA:
							m_blocks[idx].SetTypeID(snowType, false);
							break;

						case BiomeType::STONY_PEAKS:
							// Keep as stone
							break;

						case BiomeType::SNOWY_PEAKS:
							m_blocks[idx].SetTypeID(snowType, false);
							break;
						}
					}
					// Subsurface blocks (1 to dirtDepth below surface)
					else if (depthFromSurface > 0 && depthFromSurface <= dirtDepth && hasDirtLayers)
					{
						// Desert: several layers of sand before dirt
						if (biome == BiomeType::DESERT)
						{
							if (depthFromSurface <= desertSandDepth)
							{
								m_blocks[idx].SetTypeID(sandType, false);
							}
							else
							{
								m_blocks[idx].SetTypeID(dirtType, false);
							}
						}
						else
						{
							m_blocks[idx].SetTypeID(dirtType, false);
						}
					}
				}
			}

			// Step 5: Fill air blocks below sea level with water (last step)
			for (int localZ = 0; localZ <= static_cast<int>(params.m_seaLevel); ++localZ)
			{
				const int idx = GetBlockIndexInChunk(localX, localY, localZ);
				if (m_blocks[idx].GetTypeID() == airType)
				{
					m_blocks[idx].SetTypeID(waterType, false);
				}
			}

			// Step 6: Replace water with ice at sea level for cold biomes
			const int seaLevelIdx = GetBlockIndexInChunk(localX, localY, params.m_seaLevel);
			if (m_blocks[seaLevelIdx].GetTypeID() == waterType)
			{
				if (biome == BiomeType::FROZEN_OCEAN ||
					biome == BiomeType::SNOWY_PLAINS ||
					biome == BiomeType::SNOWY_TAIGA ||
					biome == BiomeType::SNOWY_PEAKS)
				{
					m_blocks[seaLevelIdx].SetTypeID(iceType, false);
				}
				else if (biome == BiomeType::DEEP_OCEAN && temp == Temperature::T0)
				{
					m_blocks[seaLevelIdx].SetTypeID(iceType, false);
				}
			}
		}
	}

	// Pass 4: Generate Trees
	GenerateTrees(params);

	// Finally Update BitFlags in Block
	for (Block& b : m_blocks)
	{
		b.SetTypeID(b.GetTypeID());
	}
}

void Chunk::InitializeMeshes()
{
	if (m_vertexBuffer == nullptr)
	{
		m_vertexBuffer = g_theRenderer->CreateVertexBuffer(1 * sizeof(Vertex_PCU), sizeof(Vertex_PCU));
	}

	if (m_indexBuffer == nullptr)
	{
		m_indexBuffer = g_theRenderer->CreateIndexBuffer(1 * sizeof(unsigned int));
	}

	m_vertices.clear();
	m_indices.clear();

	constexpr size_t ESTIMATED_MULTIPLIER = 6;
	m_vertices.reserve(ESTIMATED_MULTIPLIER * 4 * CHUNK_SIZE_X * CHUNK_SIZE_Y);
	m_indices.reserve(ESTIMATED_MULTIPLIER * 6 * CHUNK_SIZE_X * CHUNK_SIZE_Y);

	for (int index = 0; index < BLOCKS_PER_CHUNK; ++index)
	{
		AddVertsForBlock(index);
	}
}

bool Chunk::UploadToGpuIfNeeded()
{
	if (!m_needsUploadToGpu)
	{
		return false;
	}

	m_totalNumVertices = (int)m_vertices.size();
	m_totalNumIndices = (int)m_indices.size();

	g_theRenderer->CopyCPUToGPU(m_vertices.data(), static_cast<unsigned int>(m_vertices.size()) * m_vertexBuffer->GetStride(), m_vertexBuffer);
	g_theRenderer->CopyCPUToGPU(m_indices.data(), static_cast<unsigned int>(m_indices.size()) * m_indexBuffer->GetStride(), m_indexBuffer);

	m_needsUploadToGpu = false;
	return true;
}

void Chunk::AddVertsForBlock(int blockIndex)
{
	// Translucent Water
	// Water IsFullyOpaque True -> False


	uint8_t blockType = m_blocks[blockIndex].GetTypeID();

	BlockDefinition const* blockDef = BlockDefinition::GetByType(blockType);
	if (!blockDef->m_isVisible) // Air is not visible
	{
		return;
	}

	AABB2 topUVs = blockDef->m_topUVs;
	AABB2 bottomUVs = blockDef->m_bottomUVs;
	AABB2 sideUVs = blockDef->m_sideUVs;

	IntVec3 localBlockCoords = GetBlockLocalCoordsFromIndex(blockIndex);
	IntVec3 globalBlockCoords = GetBlockGlobalCoordsFromChunk(m_chunkCoords, localBlockCoords);
	AABB3 blockWorldBounds = AABB3(Vec3(globalBlockCoords), Vec3(globalBlockCoords + IntVec3(1, 1, 1)));

	float minX = blockWorldBounds.m_mins.x;
	float minY = blockWorldBounds.m_mins.y;
	float minZ = blockWorldBounds.m_mins.z;
	float maxX = blockWorldBounds.m_maxs.x;
	float maxY = blockWorldBounds.m_maxs.y;
	float maxZ = blockWorldBounds.m_maxs.z;

	// p-max n-min
	Vec3 nnn(minX, minY, minZ);
	Vec3 nnp(minX, minY, maxZ);
	Vec3 npn(minX, maxY, minZ);
	Vec3 npp(minX, maxY, maxZ);
	Vec3 pnn(maxX, minY, minZ);
	Vec3 pnp(maxX, minY, maxZ);
	Vec3 ppn(maxX, maxY, minZ);
	Vec3 ppp(maxX, maxY, maxZ);

	if (blockDef->m_isOpaque)
	{
		BlockIterator currentIter(this, blockIndex);

		AddQuadForFace(currentIter.GetEastNeighbor(), pnn, ppn, ppp, pnp, sideUVs, 230); // +x, east
		AddQuadForFace(currentIter.GetWestNeighbor(), npn, nnn, nnp, npp, sideUVs, 230); // -x, west
		AddQuadForFace(currentIter.GetNorthNeighbor(), ppn, npn, npp, ppp, sideUVs, 200); // +y, north
		AddQuadForFace(currentIter.GetSouthNeighbor(), nnn, pnn, pnp, nnp, sideUVs, 200); // -y, south
		AddQuadForFace(currentIter.GetSkywardNeighbor(), nnp, pnp, ppp, npp, topUVs, 255); // +z, skyward
		AddQuadForFace(currentIter.GetDownwardNeighbor(), npn, ppn, pnn, nnn, bottomUVs, 255); // -z, downward
	}
	else
	{
		// Water Block Only has Quad between air (not visible)
		BlockIterator currentIter(this, blockIndex);

		AddQuadForNotOpaqueFace(currentIter.GetEastNeighbor(), pnn, ppn, ppp, pnp, sideUVs, 230); // +x, east
		AddQuadForNotOpaqueFace(currentIter.GetWestNeighbor(), npn, nnn, nnp, npp, sideUVs, 230); // -x, west
		AddQuadForNotOpaqueFace(currentIter.GetNorthNeighbor(), ppn, npn, npp, ppp, sideUVs, 200); // +y, north
		AddQuadForNotOpaqueFace(currentIter.GetSouthNeighbor(), nnn, pnn, pnp, nnp, sideUVs, 200); // -y, south
		AddQuadForNotOpaqueFace(currentIter.GetSkywardNeighbor(), nnp, pnp, ppp, npp, topUVs, 255); // +z, skyward
		AddQuadForNotOpaqueFace(currentIter.GetDownwardNeighbor(), npn, ppn, pnn, nnn, bottomUVs, 255); // -z, downward


	}

	//auto isBlockOpaque = [](const BlockIterator& it) -> bool
	//	{
	//		//if (!it.IsValid()) return false;
	//		//return it.GetBlock()->IsOpaque();

	//		//return it.IsValid() && it.GetBlock()->IsOpaque();
	//		return it.IsValid() && it.GetBlock()->IsFullOpaque();
	//	};

	//BlockIterator neighborIter = currentIter.GetEastNeighbor();
	//
	//if (!isBlockOpaque(neighborIter))
	//{
	//	AddVertsForQuad3D(m_vertices, m_indices, pnn, ppn, ppp, pnp, Rgba8(230, 230, 230), sideUVs); // +x, east
	//}

	//neighborIter = currentIter.GetWestNeighbor();
	//if (!isBlockOpaque(neighborIter))
	//{
	//	AddVertsForQuad3D(m_vertices, m_indices, npn, nnn, nnp, npp, Rgba8(230, 230, 230), sideUVs); // -x, west

	//}

	//neighborIter = currentIter.GetNorthNeighbor();
	//if (!isBlockOpaque(neighborIter))
	//{
	//	AddVertsForQuad3D(m_vertices, m_indices, ppn, npn, npp, ppp, Rgba8(200, 200, 200), sideUVs); // +y, north

	//}

	//neighborIter = currentIter.GetSouthNeighbor();
	//if (!isBlockOpaque(neighborIter))
	//{
	//	AddVertsForQuad3D(m_vertices, m_indices, nnn, pnn, pnp, nnp, Rgba8(200, 200, 200), sideUVs); // -y, south
	//}

	//neighborIter = currentIter.GetSkywardNeighbor();
	//if (!isBlockOpaque(neighborIter))
	//{
	//	AddVertsForQuad3D(m_vertices, m_indices, nnp, pnp, ppp, npp, Rgba8(255, 255, 255), topUVs); // +z, skyward
	//}

	//neighborIter = currentIter.GetDownwardNeighbor();
	//if (!isBlockOpaque(neighborIter))
	//{
	//	AddVertsForQuad3D(m_vertices, m_indices, npn, ppn, pnn, nnn, Rgba8(255, 255, 255), bottomUVs); // -z, downward
	//}
}

void Chunk::AddQuadForFace(BlockIterator const& neighborIter, Vec3 const& bottomLeft, Vec3 const& bottomRight, Vec3 const& topRight, Vec3 const& topLeft, AABB2 const& UVs, unsigned char vertexColor)
{
	if (!neighborIter.IsValid())
	{
		return;
	}

	Block* block = neighborIter.GetBlock();
	if (block->IsFullOpaque())
	{
		return;
	}

	float indoorLight = static_cast<float>(block->GetIndoorLightInfluence()) / static_cast<float>(LIGHT_MAX_VALUE);
	float outdoorLight = static_cast<float>(block->GetOutdoorLightInfluence()) / static_cast<float>(LIGHT_MAX_VALUE);

	Rgba8 color(DenormalizeByte(outdoorLight), DenormalizeByte(indoorLight), vertexColor);

	AddVertsForQuad3D(m_vertices, m_indices, bottomLeft, bottomRight, topRight, topLeft, color, UVs);
}

void Chunk::AddQuadForNotOpaqueFace(BlockIterator const& neighborIter, Vec3 const& bottomLeft, Vec3 const& bottomRight, Vec3 const& topRight, Vec3 const& topLeft, AABB2 const& UVs, unsigned char vertexColor)
{
	if (!neighborIter.IsValid())
	{
		return;
	}

	Block* block = neighborIter.GetBlock();
	if (block->IsVisible())
	{
		return;
	}

	float indoorLight = static_cast<float>(block->GetIndoorLightInfluence()) / static_cast<float>(LIGHT_MAX_VALUE);
	float outdoorLight = static_cast<float>(block->GetOutdoorLightInfluence()) / static_cast<float>(LIGHT_MAX_VALUE);

	Rgba8 color(DenormalizeByte(outdoorLight), DenormalizeByte(indoorLight), vertexColor);

	AddVertsForQuad3D(m_vertices, m_indices, bottomLeft, bottomRight, topRight, topLeft, color, UVs);
	AddVertsForQuad3D(m_vertices, m_indices, bottomRight, bottomLeft, topLeft, topRight, color, UVs); // Double Side Water
}

void Chunk::RenderBlocks() const
{
	if (m_vertexBuffer != nullptr && m_indexBuffer != nullptr)
	{
		DitherFadeRenderResources resources;
		resources.diffuseTextureIndex = g_theRenderer->GetSrvIndexFromLoadedTexture(g_blockTexture, DefaultTexture::WhiteOpaque2D);
		resources.diffuseSamplerIndex = g_theRenderer->GetDefaultSamplerIndex(SamplerMode::POINT_CLAMP);
		resources.cameraConstantsIndex = g_theRenderer->GetCurrentCameraConstantsIndex();
		resources.modelConstantsIndex = g_theRenderer->GetCurrentModelConstantsIndex();

		float t = GetClampedZeroToOne(m_startFadeInElapsedSeconds / FADE_IN_SECONDS);
		resources.fadeAmount = SmoothEnd3(t);

		resources.worldConstantsIndex = m_world->GetWorldConstantsIndex();
		resources.skyQuadSRVIndex = m_world->GetSkyQuadSRVIndex();

		g_theRenderer->SetGraphicsBindlessResources(sizeof(DitherFadeRenderResources), &resources);

		g_theRenderer->BindShader(g_theGame->m_ditherShader);
		g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
		g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
		g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
		//g_theRenderer->SetRenderTargetFormats();

		g_theRenderer->DrawIndexedVertexBuffer(m_vertexBuffer, m_indexBuffer, m_indexBuffer->GetCount());
	}
}

void Chunk::DebugRenderChunkOutline() const
{
	std::vector<Vertex_PCU> verts;
	AddVertsForAABB3D(verts, m_worldBounds);

	// resource settings
	UnlitRenderResources resources;
	resources.diffuseTextureIndex = g_theRenderer->GetSrvIndexFromLoadedTexture(nullptr, DefaultTexture::WhiteOpaque2D);
	resources.diffuseSamplerIndex = g_theRenderer->GetDefaultSamplerIndex(SamplerMode::POINT_CLAMP);
	resources.cameraConstantsIndex = g_theRenderer->GetCurrentCameraConstantsIndex();
	resources.modelConstantsIndex = g_theRenderer->GetCurrentModelConstantsIndex();

	g_theRenderer->SetGraphicsBindlessResources(sizeof(UnlitRenderResources), &resources);

	g_theRenderer->BindShader(nullptr);
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->SetRasterizerMode(RasterizerMode::WIREFRAME_CULL_NONE);
	g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	//g_theRenderer->SetRenderTargetFormats();

	g_theRenderer->DrawVertexArray(verts);
}

void Chunk::DebugDrawBiome(int layerType) const
{
	if (layerType == NO_DEBUG_LAYER)
		return;

	const Vec3 worldOrigin = Vec3(
		static_cast<float>(m_chunkCoords.x * CHUNK_SIZE_X),
		static_cast<float>(m_chunkCoords.y * CHUNK_SIZE_Y),
		static_cast<float>(CHUNK_SIZE_Z));
	std::vector<Vertex_PCU> verts;
	verts.reserve(CHUNK_SIZE_X * CHUNK_SIZE_Y * 6);

	if (layerType == CONTINENT)
	{
		for (int localY = 0; localY < CHUNK_SIZE_Y; ++localY)
		{
			for (int localX = 0; localX < CHUNK_SIZE_X; ++localX)
			{
				const int idxXY = localY * CHUNK_SIZE_X + localX;

				float value = m_continent[idxXY];

				Continentalness type = NoiseRangeMappers::m_continentalnessMapper.Map(value);

				float rgbScale = static_cast<float>(type) / static_cast<float>(Continentalness::COUNT);

				Rgba8 color = Rgba8::OPAQUE_WHITE;
				color.ScaleRGB(rgbScale);

				Vec3 quadOffset = worldOrigin + Vec3(
					static_cast<float>(localX),
					static_cast<float>(localY),
					0.f);

				AddVertsForQuad3D(verts, quadOffset, quadOffset + Vec3(1.f, 0.f, 0.f), quadOffset + Vec3(1.f, 1.f, 0.f), quadOffset + Vec3(0.f, 1.f, 0.f), color);
			}
		}
		DebugAddWorldTriangleList(verts, 0.f);
	}
	else if (layerType == EROSION)
	{
		for (int localY = 0; localY < CHUNK_SIZE_Y; ++localY)
		{
			for (int localX = 0; localX < CHUNK_SIZE_X; ++localX)
			{
				const int idxXY = localY * CHUNK_SIZE_X + localX;

				float value = m_erosion[idxXY];

				Erosion type = NoiseRangeMappers::m_erosionMapper.Map(value);

				float rgbScale = static_cast<float>(type) / static_cast<float>(Erosion::COUNT);

				Rgba8 color = Rgba8::OPAQUE_WHITE;
				color.ScaleRGB(rgbScale);

				Vec3 quadOffset = worldOrigin + Vec3(
					static_cast<float>(localX),
					static_cast<float>(localY),
					0.f);

				AddVertsForQuad3D(verts, quadOffset, quadOffset + Vec3(1.f, 0.f, 0.f), quadOffset + Vec3(1.f, 1.f, 0.f), quadOffset + Vec3(0.f, 1.f, 0.f), color);
			}
		}
		DebugAddWorldTriangleList(verts, 0.f);
	}
	else if (layerType == PEAK_AND_VALLEY)
	{
		for (int localY = 0; localY < CHUNK_SIZE_Y; ++localY)
		{
			for (int localX = 0; localX < CHUNK_SIZE_X; ++localX)
			{
				const int idxXY = localY * CHUNK_SIZE_X + localX;

				float value = m_peaksValleys[idxXY];

				PeaksValleys type = NoiseRangeMappers::m_peaksValleysMapper.Map(value);

				float rgbScale = static_cast<float>(type) / static_cast<float>(PeaksValleys::COUNT);

				Rgba8 color = Rgba8::OPAQUE_WHITE;
				color.ScaleRGB(rgbScale);

				Vec3 quadOffset = worldOrigin + Vec3(
					static_cast<float>(localX),
					static_cast<float>(localY),
					0.f);

				AddVertsForQuad3D(verts, quadOffset, quadOffset + Vec3(1.f, 0.f, 0.f), quadOffset + Vec3(1.f, 1.f, 0.f), quadOffset + Vec3(0.f, 1.f, 0.f), color);
			}
		}
		DebugAddWorldTriangleList(verts, 0.f);
	}
	else if (layerType == TEMPERATURE)
	{
		for (int localY = 0; localY < CHUNK_SIZE_Y; ++localY)
		{
			for (int localX = 0; localX < CHUNK_SIZE_X; ++localX)
			{
				const int idxXY = localY * CHUNK_SIZE_X + localX;

				float value = m_temperature[idxXY];

				Temperature type = NoiseRangeMappers::m_temperatureMapper.Map(value);

				float rgbScale = static_cast<float>(type) / static_cast<float>(Temperature::COUNT);

				Rgba8 color = Rgba8::OPAQUE_WHITE;
				color.ScaleRGB(rgbScale);

				Vec3 quadOffset = worldOrigin + Vec3(
					static_cast<float>(localX),
					static_cast<float>(localY),
					0.f);

				AddVertsForQuad3D(verts, quadOffset, quadOffset + Vec3(1.f, 0.f, 0.f), quadOffset + Vec3(1.f, 1.f, 0.f), quadOffset + Vec3(0.f, 1.f, 0.f), color);
			}
		}
		DebugAddWorldTriangleList(verts, 0.f);
	}
	else if (layerType == HUMIDITY)
	{
		for (int localY = 0; localY < CHUNK_SIZE_Y; ++localY)
		{
			for (int localX = 0; localX < CHUNK_SIZE_X; ++localX)
			{
				const int idxXY = localY * CHUNK_SIZE_X + localX;

				float value = m_humidity[idxXY];

				Humidity type = NoiseRangeMappers::m_humidityMapper.Map(value);

				float rgbScale = static_cast<float>(type) / static_cast<float>(Humidity::COUNT);

				Rgba8 color = Rgba8::OPAQUE_WHITE;
				color.ScaleRGB(rgbScale);

				Vec3 quadOffset = worldOrigin + Vec3(
					static_cast<float>(localX),
					static_cast<float>(localY),
					0.f);

				AddVertsForQuad3D(verts, quadOffset, quadOffset + Vec3(1.f, 0.f, 0.f), quadOffset + Vec3(1.f, 1.f, 0.f), quadOffset + Vec3(0.f, 1.f, 0.f), color);
			}
		}
		DebugAddWorldTriangleList(verts, 0.f);
	}
}

std::array<float, CHUNK_SIZE_X* CHUNK_SIZE_Y> const & Chunk::GetDebugLayer(int layerType) const
{
	switch (layerType) {
	case CONTINENT:
		return m_continent;
	case EROSION:
		return m_erosion;
	case PEAK_AND_VALLEY:
		return m_peaksValleys;
	case TEMPERATURE:
		return m_temperature;
	case HUMIDITY:
		return m_humidity;
	default:
		ERROR_AND_DIE("Invalid Debug Layer Type.");
	}
}

void Chunk::DebugRenderChunkGrid() const
{
	std::vector<Vertex_PCU> verts;

	constexpr float	SMALL_OFFSET = 1e-3f;

	float minX = m_worldBounds.m_mins.x + SMALL_OFFSET;
	float minY = m_worldBounds.m_mins.y + SMALL_OFFSET;
	float minZ = m_worldBounds.m_mins.z + SMALL_OFFSET;
	float maxX = m_worldBounds.m_maxs.x - SMALL_OFFSET;
	float maxY = m_worldBounds.m_maxs.y - SMALL_OFFSET;
	float maxZ = m_worldBounds.m_maxs.z - SMALL_OFFSET;

	// p-max n-min
	Vec3 nnn(minX, minY, minZ);
	Vec3 nnp(minX, minY, maxZ);
	Vec3 npn(minX, maxY, minZ);
	Vec3 npp(minX, maxY, maxZ);
	Vec3 pnn(maxX, minY, minZ);
	Vec3 pnp(maxX, minY, maxZ);
	Vec3 ppn(maxX, maxY, minZ);
	Vec3 ppp(maxX, maxY, maxZ);

	// Make them face inside => a special effect
	// use world pos as uv
	AddVertsForQuad3D(verts, ppn, pnn, pnp, ppp, Rgba8(255, 255, 255), AABB2(minY, minZ, maxY, maxZ)); // +x
	AddVertsForQuad3D(verts, nnn, npn, npp, nnp, Rgba8(255, 255, 255), AABB2(-maxY, minZ, -minY, maxZ)); // -x or maxY, minZ, minY, maxZ
	AddVertsForQuad3D(verts, npn, ppn, ppp, npp, Rgba8(255, 255, 255), AABB2(-maxX, minZ, -minX, maxZ)); // +y
	AddVertsForQuad3D(verts, pnn, nnn, nnp, pnp, Rgba8(255, 255, 255), AABB2(minX, minZ, maxX, maxZ)); // -y
	AddVertsForQuad3D(verts, pnp, nnp, npp, ppp, Rgba8(255, 255, 255), AABB2(minX, minY, maxX, maxY)); // +z
	AddVertsForQuad3D(verts, ppn, npn, nnn, pnn, Rgba8(255, 255, 255), AABB2(minX, -maxY, maxX, -minY)); // -z

	Texture* gridTex = g_theRenderer->CreateOrGetTextureFromFile("Data/Images/grid32.png");

	// resource settings
	UnlitRenderResources resources;
	resources.diffuseTextureIndex = g_theRenderer->GetSrvIndexFromLoadedTexture(gridTex, DefaultTexture::WhiteOpaque2D);
	resources.diffuseSamplerIndex = g_theRenderer->GetDefaultSamplerIndex(SamplerMode::POINT_WARP);
	resources.cameraConstantsIndex = g_theRenderer->GetCurrentCameraConstantsIndex();
	resources.modelConstantsIndex = g_theRenderer->GetCurrentModelConstantsIndex();

	g_theRenderer->SetGraphicsBindlessResources(sizeof(UnlitRenderResources), &resources);

	g_theRenderer->BindShader(nullptr);
	g_theRenderer->SetBlendMode(BlendMode::ALPHA);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	//g_theRenderer->SetRenderTargetFormats();

	g_theRenderer->DrawVertexArray(verts);
}

std::string Chunk::GetChunkFilePath() const
{
	return GetChunkFilePath(m_chunkCoords);
}

std::string Chunk::GetChunkFilePath(IntVec2 chunkCoords)
{
	std::string name = Stringf("Chunk(%d,%d).chunk", chunkCoords.x, chunkCoords.y);
	namespace fs = std::filesystem;
	fs::path dir = fs::u8path(GetSavesDirectory());
	fs::path file = dir / name;
	return file.u8string();
}

void Chunk::LoadFromDisk()
{
	// Notes: if there is an error when loading, it will use the corrupted data
	std::string const filePath = GetChunkFilePath();

	if (!FileExists(filePath))
	{
		return;
	}

	std::vector<uint8_t> buffer;
	FileReadToBuffer(buffer, filePath);
	if (buffer.size() < sizeof(ChunkFileHeader))
	{
		return;
	}

	// Check Header
	ChunkFileHeader const* header = reinterpret_cast<ChunkFileHeader const*>(buffer.data());
	if (header->m_fourCC[0] != 'G' || header->m_fourCC[1] != 'C' || header->m_fourCC[2] != 'H' || header->m_fourCC[3] != 'K') return;
	if (header->m_version != CHUNK_FILE_VERSION) return;
	if (header->m_bitsX != CHUNK_BITS_X || header->m_bitsY != CHUNK_BITS_Y || header->m_bitsZ != CHUNK_BITS_Z) return;

	// Check 2-byte run
	const size_t runsBytes = buffer.size() - sizeof(ChunkFileHeader);
	if ((runsBytes % sizeof(ChunkRun)) != 0)
	{
		return;
	}
	const size_t runsCount = runsBytes / sizeof(ChunkRun);


	size_t outIndex = 0;
	const uint8_t* runsPtr = buffer.data() + sizeof(ChunkFileHeader);

	for (size_t i = 0; i < runsCount; ++i)
	{
		ChunkRun const* run = reinterpret_cast<ChunkRun const*>(runsPtr + i * sizeof(ChunkRun));
		const uint8_t type = run->m_type;
		const uint8_t len = run->m_length;

		// check run length
		if (len == 0)
		{
			return;
		}

		// check if data exceeded
		if (outIndex + len > static_cast<size_t>(BLOCKS_PER_CHUNK))
		{
			return;
		}
		for (uint8_t k = 0; k < len; ++k)
		{
			m_blocks[outIndex].SetTypeID(type);
			++outIndex;
		}
	}

	// Wrong Block Count, but already write in m_blocks
	if (outIndex != static_cast<size_t>(BLOCKS_PER_CHUNK))
	{
		return;
	}
}

void Chunk::SaveToDisk()
{
	if (!m_needsSaving)
	{
		return;
	}

	// Not Checking if  Directory Exists (will crash)

	std::string const filename = GetChunkFilePath();

	std::vector<uint8_t> out;
	out.reserve(BLOCKS_PER_CHUNK / 4); // Predicted number

	ChunkFileHeader header{};
	header.m_fourCC[0] = 'G';
	header.m_fourCC[1] = 'C';
	header.m_fourCC[2] = 'H';
	header.m_fourCC[3] = 'K';
	header.m_version = 1;
	header.m_bitsX = static_cast<uint8_t>(CHUNK_BITS_X);
	header.m_bitsY = static_cast<uint8_t>(CHUNK_BITS_Y);
	header.m_bitsZ = static_cast<uint8_t>(CHUNK_BITS_Z);

	const uint8_t* headerBytes = reinterpret_cast<const uint8_t*>(&header);
	out.insert(out.end(), headerBytes, headerBytes + sizeof(ChunkFileHeader));

	// RLE Encoding
	uint8_t currentType = m_blocks[0].GetTypeID();
	uint8_t runLen = 0;

	for (int i = 0; i < BLOCKS_PER_CHUNK; ++i)
	{
		const uint8_t t = m_blocks[i].GetTypeID();

		if (t != currentType || runLen == 255)
		{
			out.push_back(currentType);
			out.push_back(runLen);

			// New run
			currentType = t;
			runLen = 0;
		}

		++runLen;
	}

	// Last Run
	if (runLen > 0)
	{
		out.push_back(currentType);
		out.push_back(runLen);
	}

	const int written = FileWriteFromBuffer(out, filename);
	if (written >= static_cast<int>(sizeof(ChunkFileHeader)))
	{
		// Written Success
		m_needsSaving = false; 
	}
}

void Chunk::GenerateBlocks()
{
	//InitializeRandomBlocks();
	InitializePCGBlocks();
}


void Chunk::UpdateLightInfluenceAfterActivating()
{
	// Marking SKY
	for (int y = 0; y < CHUNK_SIZE_Y; ++y)
	{
		for (int x = 0; x < CHUNK_SIZE_X; ++x)
		{
			for (int z = CHUNK_MAX_Z; z >= 0; --z)
			{
				Block& block = m_blocks[GetBlockIndexInChunk(x, y, z)];
				if (block.IsFullOpaque())
				{
					break;
				}
				block.SetIsSky(true);
			}
		}
	}

	// Set SKY outdoor light influence to MAXIMUM, Mark its non-opaque and non-sky horizontal neighbors dirty
	auto isNonOpaqueAndNonSky = [](const BlockIterator& it) -> bool
		{
			return it.IsValid() && !it.GetBlock()->IsFullOpaque() && !it.GetBlock()->IsSky();
		};

	for (int y = 0; y < CHUNK_SIZE_Y; ++y)
	{
		for (int x = 0; x < CHUNK_SIZE_X; ++x)
		{
			for (int z = CHUNK_MAX_Z; z >= 0; --z)
			{
				int blockIndex = GetBlockIndexInChunk(x, y, z);
				Block& block = m_blocks[blockIndex];

				if (block.IsFullOpaque())
				{
					break;
				}

				block.SetOutdoorLightInfluence(LIGHT_MAX_VALUE);

				BlockIterator currentIter(this, blockIndex);

				BlockIterator neighborIter = currentIter.GetEastNeighbor();
				if (isNonOpaqueAndNonSky(neighborIter))
				{
					m_world->MarkLightingDirty(neighborIter);
				}

				neighborIter = currentIter.GetWestNeighbor();
				if (isNonOpaqueAndNonSky(neighborIter))
				{
					m_world->MarkLightingDirty(neighborIter);
				}

				neighborIter = currentIter.GetNorthNeighbor();
				if (isNonOpaqueAndNonSky(neighborIter))
				{
					m_world->MarkLightingDirty(neighborIter);
				}

				neighborIter = currentIter.GetSouthNeighbor();
				if (isNonOpaqueAndNonSky(neighborIter))
				{
					m_world->MarkLightingDirty(neighborIter);
				}
			}
		}
	}

	for (int z = 0; z < CHUNK_SIZE_Z; ++z)
	{
		for (int y = 0; y < CHUNK_SIZE_Y; ++y)
		{
			for (int x = 0; x < CHUNK_SIZE_X; ++x)
			{
				int blockIndex = GetBlockIndexInChunk(x, y, z);
				Block& block = m_blocks[blockIndex];
				BlockIterator currentIter(this, blockIndex);

				// Emits Light, Mark Dirty
				if (BlockDefinition::GetLightInfluenceByType(block.GetTypeID()) > 0)
				{
					m_world->MarkLightingDirty(currentIter);
				}

				// Mark non-opaque boundary blocks with neighbor chunk dirty
				if (x == 0 && m_westNeighbor ||
					x == CHUNK_MAX_X && m_eastNeighbor || 
					y == 0 && m_southNeighbor || 
					y == CHUNK_MAX_Y && m_northNeighbor)
				{
					if (!block.IsFullOpaque())
					{
						m_world->MarkLightingDirty(currentIter);
					}
				}
			}
		}
	}

}

void Chunk::UpdateLightInfluenceBeforeDeactivating()
{
	// #ToDo mark neighbor dirty? Not necessary? light is still there, although deactivated
	m_world->UndirtyAllBlocksInChunk(this);
}

void Chunk::GenerateTrees(const PCGParams& params)
{
	const uint8_t airType = BlockDefinition::GetBlockTypeIDByName("Air");

	const int chunkMinX = m_chunkCoords.x * CHUNK_SIZE_X;
	const int chunkMinY = m_chunkCoords.y * CHUNK_SIZE_Y;

	// Determine maximum tree radius to check neighboring chunks
	int maxTreeRadius = 3; // Conservative estimate

	// Expand search area to include trees from neighboring chunks
	for (int localY = -maxTreeRadius; localY < CHUNK_SIZE_Y + maxTreeRadius; ++localY)
	{
		const int globalY = chunkMinY + localY;

		for (int localX = -maxTreeRadius; localX < CHUNK_SIZE_X + maxTreeRadius; ++localX)
		{
			const int globalX = chunkMinX + localX;
			const int idxXY = (localY >= 0 && localY < CHUNK_SIZE_Y && localX >= 0 && localX < CHUNK_SIZE_X)
				? (localY * CHUNK_SIZE_X + localX) : -1;

			// Sample tree noise for this location
			const float treeNoise = Compute2dPerlinNoise(
				static_cast<float>(globalX),
				static_cast<float>(globalY),
				params.m_treeNoiseScale,
				3,  // octaves
				0.5f,
				2.0f,
				true,
				params.m_treeSeed
			);



			// Check if this is a local maximum (higher than all 8 neighbors)
			if (!IsLocalMaximum(localX, localY, treeNoise, params))
			{
				continue;
			}

			// Get biome information
			float continent, erosion, peaksValleys, temperature, humidity;
			if (idxXY >= 0)
			{
				// Inside chunk, use cached values
				continent = m_continent[idxXY];
				erosion = m_erosion[idxXY];
				peaksValleys = m_peaksValleys[idxXY];
				temperature = m_temperature[idxXY];
				humidity = m_humidity[idxXY];
			}
			else
			{
				// Outside chunk, recalculate
				continent = params.m_continentNoise.SamplePerlinNoise2D(globalX, globalY);
				erosion = params.m_erosionNoise.SamplePerlinNoise2D(globalX, globalY);
				peaksValleys = CalculatePV(params.m_peaksValleysNoise.SamplePerlinNoise2D(globalX, globalY));
				temperature = params.m_temperatureNoise.SamplePerlinNoise2D(globalX, globalY);
				humidity = params.m_humidityNoise.SamplePerlinNoise2D(globalX, globalY);
			}

			// Map to discrete categories
			Continentalness cont = NoiseRangeMappers::m_continentalnessMapper.Map(continent);
			Erosion ero = NoiseRangeMappers::m_erosionMapper.Map(erosion);
			PeaksValleys pv = NoiseRangeMappers::m_peaksValleysMapper.Map(peaksValleys);
			Temperature temp = NoiseRangeMappers::m_temperatureMapper.Map(temperature);
			Humidity hum = NoiseRangeMappers::m_humidityMapper.Map(humidity);

			BiomeType biome = BiomeSelector::SelectBiome(cont, ero, pv, temp, hum);

			// Determine if we should place a tree based on biome and threshold
			float threshold = 1.0f; // Default: no trees

			switch (biome)
			{
			case BiomeType::DESERT:
				threshold = params.m_desertCactusThreshold;
				break;
			case BiomeType::SAVANNA:
				threshold = params.m_savannaTreeThreshold;
				break;
			case BiomeType::FOREST:
				threshold = params.m_forestTreeThreshold;
				break;
			case BiomeType::JUNGLE:
				threshold = params.m_jungleTreeThreshold;
				break;
			case BiomeType::TAIGA:
			case BiomeType::SNOWY_TAIGA:
				threshold = params.m_taigaTreeThreshold;
				break;
			default:
				continue; // No trees in this biome
			}

			// Adjust threshold slightly based on temperature and humidity
			float tempFactor = (static_cast<int>(temp) - 2) * 0.01f; // -0.02 to +0.02
			float humidityFactor = (static_cast<int>(hum) - 2) * 0.01f; // -0.02 to +0.02
			threshold += tempFactor + humidityFactor;

			// Check if tree should be placed
			if (treeNoise < threshold)
			{
				continue;
			}

			// Find surface height at this location
			// We need to check the actual world blocks (may need to look at neighboring chunks)
			int surfaceZ = -1;
			uint8_t surfaceBlockType = airType;

			// If within our chunk, find surface directly
			if (localX >= 0 && localX < CHUNK_SIZE_X && localY >= 0 && localY < CHUNK_SIZE_Y)
			{
				for (int localZ = CHUNK_SIZE_Z - 1; localZ >= 0; --localZ)
				{
					const int idx = GetBlockIndexInChunk(localX, localY, localZ);
					if (m_blocks[idx].GetTypeID() != airType)
					{
						surfaceZ = localZ;
						surfaceBlockType = m_blocks[idx].GetTypeID();
						break;
					}
				}
			}
			else
			{
				// For trees outside our chunk, we'd need to query neighboring chunks
				// For simplicity, skip trees that originate outside our chunk
				//continue;


				// For trees outside our chunk, recalculate terrain to find surface
				// Already calculated above: continent, erosion, peaksValleys, temperature, humidity

				// Get block type IDs needed for surface determination
				const uint8_t sandType = BlockDefinition::GetBlockTypeIDByName("Sand");
				const uint8_t snowType = BlockDefinition::GetBlockTypeIDByName("Snow");
				const uint8_t dirtType = BlockDefinition::GetBlockTypeIDByName("Dirt");
				const uint8_t stoneType = BlockDefinition::GetBlockTypeIDByName("Stone");
				const uint8_t grassType = BlockDefinition::GetBlockTypeIDByName("Grass");
				const uint8_t grassLightType = BlockDefinition::GetBlockTypeIDByName("GrassLight");
				const uint8_t grassDarkType = BlockDefinition::GetBlockTypeIDByName("GrassDark");
				const uint8_t grassYellowType = BlockDefinition::GetBlockTypeIDByName("GrassYellow");

				// Find surface by recalculating density from top to bottom
				for (int localZ = CHUNK_SIZE_Z - 1; localZ >= 0; --localZ)
				{
					const int globalZ = localZ;

					// Recalculate density using same formula as Pass 2
					float h = params.m_heightOffsetSpline.GetValueAtInputKey(continent);
					float s = params.m_squashingSpline.GetValueAtInputKey(continent);
					float e = params.m_erosionFactorSpline.GetValueAtInputKey(erosion);

					float density = params.m_densityNoise.SamplePerlinNoise3D(globalX, globalY, globalZ);

					float base = params.m_baseHeight;
					float t = (static_cast<float>(globalZ) - base) / base;

					// Dynamic Bias
					float continentalityFactor = (continent + 1.0f) * 0.5f;
					float biasStrength = params.m_biasStrengthMin +
						(params.m_biasStrengthMax - params.m_biasStrengthMin) * continentalityFactor;
					float bias = (static_cast<float>(globalZ) - base) * (-biasStrength);
					density += bias;

					// Height Offset
					density += h;

					density -= s * t;
					density -= e * t;

					float peaksValleysEffect = peaksValleys * continentalityFactor * t * params.m_peaksValleysStrength;
					density -= peaksValleysEffect;

					// Check if this is solid block
					if (density > 0.f)
					{
						// Found the surface!
						surfaceZ = localZ;

						// Determine surface block type based on biome (same logic as Pass 3)
						const bool isAboveSeaLevel = localZ > static_cast<int>(params.m_seaLevel);

						switch (biome)
						{
						case BiomeType::OCEAN:
							surfaceBlockType = isAboveSeaLevel ? sandType : dirtType;
							break;

						case BiomeType::DEEP_OCEAN:
							surfaceBlockType = isAboveSeaLevel ?
								((temp == Temperature::T0) ? snowType : sandType) : stoneType;
							break;

						case BiomeType::FROZEN_OCEAN:
							surfaceBlockType = isAboveSeaLevel ? snowType : stoneType;
							break;

						case BiomeType::BEACH:
							surfaceBlockType = sandType;
							break;

						case BiomeType::SNOWY_BEACH:
							surfaceBlockType = snowType;
							break;

						case BiomeType::DESERT:
							surfaceBlockType = sandType;
							break;

						case BiomeType::SAVANNA:
							surfaceBlockType = grassYellowType;
							break;

						case BiomeType::PLAINS:
							surfaceBlockType = grassLightType;
							break;

						case BiomeType::SNOWY_PLAINS:
							surfaceBlockType = snowType;
							break;

						case BiomeType::FOREST:
							surfaceBlockType = grassType;
							break;

						case BiomeType::JUNGLE:
							surfaceBlockType = grassDarkType;
							break;

						case BiomeType::TAIGA:
							surfaceBlockType = grassLightType;
							break;

						case BiomeType::SNOWY_TAIGA:
							surfaceBlockType = snowType;
							break;

						case BiomeType::STONY_PEAKS:
							surfaceBlockType = stoneType;
							break;

						case BiomeType::SNOWY_PEAKS:
							surfaceBlockType = snowType;
							break;

						default:
							surfaceBlockType = dirtType;
							break;
						}

						break; // Found surface, exit loop
					}
				}
			}

			if (surfaceZ < 0 || surfaceZ <= static_cast<int>(params.m_seaLevel))
			{
				continue; // No valid surface or underwater
			}

			// Check if surface block is valid for tree placement
			if (!CanPlaceTree(surfaceZ, biome, surfaceBlockType))
			{
				continue;
			}

			// Select tree type based on biome
			TreeType treeType = SelectTreeType(biome, temp, hum, globalX, globalY);

			// Get tree stamp and place tree
			const TreeStamp& stamp = TreeGenerator::GetTreeStamp(treeType);
			PlaceTree(globalX, globalY, surfaceZ + 1, stamp); // +1 to place above surface
		}
	}
}

bool Chunk::IsLocalMaximum(int localX, int localY, float treeNoise, const PCGParams& params) const
{
	const int chunkMinX = m_chunkCoords.x * CHUNK_SIZE_X;
	const int chunkMinY = m_chunkCoords.y * CHUNK_SIZE_Y;
	const int globalX = chunkMinX + localX;
	const int globalY = chunkMinY + localY;

	// Check all 8 neighbors
	for (int dy = -1; dy <= 1; ++dy)
	{
		for (int dx = -1; dx <= 1; ++dx)
		{
			if (dx == 0 && dy == 0) continue; // Skip self

			const int neighborGlobalX = globalX + dx;
			const int neighborGlobalY = globalY + dy;

			const float neighborNoise = Compute2dPerlinNoise(
				static_cast<float>(neighborGlobalX),
				static_cast<float>(neighborGlobalY),
				params.m_treeNoiseScale,
				3,
				0.5f,
				2.0f,
				true,
				params.m_treeSeed
			);

			if (neighborNoise >= treeNoise)
			{
				return false; // Not a local maximum
			}
		}
	}

	return true;
}

bool Chunk::CanPlaceTree(int surfaceZ, BiomeType biome, uint8_t surfaceBlockType) const
{
	UNUSED(surfaceZ);

	const uint8_t sandType = BlockDefinition::GetBlockTypeIDByName("Sand");
	const uint8_t snowType = BlockDefinition::GetBlockTypeIDByName("Snow");
	const uint8_t dirtType = BlockDefinition::GetBlockTypeIDByName("Dirt");
	const uint8_t grassType = BlockDefinition::GetBlockTypeIDByName("Grass");
	const uint8_t grassLightType = BlockDefinition::GetBlockTypeIDByName("GrassLight");
	const uint8_t grassDarkType = BlockDefinition::GetBlockTypeIDByName("GrassDark");
	const uint8_t grassYellowType = BlockDefinition::GetBlockTypeIDByName("GrassYellow");

	// Cactus can only grow on sand
	if (biome == BiomeType::DESERT || biome == BiomeType::SAVANNA)
	{
		if (surfaceBlockType == sandType)
			return true;
	}

	// Trees can grow on grass, dirt, or snow
	return (surfaceBlockType == grassType ||
		surfaceBlockType == grassLightType ||
		surfaceBlockType == grassDarkType ||
		surfaceBlockType == grassYellowType ||
		surfaceBlockType == dirtType ||
		surfaceBlockType == snowType);
}

//TreeType Chunk::SelectTreeType(BiomeType biome, Temperature temp, Humidity hum) const
//{
//	switch (biome)
//	{
//	case BiomeType::DESERT:
//		return TreeType::CACTUS;
//
//	case BiomeType::SAVANNA:
//		// Mix of cactus and acacia
//	{
//		float mixNoise = Get2dNoiseZeroToOne(
//			m_chunkCoords.x * CHUNK_SIZE_X,
//			m_chunkCoords.y * CHUNK_SIZE_Y,
//			6000
//		);
//		return (mixNoise < 0.3f) ? TreeType::CACTUS : TreeType::ACACIA;
//	}
//
//	case BiomeType::FOREST:
//		// Mix of oak and birch
//	{
//		float mixNoise = Get2dNoiseZeroToOne(
//			m_chunkCoords.x * CHUNK_SIZE_X,
//			m_chunkCoords.y * CHUNK_SIZE_Y,
//			7000
//		);
//		return (mixNoise < 0.6f) ? TreeType::OAK : TreeType::BIRCH;
//	}
//
//	case BiomeType::JUNGLE:
//		return TreeType::JUNGLE;
//
//	case BiomeType::TAIGA:
//		return TreeType::SPRUCE;
//
//	case BiomeType::SNOWY_TAIGA:
//		return TreeType::SPRUCE_SNOWY;
//
//	default:
//		return TreeType::OAK;
//	}
//}

TreeType Chunk::SelectTreeType(BiomeType biome, Temperature temp, Humidity hum, int globalX, int globalY) const
{
	UNUSED(hum);
	UNUSED(temp);

	switch (biome)
	{
	case BiomeType::DESERT:
		return TreeType::CACTUS;

	case BiomeType::SAVANNA:
	{
		float mixNoise = Get2dNoiseZeroToOne(
			globalX,
			globalY,
			6000
		);
		return (mixNoise < 0.3f) ? TreeType::CACTUS : TreeType::ACACIA;
	}

	case BiomeType::FOREST:
	{
		float mixNoise = Get2dNoiseZeroToOne(
			globalX,
			globalY,
			7000
		);
		return (mixNoise < 0.6f) ? TreeType::OAK : TreeType::BIRCH;
	}

	case BiomeType::JUNGLE:
		return TreeType::JUNGLE;

	case BiomeType::TAIGA:
		return TreeType::SPRUCE;

	case BiomeType::SNOWY_TAIGA:
		return TreeType::SPRUCE_SNOWY;

	default:
		return TreeType::OAK;
	}
}

void Chunk::PlaceTree(int globalX, int globalY, int surfaceZ, const TreeStamp& stamp)
{
	const int chunkMinX = m_chunkCoords.x * CHUNK_SIZE_X;
	const int chunkMinY = m_chunkCoords.y * CHUNK_SIZE_Y;
	const uint8_t airType = BlockDefinition::GetBlockTypeIDByName("Air");

	// Iterate through all blocks in the stamp
	for (const TreeBlockOffset& offset : stamp.blocks)
	{
		const int worldX = globalX + offset.offsetX;
		const int worldY = globalY + offset.offsetY;
		const int worldZ = surfaceZ + offset.offsetZ;

		// Convert to local coordinates
		const int localX = worldX - chunkMinX;
		const int localY = worldY - chunkMinY;
		const int localZ = worldZ;

		// Check if block is within our chunk
		if (localX < 0 || localX >= CHUNK_SIZE_X ||
			localY < 0 || localY >= CHUNK_SIZE_Y ||
			localZ < 0 || localZ >= CHUNK_SIZE_Z)
		{
			continue; // Skip blocks outside our chunk
		}

		const int idx = GetBlockIndexInChunk(localX, localY, localZ);

		// Only place tree blocks in air
		if (m_blocks[idx].GetTypeID() == airType)
		{
			m_blocks[idx].SetTypeID(offset.blockType, false);
		}
	}
}