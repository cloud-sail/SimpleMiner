#pragma once
#include "Engine/Core/XmlUtils.hpp"
#include "Engine/Math/AABB2.hpp"
#include <string>

class BlockDefinition
{
public:
	static void InitializeDefinitions(const char* path);
	static BlockDefinition const* GetByType(uint8_t type);
	static BlockDefinition const* GetByName(std::string const& defName);
	static uint8_t GetBlockTypeIDByName(std::string const& defName);
	static void ClearDefinitions();

	static std::vector<BlockDefinition*> s_definitions;

	static std::vector<uint8_t> s_lightInfluences;
	static int GetLightInfluenceByType(uint8_t type);

public:
	~BlockDefinition();
	bool LoadFromXmlElement(XmlElement const& element);

public:
	std::string m_name = "UNKNOWN";
	bool m_isVisible = false;
	bool m_isSolid = false;
	bool m_isOpaque = false;

	AABB2 m_topUVs = AABB2::ZERO_TO_ONE;
	AABB2 m_bottomUVs = AABB2::ZERO_TO_ONE;
	AABB2 m_sideUVs = AABB2::ZERO_TO_ONE;
};

