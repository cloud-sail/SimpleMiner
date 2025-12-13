#include "Game/ChunkUtils.hpp"

#include <algorithm>

static std::vector<IntVec2> GenerateChunkActivationOffsets()
{
	std::vector<std::pair<int, IntVec2>> candidates;

	for (int dy = -CHUNK_ACTIVATION_RADIUS_Y; dy <= CHUNK_ACTIVATION_RADIUS_Y; ++dy)
	{
		for (int dx = -CHUNK_ACTIVATION_RADIUS_X; dx <= CHUNK_ACTIVATION_RADIUS_X; ++dx)
		{
			IntVec2 offset(dx, dy);
			int distanceSquared = offset.GetLengthSquared();
			candidates.push_back({ distanceSquared, offset });
		}
	}

	std::sort(candidates.begin(), candidates.end(),
		[](const auto& a, const auto& b) {
			return a.first < b.first;
		});

	std::vector<IntVec2> offsets;
	offsets.reserve(candidates.size());
	for (const auto& [distSq, offset] : candidates)
	{
		offsets.push_back(offset);
	}

	return offsets;
}

const std::vector<IntVec2> g_chunkActivationOffsets = GenerateChunkActivationOffsets();

