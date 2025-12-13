#include "Game/ChunkFadeOutManager.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Game.hpp"
#include "Game/Chunk.hpp"
#include "Game/World.hpp"
#include "Game/ChunkUtils.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/Renderer/IndexBuffer.hpp"
#include "Engine/Math/MathUtils.hpp"


ChunkFadeOutManager::FadingChunk::FadingChunk(FadingChunk&& other) noexcept
	: m_vertexBuffer(other.m_vertexBuffer)
	, m_indexBuffer(other.m_indexBuffer)
	, m_elapsedSeconds(other.m_elapsedSeconds)
	, m_worldBounds(other.m_worldBounds)
{
	other.m_vertexBuffer = nullptr;
	other.m_indexBuffer = nullptr;
}

ChunkFadeOutManager::FadingChunk::FadingChunk(VertexBuffer* vb, IndexBuffer* ib, AABB3 const& bounds)
	: m_vertexBuffer(vb)
	, m_indexBuffer(ib)
	, m_elapsedSeconds(0.f)
	, m_worldBounds(bounds)
{

}

ChunkFadeOutManager::FadingChunk& ChunkFadeOutManager::FadingChunk::operator=(FadingChunk&& other) noexcept
{
	// Self assignment check
	if (this != &other) 
	{
		// release current resource
		delete m_vertexBuffer;
		delete m_indexBuffer;

		// transfer ownership
		m_vertexBuffer = other.m_vertexBuffer;
		m_indexBuffer = other.m_indexBuffer;
		m_elapsedSeconds = other.m_elapsedSeconds;
		m_worldBounds = other.m_worldBounds;

		// clear origin object
		other.m_vertexBuffer = nullptr;
		other.m_indexBuffer = nullptr;		
	}
	return *this;
}

ChunkFadeOutManager::FadingChunk::~FadingChunk()
{
	delete m_vertexBuffer;
	m_vertexBuffer = nullptr;

	delete m_indexBuffer;
	m_indexBuffer = nullptr;
}

ChunkFadeOutManager::ChunkFadeOutManager(World* world)
	: m_world(world)
{
	m_fadingChunks.reserve(MAX_CHUNKS_DEACTIVATED_PER_FRAME);
}

ChunkFadeOutManager::~ChunkFadeOutManager()
{
	m_fadingChunks.clear();
}

void ChunkFadeOutManager::AddChunk(VertexBuffer*& vb, IndexBuffer*& ib, AABB3 const& bounds)
{
	if (vb == nullptr || ib == nullptr) 
	{
		return;
	}

	m_fadingChunks.emplace_back(vb, ib, bounds);

	vb = nullptr;
	ib = nullptr;
}

void ChunkFadeOutManager::AddChunk(Chunk* chunk)
{
	AddChunk(chunk->m_vertexBuffer, chunk->m_indexBuffer, chunk->m_worldBounds);
}

void ChunkFadeOutManager::Update(float deltaSeconds)
{
	// Normal method
	//for (auto it = m_fadingChunks.begin(); it != m_fadingChunks.end();) 
	//{
	//	it->m_elapsedSeconds += deltaSeconds;

	//	if (it->m_elapsedSeconds >= FADE_OUT_SECONDS) 
	//	{
	//		it = m_fadingChunks.erase(it);
	//	}
	//	else 
	//	{
	//		++it;
	//	}
	//}

	// Better method
	for (size_t i = 0; i < m_fadingChunks.size();) 
	{
		m_fadingChunks[i].m_elapsedSeconds += deltaSeconds;

		if (m_fadingChunks[i].m_elapsedSeconds >= FADE_OUT_SECONDS) 
		{
			// swap-and-pop
			if (i != m_fadingChunks.size() - 1) 
			{
				m_fadingChunks[i] = std::move(m_fadingChunks.back());
			}
			m_fadingChunks.pop_back();
		}
		else 
		{
			++i;
		}
	}
}

void ChunkFadeOutManager::Render(Frustum const& frustum) const
{
	for (auto& chunk : m_fadingChunks) 
	{

		if (chunk.m_vertexBuffer != nullptr && chunk.m_indexBuffer != nullptr)
		{
			if (!IsAABBOnFrustum(chunk.m_worldBounds, frustum)) continue;

			DitherFadeRenderResources resources;
			resources.diffuseTextureIndex = g_theRenderer->GetSrvIndexFromLoadedTexture(g_blockTexture, DefaultTexture::WhiteOpaque2D);
			resources.diffuseSamplerIndex = g_theRenderer->GetDefaultSamplerIndex(SamplerMode::POINT_CLAMP);
			resources.cameraConstantsIndex = g_theRenderer->GetCurrentCameraConstantsIndex();
			resources.modelConstantsIndex = g_theRenderer->GetCurrentModelConstantsIndex();

			float t = GetClampedZeroToOne(1.0f - (chunk.m_elapsedSeconds / FADE_OUT_SECONDS));
			resources.fadeAmount = SmoothEnd3(t);

			resources.worldConstantsIndex = m_world->GetWorldConstantsIndex();
			resources.skyQuadSRVIndex = m_world->GetSkyQuadSRVIndex();

			g_theRenderer->SetGraphicsBindlessResources(sizeof(DitherFadeRenderResources), &resources);

			g_theRenderer->BindShader(g_theGame->m_ditherShader);
			g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
			g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
			g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
			//g_theRenderer->SetRenderTargetFormats();

			g_theRenderer->DrawIndexedVertexBuffer(chunk.m_vertexBuffer, chunk.m_indexBuffer, chunk.m_indexBuffer->GetCount());
		}

	}
}
