#include "FFont.h"
#include "Runtime/Core/IntTypes.h"
#include "Runtime/Core/FString.h"
#include <filesystem>
#include <fstream>

void FFont::InitializeForASCII(float InNumberOfLine)
{
	// 16x16 코드페이지 437 기준
	float uvSize = 1.0f / InNumberOfLine;
	for (uint16 i = 0; i < 256; ++i)
	{
		uint16 col = i % 16;
		uint16 row = i / 16;

		FCharacterInfo ci;
		ci.u = col * uvSize;
		ci.v = row * uvSize;
		ci.width = uvSize;
		ci.height = uvSize;

		CharInfoMap[static_cast<char>(i)] = ci;
	}
}

void FFont::Deserialize(const FWString& path)
{
	std::ifstream f(path);
	if (!f)
	{
		return;
	}

	std::stringstream buffer;
	buffer << f.rdbuf();

	json::JSON data = json::JSON::Load(buffer.str());

	// atlas 자체 정보 
	FString type = data["atlas"]["type"].ToString();
	uint32 distanceRange = data["atlas"]["distanceRange"].ToInt();
	uint32 dixtanceRangeMiddle = data["atlas"]["distanceRangeMiddle"].ToInt();
	uint32 size = data["atlas"]["size"].ToInt();	
	uint32 width = data["atlas"]["width"].ToInt();
	uint32 height = data["atlas"]["height"].ToInt();
	FString yOrigin = data["atlas"]["yOrigin"].ToString();

	// metrics
	uint32 emSize = data["metrics"]["emSize"].ToInt();
	float lineHeight = data["metrics"]["lineHeight"].ToFloat();
	float ascender = data["metrics"]["ascender"].ToFloat();
	float descender = data["metrics"]["descender"].ToFloat();
	float underlineY = data["metrics"]["underlineY"].ToFloat();
	float underlineThickness = data["metrics"]["underlineThickness"].ToFloat();

	// 문자
	for (auto& glyph : data["glyphs"].ArrayRange())
	{
		FCharacterInfo info{};

		uint32 unicode = glyph["unicode"].ToInt();
		info.advance = glyph["advance"].ToFloat();

		if (glyph.hasKey("planeBounds"))
		{
			info.planeLeft = glyph["planeBounds"]["left"].ToFloat();
			info.planeTop = glyph["planeBounds"]["top"].ToFloat();
			info.planeRight = glyph["planeBounds"]["right"].ToFloat();
			info.planeBottom = glyph["planeBounds"]["bottom"].ToFloat();
		}

		if (glyph.hasKey("atlasBounds"))
		{
			float atlLeft = glyph["atlasBounds"]["left"].ToFloat();
			float atlTop = glyph["atlasBounds"]["top"].ToFloat();
			float atlRight = glyph["atlasBounds"]["right"].ToFloat();
			float atlBot = glyph["atlasBounds"]["bottom"].ToFloat();


			info.u = atlLeft / width;
			info.v = atlTop / height;
			info.width = (atlRight - atlLeft) / width;
			info.height = (atlBot - atlTop) / height;
		}

		CharInfoMap.emplace(static_cast<char32_t>(unicode), info);
	}
}

const FCharacterInfo& FFont::GetCharInfo(char32_t InCharacter) const
{
	auto it = CharInfoMap.find(InCharacter);
	if (it != CharInfoMap.end())
	{
		return it->second;
	}

	auto fallbackIt = CharInfoMap.find('?');
	if (fallbackIt != CharInfoMap.end())
	{
		return fallbackIt->second;
	}
		
	static const FCharacterInfo defaultInfo{};
	return defaultInfo;
}

void FFont::SetTexture(const TSharedPtr<FTexture>& InTexture)
{
	Texture = InTexture;
}
