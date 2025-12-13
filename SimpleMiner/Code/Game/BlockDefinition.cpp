#include "Game/BlockDefinition.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Core/NamedStrings.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Renderer/SpriteSheet.hpp"

std::vector<BlockDefinition*> BlockDefinition::s_definitions;

std::vector<uint8_t> BlockDefinition::s_lightInfluences;

int BlockDefinition::GetLightInfluenceByType(uint8_t type)
{
	return s_lightInfluences[type];
}

void BlockDefinition::InitializeDefinitions(const char* path /*= "Data/Definitions/BlockSpriteSheet_BlockDefinitions.xml"*/)
{
	ClearDefinitions();

	XmlDocument document;
	XmlResult result = document.LoadFile(path);
	GUARANTEE_OR_DIE(result == tinyxml2::XML_SUCCESS, Stringf("Failed to open xml file: \"%s\"", path));

	XmlElement* rootElement = document.RootElement();
	GUARANTEE_OR_DIE(rootElement, Stringf("No elements in xml file: \"%s\"", path));

	XmlElement* defElement = rootElement->FirstChildElement();

	while (defElement != nullptr)
	{
		std::string elementName = defElement->Name();
		GUARANTEE_OR_DIE(elementName == "BlockDefinition", Stringf("Root child element in %s was <%s>, must be <BlockDefinition>!", path, elementName.c_str()));
		BlockDefinition* newDef = new BlockDefinition();

		newDef->LoadFromXmlElement(*defElement);

		s_definitions.push_back(newDef);


		int indoorLighting = ParseXmlAttribute(*defElement, "indoorLighting", 0);
		s_lightInfluences.push_back(static_cast<uint8_t>(indoorLighting));


		defElement = defElement->NextSiblingElement();
	}

}

BlockDefinition const* BlockDefinition::GetByType(uint8_t type)
{
	GUARANTEE_OR_DIE(type < s_definitions.size(), "Block type is out of range.");
	return s_definitions[type];
}

BlockDefinition const* BlockDefinition::GetByName(std::string const& defName)
{
	for (int i = 0; i < (int)s_definitions.size(); ++i)
	{
		if (s_definitions[i]->m_name == defName)
		{
			return s_definitions[i];
		}
	}

	ERROR_AND_DIE(Stringf("Block is not in Block Definitions: \"%s\"", defName.c_str()));
}

uint8_t BlockDefinition::GetBlockTypeIDByName(std::string const& defName)
{
	for (int i = 0; i < (int)s_definitions.size(); ++i)
	{
		if (s_definitions[i]->m_name == defName)
		{
			return static_cast<uint8_t>(i);
		}
	}

	ERROR_AND_DIE(Stringf("Block is not in Block Definitions: \"%s\"", defName.c_str()));
}

void BlockDefinition::ClearDefinitions()
{
	for (int i = 0; i < (int)s_definitions.size(); ++i)
	{
		delete s_definitions[i];
	}
	s_definitions.clear();
	s_lightInfluences.clear();
}

BlockDefinition::~BlockDefinition()
{

}

bool BlockDefinition::LoadFromXmlElement(XmlElement const& element)
{
	m_name		= ParseXmlAttribute(element, "name", m_name);
	m_isVisible = ParseXmlAttribute(element, "isVisible", m_isVisible);
	m_isSolid	= ParseXmlAttribute(element, "isSolid", m_isSolid);
	m_isOpaque	= ParseXmlAttribute(element, "isOpaque", m_isOpaque);

	IntVec2 topSpriteCoords = ParseXmlAttribute(element, "topSpriteCoords", IntVec2());
	IntVec2 bottomSpriteCoords = ParseXmlAttribute(element, "bottomSpriteCoords", IntVec2());
	IntVec2 sideSpriteCoords = ParseXmlAttribute(element, "sideSpriteCoords", IntVec2());

	const IntVec2 dimensions = g_gameConfigBlackboard.GetValue("blockSpriteSheetCellCount", IntVec2(8, 8));

	m_topUVs	= g_blockSpriteSheet->GetSpriteUVs(topSpriteCoords.x + topSpriteCoords.y * dimensions.x);
	m_bottomUVs = g_blockSpriteSheet->GetSpriteUVs(bottomSpriteCoords.x + bottomSpriteCoords.y * dimensions.x);
	m_sideUVs	= g_blockSpriteSheet->GetSpriteUVs(sideSpriteCoords.x + sideSpriteCoords.y * dimensions.x);

	return true;
}
