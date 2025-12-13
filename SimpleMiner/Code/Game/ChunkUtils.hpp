#pragma once

#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/IntVec3.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include <cmath>
#include <vector>

/********************************************
SPACES AND TRANSFORMS
1.	World space is defined as (absolute):  +X east, +Y north, +Z skyward.
2.	Local space for actors, objects, and game/world cameras is defined as (relative):  +I relative-forward, +J relative-left, +K relative-up.  Actors with an Identity orientation therefore face forward toward +X east, left toward +Y north, and up toward +Z skyward.
3.	Euler angles (Tait-Bryan angles) are therefore defined in SimpleMiner as:
a.	Yaw is the initial (major) positive rotation about absolute +Z (from +X toward +Y).  It is appended first, and is therefore relative only to the world (absolute) axes.
b.	Pitch is a positive rotation about relative +J left (from +K toward +I) appended after Yaw.
c.	Roll is a positive rotation about relative +I forward (from +J toward +K) appended after Yaw and Pitch.
4.	World units: Each world unit is 1 meter.  Each block is 1.0 x 1.0 x 1.0 world units (meters) in size.  This assumption (that blocks are each 1m x 1m x 1m) is fundamental and should be hard-coded (for purposes of speed and numerical precision).
5.	World positions: 3D, Vec3 (float x,y,z), free-floating positions in world space.  The world’s bounds extend infinitely in +/- X (east/west) and +/- Y (north/south) directions, but are finite vertically (from Z=0.0 at world bottom to Z=128.0 at world top, if chunks are 128 blocks tall).
6.	Chunk coordinates: 2D, IntVec2 (int x,y), with x and y axes aligned with world axes (above).  Adjacent chunks have adjacent chunk coordinates; for example, chunk (4,7) is the immediate eastern neighbor of chunk (3,7), and chunk (3,7)’s easternmost edge lines up exactly with chunk (4,7)’s westernmost edge.  There is no chunk Z coordinate since each chunk extends fully from the bottom of the world to the top of the world (i.e. Chunks do not stack vertically).
7.	Local block coordinates: 3D, IntVec3 (int x,y,z), with x,y,z axes aligned with world axes.  The block-grid coordinates of any given block within a chunk, such that the block at local block coordinates (0,0,0) is at the west-south-bottom of any given chunk.  If chunks are 16x16x128 blocks, the block coordinates of the east-north-top block are (15,15,127).
8.	Global block coordinates: 3D, IntVec3 (int x,y,z), with x,y,z axes aligned with world axes.  Global block (0,0,0) has its mins at world position (0.0,0.0,0.0) and its maxs at world (1.0,1.0,1.0).  Unlike local block coordinates, global block coordinates continue counting up as you move east/north, and go negative west/south of the world origin.  Because each chunk covers the entire world vertically, any block’s global block z coordinate is coincidentally equal to its local block z.
9.	Local block index: integer (int32_t) index into the (1D) array of blocks owned by the chunk (see below).  Block 0 is the west-south-bottom block, with indexes counting up in the +X (east) direction, then in the +Y (north) direction, then lastly in the +Z (up) direction.  Therefore, for a 16x16x128 chunk there are 32768 blocks, with the following notable block indexes:
	a.	The east-south-bottom block is #15.
	b.	The west-north-bottom block is #240.
	c.	The east-north-bottom block is #255.
	d.	The east-north-top block is #32767, which is the last block / highest index in each chunk.

> I am creating a Minecraft-like voxel engine in C++. My world is divided into chunks, and blocks within those chunks. Each chunk is a rectangular region aligned to the world axes, for example 16x16x128 blocks in size.
>
> Please write a `ChunkUtils.hpp` header file that provides efficient, clear, and industry-standard inline utility functions for coordinate space conversions:
>
> - Block local coordinates <-> block linear index within a chunk
> - Global block coordinates <-> chunk coordinates and chunk-local block coordinates
> - World position (float) <-> global block coordinates, and vice versa
> - World position (float) <-> chunk coordinates, and vice versa
> - Chunk-local coordinate wrapping and validation
> - get chunk AABB (axis-aligned bounding box) in world space
>
> Requirements:
>
> - Use clear, precise, and concise camelCase naming that avoids ambiguity and follows modern C++ best practices.
> - Use English comments for each function, explaining its purpose.
> - Assume types `Vec2`, `Vec3`, `IntVec2`, and `IntVec3` are already implemented and can be included.
> - Make sure the code is ready to use as a utility header in a professional C++ voxel/world engine.
>
> Please output only the code, with all inline functions and constants in a single, cohesive header.
*/

// ======= Chunk Size and Bitmask Constants =======

constexpr int CHUNK_BITS_X = 5;
constexpr int CHUNK_BITS_Y = 5;
constexpr int CHUNK_BITS_Z = 7;
static_assert(CHUNK_BITS_X + CHUNK_BITS_Y + CHUNK_BITS_Z < 32,
	"The sum of CHUNK_BITS_X, CHUNK_BITS_Y, and CHUNK_BITS_Z must be less than 32");

constexpr int CHUNK_SIZE_X = 1 << CHUNK_BITS_X;
constexpr int CHUNK_SIZE_Y = 1 << CHUNK_BITS_Y;
constexpr int CHUNK_SIZE_Z = 1 << CHUNK_BITS_Z;

constexpr int CHUNK_MAX_X = CHUNK_SIZE_X - 1;
constexpr int CHUNK_MAX_Y = CHUNK_SIZE_Y - 1;
constexpr int CHUNK_MAX_Z = CHUNK_SIZE_Z - 1;

constexpr int CHUNK_MASK_X = CHUNK_MAX_X;
constexpr int CHUNK_MASK_Y = CHUNK_MAX_Y << CHUNK_BITS_X;
constexpr int CHUNK_MASK_Z = CHUNK_MAX_Z << (CHUNK_BITS_X + CHUNK_BITS_Y);


constexpr int BLOCKS_PER_CHUNK = CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z;

// Chunk Activation/Deactivation
constexpr int CHUNK_ACTIVATION_RANGE = 320;
constexpr int CHUNK_DEACTIVATION_RANGE = CHUNK_ACTIVATION_RANGE + CHUNK_SIZE_X + CHUNK_SIZE_Y;

constexpr int CHUNK_ACTIVATION_RADIUS_X = 1 + (CHUNK_ACTIVATION_RANGE / CHUNK_SIZE_X);
constexpr int CHUNK_ACTIVATION_RADIUS_Y = 1 + (CHUNK_ACTIVATION_RANGE / CHUNK_SIZE_Y);
constexpr int MAX_ACTIVE_CHUNKS = (2 * CHUNK_ACTIVATION_RADIUS_X) * (2 * CHUNK_ACTIVATION_RADIUS_Y);

constexpr int MAX_GENERATING_CKUNKS = 256;
constexpr int MAX_LOADING_CHUNKS = 8;
constexpr int MAX_SAVING_CHUNKS = 8;

constexpr int MAX_CHUNK_MESHES_BUILT_PER_FRAME = 2;
constexpr int MAX_CHUNKS_DEACTIVATED_PER_FRAME = 256;


// Contains sorted offsets, for fast iteration, still needs IsWithinActivationRange checks
extern const std::vector<IntVec2> g_chunkActivationOffsets;



// ======= 1. Local Block Index <-> Local Coordinates =======

// Convert local block coordinates (within chunk) to a linear array index
inline int GetBlockIndexInChunk(int lx, int ly, int lz) 
{
	return lx + (ly << CHUNK_BITS_X) + (lz << (CHUNK_BITS_X + CHUNK_BITS_Y));
}

inline int GetBlockIndexInChunk(const IntVec3& local) 
{
	return GetBlockIndexInChunk(local.x, local.y, local.z);
}

// Convert a chunk-local linear index to local block coordinates
inline IntVec3 GetBlockLocalCoordsFromIndex(int index) 
{
	return IntVec3{
		index & CHUNK_MAX_X,
		(index >> CHUNK_BITS_X) & CHUNK_MAX_Y,
		(index >> (CHUNK_BITS_X + CHUNK_BITS_Y)) & CHUNK_MAX_Z
	};
}

inline int GetBlockLocalXFromIndex(int index)
{
	return index & CHUNK_MAX_X;
}

inline int GetBlockLocalYFromIndex(int index)
{
	return (index >> CHUNK_BITS_X) & CHUNK_MAX_Y;
}

inline int GetBlockLocalZFromIndex(int index)
{
	return (index >> (CHUNK_BITS_X + CHUNK_BITS_Y)) & CHUNK_MAX_Z;
}

// ======= 2. Global Block Coordinates <-> Chunk Coordinates + Local Block =======

// Convert global block coordinates to chunk coordinates
inline IntVec2 GetChunkCoordsFromBlockGlobal(const IntVec3& global) 
{
	return IntVec2(global.x >> CHUNK_BITS_X, global.y >> CHUNK_BITS_Y);
}

// Convert global block coordinates to local chunk block coordinates
inline IntVec3 GetBlockLocalCoordsFromGlobal(const IntVec3& global) 
{
	// Notes: global.z & CHUNK_MAX_Z is wrong in simple miner
	return IntVec3(global.x & CHUNK_MAX_X, global.y & CHUNK_MAX_Y, global.z);
}

// Convert chunk coordinates and local block coordinates to global block coordinates
inline IntVec3 GetBlockGlobalCoordsFromChunk(const IntVec2& chunk, const IntVec3& local) 
{
	return IntVec3(
		(chunk.x << CHUNK_BITS_X) + local.x,
		(chunk.y << CHUNK_BITS_Y) + local.y,
		local.z
	);
}

// ======= 3. World Position <-> Global Block Coordinates =======

// Convert world position (float) to global block coordinates (floor/truncate)
inline IntVec3 GetBlockGlobalCoordsFromWorld(const Vec3& world) 
{
	return IntVec3(
		static_cast<int>(floorf(world.x)),
		static_cast<int>(floorf(world.y)),
		static_cast<int>(floorf(world.z))
	);
}

// Convert global block coordinates to the world position of the block's minimum (corner)
inline Vec3 GetWorldCoordsFromBlockGlobal(const IntVec3& global) {
	return Vec3(
		static_cast<float>(global.x),
		static_cast<float>(global.y),
		static_cast<float>(global.z)
	);
}

// ======= 4. World Position <-> Chunk Coordinates =======

// Convert world position (float) to chunk coordinates
inline IntVec2 GetChunkCoordsFromWorld(const Vec2& world) 
{
	return IntVec2(
		static_cast<int>(floorf(world.x)) >> CHUNK_BITS_X,
		static_cast<int>(floorf(world.y)) >> CHUNK_BITS_Y
	);
}

// Get the world position (corner) of the chunk's minimum from chunk coordinates
inline Vec2 GetWorldCoordsFromChunk(const IntVec2& chunk) {
	return Vec2(
		static_cast<float>(chunk.x * CHUNK_SIZE_X),
		static_cast<float>(chunk.y * CHUNK_SIZE_Y)
	);
}

// ======= 5. Validation & Utility =======

// Check whether local block coordinates are valid within a chunk
inline bool IsBlockLocalCoordsValid(const IntVec3& local) 
{
	return local.x >= 0 && local.x < CHUNK_SIZE_X &&
		local.y >= 0 && local.y < CHUNK_SIZE_Y &&
		local.z >= 0 && local.z < CHUNK_SIZE_Z;
}

// Check if a given z is valid for blocks (vertical range)
inline bool IsBlockZValid(int z) 
{
	return z >= 0 && z < CHUNK_SIZE_Z;
}

// Align global block coordinates to the chunk's starting block (minimum corner)
inline IntVec3 GetChunkStartBlockGlobal(const IntVec3& global) 
{
	return IntVec3(
		(global.x >> CHUNK_BITS_X) << CHUNK_BITS_X,
		(global.y >> CHUNK_BITS_Y) << CHUNK_BITS_Y,
		0
	);
}

// ======= 6. Chunk-Local Coordinate Wrapping =======

// Fast modulus for chunk-local coordinates
inline int WrapLocalX(int x) { return x & CHUNK_MAX_X; }
inline int WrapLocalY(int y) { return y & CHUNK_MAX_Y; }
inline int WrapLocalZ(int z) { return z & CHUNK_MAX_Z; }

inline bool IsLocalXValid(int x) { return x >= 0 && x < CHUNK_SIZE_X; }
inline bool IsLocalYValid(int y) { return y >= 0 && y < CHUNK_SIZE_Y; }
inline bool IsLocalZValid(int z) { return z >= 0 && z < CHUNK_SIZE_Z; }

inline bool IsBlockLocalPosValid(int x, int y, int z) 
{
	return IsLocalXValid(x) && IsLocalYValid(y) && IsLocalZValid(z);
}
inline bool IsBlockLocalPosValid(const IntVec3& local) 
{
	return IsBlockLocalPosValid(local.x, local.y, local.z);
}

// ======= 7. Chunk AABB in World Space =======

// Get the AABB (axis-aligned bounding box) of a chunk in world coordinates
inline AABB3 GetChunkAABBWorld(const IntVec2& chunk) {
	Vec3 mins(
		static_cast<float>(chunk.x * CHUNK_SIZE_X),
		static_cast<float>(chunk.y * CHUNK_SIZE_Y),
		0.0f
	);
	Vec3 maxs(
		static_cast<float>((chunk.x + 1) * CHUNK_SIZE_X),
		static_cast<float>((chunk.y + 1) * CHUNK_SIZE_Y),
		static_cast<float>(CHUNK_SIZE_Z)
	);
	return AABB3(mins, maxs);
}

inline Vec2 GetChunkCenter(const IntVec2& chunkCoords)
{
	return Vec2(static_cast<float>(chunkCoords.x * CHUNK_SIZE_X) + static_cast<float>((chunkCoords.x + 1) * CHUNK_SIZE_X),
		static_cast<float>(chunkCoords.y * CHUNK_SIZE_Y) + static_cast<float>((chunkCoords.y + 1) * CHUNK_SIZE_Y)) * 0.5f;
}
