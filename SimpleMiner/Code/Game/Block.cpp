#include "Game/Block.hpp"
#include "Game/BlockDefinition.hpp"

void Block::SetTypeID(uint8_t newType, bool updateBitFlags /*= true*/)
{
	m_type = newType;

	if (updateBitFlags)
	{
		BlockDefinition const* def = BlockDefinition::GetByType(m_type);

		SetIsSolid(def->m_isSolid);
		SetIsFullOpaque(def->m_isOpaque);
		SetIsVisible(def->m_isVisible);
	}
}
