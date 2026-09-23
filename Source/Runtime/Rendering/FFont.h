#pragma once

#include "Runtime/Core/TMap.h"
#include "ThirdParty/Json/json.hpp"
#include "Runtime/Core/FString.h"
#include "Runtime/Core/PointerTypes.h"
#include "FTexture.h"

struct FCharacterInfo
{
	float u;
	float v;
	float width;
	float height;

	// 가변폭
	float advance;
	float planeLeft;
	float planeTop;
	float planeRight;
	float planeBottom;
};

class FFont
{
public:
	void InitializeForASCII(float InNumberOfLine);
	void Deserialize(const FWString& path);
	const FCharacterInfo& GetCharInfo(char32_t InCharacter) const;
	void SetTexture(const TSharedPtr<FTexture>& InName);
private:
	
	//FMeterial	// 폰트 머터리얼
	TSharedPtr<FTexture> Texture;
	TMap<char32_t, FCharacterInfo> CharInfoMap;
};