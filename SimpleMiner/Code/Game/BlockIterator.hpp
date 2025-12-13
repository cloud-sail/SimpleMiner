#pragma once
#include "Game/ChunkUtils.hpp"

class Chunk;
class Block;

class BlockIterator
{
public:
	BlockIterator() = default;
	BlockIterator(Chunk* chunk, int blockIndex);

public:
	bool IsValid() const { return m_chunk != nullptr && m_blockIndex >= 0 && m_blockIndex < BLOCKS_PER_CHUNK; }

	Block* GetBlock() const;


	BlockIterator GetEastNeighbor() const;
	BlockIterator GetWestNeighbor() const;
	BlockIterator GetNorthNeighbor() const;
	BlockIterator GetSouthNeighbor() const;
	BlockIterator GetSkywardNeighbor() const;
	BlockIterator GetDownwardNeighbor() const;

	int GetIndoorLightInfluence() const;
	int GetOutdoorLightInfluence() const;

	void MarkChunkMeshDirty();
	void SetChunkNeedsSaving();

	//BlockIterator GetNeighbor(int dx, int dy, int dz) const; // #ToDo a full function iterator

public:
	Chunk* m_chunk = nullptr;
	int m_blockIndex = -1;

private:
	static constexpr int STRIDE_X = 1;
	static constexpr int STRIDE_Y = 1 << CHUNK_BITS_X;
	static constexpr int STRIDE_Z = 1 << (CHUNK_BITS_X + CHUNK_BITS_Y);
};

