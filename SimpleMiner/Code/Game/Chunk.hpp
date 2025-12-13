#pragma once
#include "Game/GameCommon.hpp"
#include "Game/ChunkUtils.hpp"
#include "Game/PCGParams.hpp"
#include "Game/Block.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/AABB3.hpp"
#include <vector>
#include <string>
#include <atomic>
#include <array>

//-----------------------------------------------------------------------------------------------
struct AABB2;
class BlockIterator;

//-------------------------------------------------------------------------------------------
enum class ChunkState
{
	MISSING,                        // Optional; Used only in cases where we want to say that a chunk doesn't exist at all
	ON_DISK,                        // Optional; Used only in cases where we want to say a chunk is missing, but it exists on disk
	CONSTRUCTING,                   // [set by main thread] Initial ChunkState::m_state value during early construction

	ACTIVATING_QUEUED_LOAD,         // [set by main thread] Chunk has been added to the loading queue
	ACTIVATING_LOADING,             // [set by disk thread] chunk is being loaded & populated by disk i/o thread
	ACTIVATING_LOAD_COMPLETE,       // [set by disk thread] chunk is done loading and ready for main thread to claim

	ACTIVATING_QUEUED_GENERATE,     // [set by main thread] Chunk has been added to the generating queue
	ACTIVATING_GENERATING,          // [set by generator thread] chunk is being generated & populated by a generator thread
	ACTIVATING_GENERATE_COMPLETE,   // [set by generator thread] chunk is done generating and ready for main thread to claim

	ACTIVE,                         // [set by main thread] Chunk is in m_activeChunks; only main thread can touch it. Lighting, mesh building allowed.

	DEACTIVATING_QUEUED_SAVE,       // [set by main thread] Chunk has been deactivated, and is [being] queued for save
	DEACTIVATING_SAVING,            // [set by disk thread] Chunk is being compressed and saved by disk i/o thread
	DEACTIVATING_SAVE_COMPLETE,     // [set by disk thread] Chunk has been saved and is ready for main thread to claim
	DECONSTRUCTING,                 // [set by main thread] Chunk is being destroyed by main thread

	NUM_CHUNK_STATES
};



//-----------------------------------------------------------------------------------------------
class Chunk
{
	friend class ChunkFadeOutManager;
public:
	~Chunk();
	Chunk(World* world, IntVec2 chunkCoords);

	void Update(float deltaSeconds);
	void Render() const;
	void RenderDepth() const;

	IntVec2 GetChunkCoords() const { return m_chunkCoords; }
	AABB3 GetWorldBounds() const { return m_worldBounds; }
	void DebugRenderChunkGrid() const;

	// Activate & Deactivate
	void MarkMeshDirty() { m_isMeshDirty = true; };
	void SetNeedsSaving() { m_needsSaving = true; }
	bool IsMeshDirty() const { return m_isMeshDirty; }
	bool NeedsSaving() const { return m_needsSaving; }


	void SetEastNeighbor(Chunk* c) { m_eastNeighbor = c; }
	void SetWestNeighbor(Chunk* c) { m_westNeighbor = c; }
	void SetNorthNeighbor(Chunk* c) { m_northNeighbor = c; }
	void SetSouthNeighbor(Chunk* c) { m_southNeighbor = c; }

	Chunk* GetEastNeighbor() const { return m_eastNeighbor; }
	Chunk* GetWestNeighbor() const { return m_westNeighbor; }
	Chunk* GetNorthNeighbor() const { return m_northNeighbor; }
	Chunk* GetSouthNeighbor() const { return m_southNeighbor; }

	void RegenerateMeshIfDirty();
	bool UploadToGpuIfNeeded();


	bool LoadFromDiskIfExists();
	void SaveToDiskIfNeeded();

	static std::string GetSavesDirectory() { return "Saves"; }
	static std::string GetChunkFilePath(IntVec2 chunkCoords);
	std::string GetChunkFilePath() const;

	// Jobs
	void LoadFromDisk();
	void SaveToDisk();
	void GenerateBlocks();

	// Lighting
	void UpdateLightInfluenceAfterActivating();
	void UpdateLightInfluenceBeforeDeactivating();


public:
	int m_totalNumVertices = 0;
	int m_totalNumIndices = 0;

private:

	void InitializeRandomBlocks();
	void InitializePCGBlocks();
	void InitializeMeshes();

	void AddVertsForBlock(int blockIndex);
	void AddQuadForFace(BlockIterator const& neighborIter,
		Vec3 const& bottomLeft, Vec3 const& bottomRight, Vec3 const& topRight, Vec3 const& topLeft,
		AABB2 const& UVs, unsigned char vertexColor);

	void AddQuadForNotOpaqueFace(BlockIterator const& neighborIter,
		Vec3 const& bottomLeft, Vec3 const& bottomRight, Vec3 const& topRight, Vec3 const& topLeft,
		AABB2 const& UVs, unsigned char vertexColor);

	void RenderBlocks() const;
	void DebugRenderChunkOutline() const;


public:
	std::vector<Block> m_blocks;
	std::atomic<ChunkState> m_state = ChunkState::CONSTRUCTING;

	bool m_hasMeshGenerated = false; // for fade in

private:
	World* m_world = nullptr;
	IntVec2 m_chunkCoords;
	AABB3 m_worldBounds;
	float m_startFadeInElapsedSeconds = 0.f;

	std::vector<Vertex_PCU> m_vertices;
	std::vector<unsigned int> m_indices;
	VertexBuffer* m_vertexBuffer = nullptr;
	IndexBuffer* m_indexBuffer = nullptr;

	// Not used now
	std::vector<Vertex_PCU> m_debugVertices;
	std::vector<unsigned int> m_debugIndices;
	VertexBuffer* m_debugVertexBuffer = nullptr;
	IndexBuffer* m_debugIndexBuffer = nullptr;

private:
	bool m_isMeshDirty = false;
	bool m_needsSaving = false;
	bool m_needsUploadToGpu = false;

	Chunk* m_eastNeighbor = nullptr;
	Chunk* m_westNeighbor = nullptr;
	Chunk* m_northNeighbor = nullptr;
	Chunk* m_southNeighbor = nullptr;

public:
	enum {
		NO_DEBUG_LAYER,
		CONTINENT,
		EROSION,
		PEAK_AND_VALLEY,
		TEMPERATURE,
		HUMIDITY,
	};


	void DebugDrawBiome(int layerType) const;

	// Precalculate the safe index, and query!
	float GetContinentByIndex(int index) { return m_continent[index]; }
	float GetErosionByIndex(int index) { return m_erosion[index]; }
	float GetPeakAndValleyByIndex(int index) { return m_peaksValleys[index]; }
	float GetTemperatureByIndex(int index) { return m_temperature[index]; }
	float GetHumidityByIndex(int index) { return m_humidity[index]; }


private:
	std::array<float, CHUNK_SIZE_X* CHUNK_SIZE_Y> const& GetDebugLayer(int layerType) const;

private:
	std::array<float, CHUNK_SIZE_X* CHUNK_SIZE_Y> m_continent;
	std::array<float, CHUNK_SIZE_X* CHUNK_SIZE_Y> m_erosion;
	std::array<float, CHUNK_SIZE_X* CHUNK_SIZE_Y> m_peaksValleys;
	std::array<float, CHUNK_SIZE_X* CHUNK_SIZE_Y> m_temperature;
	std::array<float, CHUNK_SIZE_X* CHUNK_SIZE_Y> m_humidity;

private:
	void GenerateTrees(const PCGParams& params);
	bool IsLocalMaximum(int localX, int localY, float treeNoise, const PCGParams& params) const;
	bool CanPlaceTree(int surfaceZ, BiomeType biome, uint8_t surfaceBlockType) const;
	//TreeType SelectTreeType(BiomeType biome, Temperature temp, Humidity hum) const;
	TreeType SelectTreeType(BiomeType biome, Temperature temp, Humidity hum, int globalX, int globalY) const;


	void PlaceTree(int globalX, int globalY, int surfaceZ, const TreeStamp& stamp);
};



/*
float TerrainDensityCalculator::EvaluateHeightOffset(float C) const
{
	return m_params.heightOffsetSpline.GetValueAtInputKey(C);
}

float TerrainDensityCalculator::EvaluateSquashing(float C) const
{
	return m_params.squashingSpline.GetValueAtInputKey(C);
}

float TerrainDensityCalculator::EvaluateErosion(float E) const
{
	return m_params.erosionFactorSpline.GetValueAtInputKey(E);
}
*/