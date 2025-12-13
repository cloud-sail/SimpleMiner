#include "Game/BlockIterator.hpp"
#include "Game/Chunk.hpp"

BlockIterator::BlockIterator(Chunk* chunk, int blockIndex)
	: m_chunk(chunk)
	, m_blockIndex(blockIndex)
{

}

Block* BlockIterator::GetBlock() const
{
	if (!IsValid()) return nullptr;

	return &(m_chunk->m_blocks[m_blockIndex]);
}

BlockIterator BlockIterator::GetEastNeighbor() const
{
	if (!IsValid()) return BlockIterator();

	int localX = GetBlockLocalXFromIndex(m_blockIndex);

	if (localX == CHUNK_MAX_X)
	{
		return BlockIterator(m_chunk->GetEastNeighbor(), m_blockIndex & (~CHUNK_MASK_X));
	}
	else
	{
		return BlockIterator(m_chunk, m_blockIndex + STRIDE_X);
	}

}

BlockIterator BlockIterator::GetWestNeighbor() const
{
	if (!IsValid()) return BlockIterator();

	int localX = GetBlockLocalXFromIndex(m_blockIndex);

	if (localX == 0)
	{
		return BlockIterator(m_chunk->GetWestNeighbor(), m_blockIndex | CHUNK_MASK_X);
	}
	else
	{
		return BlockIterator(m_chunk, m_blockIndex - STRIDE_X);
	}
}

BlockIterator BlockIterator::GetNorthNeighbor() const
{
	if (!IsValid()) return BlockIterator();

	int localY = GetBlockLocalYFromIndex(m_blockIndex);

	if (localY == CHUNK_MAX_Y)
	{
		return BlockIterator(m_chunk->GetNorthNeighbor(), m_blockIndex & (~CHUNK_MASK_Y));
	}
	else
	{
		return BlockIterator(m_chunk, m_blockIndex + STRIDE_Y);
	}
}

BlockIterator BlockIterator::GetSouthNeighbor() const
{
	if (!IsValid()) return BlockIterator();

	int localY = GetBlockLocalYFromIndex(m_blockIndex);

	if (localY == 0)
	{
		return BlockIterator(m_chunk->GetSouthNeighbor(), m_blockIndex | CHUNK_MASK_Y);
	}
	else
	{
		return BlockIterator(m_chunk, m_blockIndex - STRIDE_Y);
	}
}

BlockIterator BlockIterator::GetSkywardNeighbor() const
{
	if (!IsValid()) return BlockIterator();

	int localZ = GetBlockLocalZFromIndex(m_blockIndex);

	if (localZ == CHUNK_MAX_Z)
	{
		return BlockIterator(); // Invalid
	}
	else
	{
		return BlockIterator(m_chunk, m_blockIndex + STRIDE_Z);
	}
}

BlockIterator BlockIterator::GetDownwardNeighbor() const
{
	if (!IsValid()) return BlockIterator();

	int localZ = GetBlockLocalZFromIndex(m_blockIndex);

	if (localZ == 0)
	{
		return BlockIterator(); // Invalid
	}
	else
	{
		return BlockIterator(m_chunk, m_blockIndex - STRIDE_Z);
	}
}

int BlockIterator::GetIndoorLightInfluence() const
{
	if (!IsValid()) return 0;

	return m_chunk->m_blocks[m_blockIndex].GetIndoorLightInfluence();
}

int BlockIterator::GetOutdoorLightInfluence() const
{
	if (!IsValid()) return 0;

	return m_chunk->m_blocks[m_blockIndex].GetOutdoorLightInfluence();
}

void BlockIterator::MarkChunkMeshDirty()
{
	if (!IsValid()) return;

	m_chunk->MarkMeshDirty();
}

void BlockIterator::SetChunkNeedsSaving()
{
	if (!IsValid()) return;

	m_chunk->SetNeedsSaving();
}
