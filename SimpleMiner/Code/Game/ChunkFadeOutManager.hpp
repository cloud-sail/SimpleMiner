#pragma once
#include "Engine/Math/AABB3.hpp"
#include <vector>

class VertexBuffer;
class IndexBuffer;
struct Frustum;
class Chunk;
class World;

class ChunkFadeOutManager
{
private:
	struct FadingChunk
	{
		VertexBuffer* m_vertexBuffer = nullptr;
		IndexBuffer* m_indexBuffer = nullptr;

		float m_elapsedSeconds = 0.f;
		AABB3 m_worldBounds;
		// No Transform Data


		FadingChunk(VertexBuffer* vb, IndexBuffer* ib, AABB3 const& bounds);
		~FadingChunk();

		FadingChunk(const FadingChunk&) = delete;
		FadingChunk& operator=(const FadingChunk&) = delete;

		FadingChunk(FadingChunk&& other) noexcept;
		FadingChunk& operator=(FadingChunk&& other) noexcept;
	};

	std::vector<FadingChunk> m_fadingChunks;

public:
	ChunkFadeOutManager(World* world);
	~ChunkFadeOutManager();

	ChunkFadeOutManager(const ChunkFadeOutManager&) = delete;
	ChunkFadeOutManager& operator=(const ChunkFadeOutManager&) = delete;

	void AddChunk(VertexBuffer*& vb, IndexBuffer*& ib, AABB3 const& bounds);
	void AddChunk(Chunk* chunk);

	void Update(float deltaSeconds);

	void Render(Frustum const& frustum) const;

private:
	World* m_world = nullptr;
};

