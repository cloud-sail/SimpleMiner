#pragma once
#include <stdint.h>

//-----------------------------------------------------------------------------------------------
// Block bit flag masks
//-----------------------------------------------------------------------------------------------
constexpr uint8_t BLOCK_BIT_MASK_IS_SKY				= 1;		// Can see sky, non-opaque and no opaque blocks are above
constexpr uint8_t BLOCK_BIT_MASK_IS_LIGHT_DIRTY		= 1 << 1;	// The block iterator is in the dirty light queue
constexpr uint8_t BLOCK_BIT_MASK_IS_FULL_OPAQUE		= 1 << 2;	// block light, visibility, hide neighbor faces
constexpr uint8_t BLOCK_BIT_MASK_IS_SOLID			= 1 << 3;	// Physical objects and physics raycast will hit
constexpr uint8_t BLOCK_BIT_MASK_IS_VISIBLE			= 1 << 4;

//-----------------------------------------------------------------------------------------------
// Light influence masks
//-----------------------------------------------------------------------------------------------
constexpr uint8_t LIGHT_OUTDOOR_MASK	= 0xF0; // High part: 11110000
constexpr uint8_t LIGHT_INDOOR_MASK		= 0x0F; // Low part:  00001111
constexpr uint8_t LIGHT_VALUE_MASK		= 0x0F;
constexpr int     LIGHT_OUTDOOR_SHIFT	= 4;
constexpr int     LIGHT_MAX_VALUE		= 15;	

// GLOWSTONE is solid, opaque, and emits 15 indoor light

//-----------------------------------------------------------------------------------------------
// An ultra-flyweight voxel, only store critical information on a per-block basis
//-----------------------------------------------------------------------------------------------
class Block
{
private:
	uint8_t m_type = 0;

	uint8_t m_lightInfluence = 0; // Outdoor light influence (4-bit) + Indoor light influence (4-bit)
	uint8_t m_bitFlags = 0;

public:
	void SetTypeID(uint8_t newType, bool updateBitFlags = true);
	uint8_t GetTypeID() const { return m_type; }

public:
	// Light influence accessors
	inline int GetOutdoorLightInfluence() const;
	inline int GetIndoorLightInfluence() const;
	inline void SetOutdoorLightInfluence(uint8_t lightLevel);
	inline void SetIndoorLightInfluence(uint8_t lightLevel);

	// Bitflag accessors
	inline bool IsSky() const;
	inline bool IsLightDirty() const;
	inline bool IsFullOpaque() const;
	inline bool IsSolid() const;
	inline bool IsVisible() const;

	inline void SetIsSky(bool isSky);
	inline void SetIsLightDirty(bool isLightDirty);
	inline void SetIsFullOpaque(bool isFullOpaque);
	inline void SetIsSolid(bool isSolid);
	inline void SetIsVisible(bool isVisible);

private:
	inline bool HasBitFlag(uint8_t bitMask) const;
	inline void SetBitFlag(uint8_t bitMask, bool value);



};


//-----------------------------------------------------------------------------------------------
// Light influence inline implementations
//-----------------------------------------------------------------------------------------------
inline int Block::GetOutdoorLightInfluence() const
{
	return (m_lightInfluence >> LIGHT_OUTDOOR_SHIFT) & LIGHT_VALUE_MASK;
	//return (m_lightInfluence & LIGHT_OUTDOOR_MASK) >> LIGHT_OUTDOOR_SHIFT;
}

//-----------------------------------------------------------------------------------------------
inline int Block::GetIndoorLightInfluence() const
{
	return m_lightInfluence & LIGHT_VALUE_MASK;
	//return m_lightInfluence & LIGHT_INDOOR_MASK;
}

//-----------------------------------------------------------------------------------------------
inline void Block::SetOutdoorLightInfluence(uint8_t lightLevel)
{
	// Clamp to valid range [0, 15]
	if (lightLevel > LIGHT_MAX_VALUE)
	{
		lightLevel = LIGHT_MAX_VALUE;
	}

	// Clear outdoor bits and set new value
	m_lightInfluence = (m_lightInfluence & (~LIGHT_OUTDOOR_MASK)) | (lightLevel << LIGHT_OUTDOOR_SHIFT);
}

//-----------------------------------------------------------------------------------------------
inline void Block::SetIndoorLightInfluence(uint8_t lightLevel)
{
	// Clamp to valid range [0, 15]
	if (lightLevel > LIGHT_MAX_VALUE)
	{
		lightLevel = LIGHT_MAX_VALUE;
	}

	// Clear indoor bits and set new value
	m_lightInfluence = (m_lightInfluence & (~LIGHT_INDOOR_MASK)) | lightLevel;
}

//-----------------------------------------------------------------------------------------------
// Bitflag inline implementations
//-----------------------------------------------------------------------------------------------
inline bool Block::HasBitFlag(uint8_t bitMask) const
{
	return (m_bitFlags & bitMask) != 0;
}

//-----------------------------------------------------------------------------------------------
inline void Block::SetBitFlag(uint8_t bitMask, bool value)
{
	if (value)
	{
		m_bitFlags |= bitMask;  // Set bit
	}
	else
	{
		m_bitFlags &= ~bitMask; // Clear bit
	}
}

//-----------------------------------------------------------------------------------------------
inline bool Block::IsSky() const
{
	return HasBitFlag(BLOCK_BIT_MASK_IS_SKY);
}

//-----------------------------------------------------------------------------------------------
inline bool Block::IsLightDirty() const
{
	return HasBitFlag(BLOCK_BIT_MASK_IS_LIGHT_DIRTY);
}

//-----------------------------------------------------------------------------------------------
inline bool Block::IsFullOpaque() const
{
	return HasBitFlag(BLOCK_BIT_MASK_IS_FULL_OPAQUE);
}

//-----------------------------------------------------------------------------------------------
inline bool Block::IsSolid() const
{
	return HasBitFlag(BLOCK_BIT_MASK_IS_SOLID);
}

//-----------------------------------------------------------------------------------------------
inline bool Block::IsVisible() const
{
	return HasBitFlag(BLOCK_BIT_MASK_IS_VISIBLE);
}

//-----------------------------------------------------------------------------------------------
inline void Block::SetIsSky(bool isSky)
{
	SetBitFlag(BLOCK_BIT_MASK_IS_SKY, isSky);
}

//-----------------------------------------------------------------------------------------------
inline void Block::SetIsLightDirty(bool isLightDirty)
{
	SetBitFlag(BLOCK_BIT_MASK_IS_LIGHT_DIRTY, isLightDirty);
}

//-----------------------------------------------------------------------------------------------
inline void Block::SetIsFullOpaque(bool isFullOpaque)
{
	SetBitFlag(BLOCK_BIT_MASK_IS_FULL_OPAQUE, isFullOpaque);
}

//-----------------------------------------------------------------------------------------------
inline void Block::SetIsSolid(bool isSolid)
{
	SetBitFlag(BLOCK_BIT_MASK_IS_SOLID, isSolid);
}

//-----------------------------------------------------------------------------------------------
inline void Block::SetIsVisible(bool isVisible)
{
	SetBitFlag(BLOCK_BIT_MASK_IS_VISIBLE, isVisible);
}
