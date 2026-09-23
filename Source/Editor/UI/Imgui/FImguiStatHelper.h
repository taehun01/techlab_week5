#pragma once

#include "Source/Runtime/Core/FName.h"
#include "Source/Runtime/Core/TArray.h"
#include "Source/Runtime/Core/FStatRegistry.h"
#include <algorithm>

struct FStatHistory
{
private:
	uint32 Min;
	uint32 Max;
	bool bIsInitialized = false;
	uint32 WindowSecond = 5;
	uint32 TargetFPS = static_cast<uint32>(STATS.GetTargetFPS());
	uint32 WindowWidth = TargetFPS * WindowSecond;
	TArray<uint32> Window;
	uint32 WindowPtr = 0;
	uint32 TickCount = 0;
	uint32 Sum = 0;
public:
	uint32 Update(uint32 Value)
	{
		if (!bIsInitialized)
		{
			Min = Value;
			Max = Value;
			Window.resize(WindowWidth);
			bIsInitialized = true;
		}
		else
		{
			if (Value < Min) Min = Value;
			if (Value > Max) Max = Value;
		}
		WindowPtr %= WindowWidth;
		Sum -= Window[WindowPtr];
		Sum += Value;
		Window[WindowPtr] = Value;
		WindowPtr++;
		if (TickCount < WindowWidth)
		{
			++TickCount;
		}
		return Value;
	}
	float GetAverage() { return static_cast<float>(Sum) / TickCount; }
	uint32 GetMin() { return Min; }
	uint32 GetMax() { return Max; }
};

enum class EStatPool
{
	None = 0,
	Texture,
	Physical,
	Count
};

constexpr const char* ToString(EStatPool Pool)
{
	switch (Pool)
	{
	case EStatPool::None:
		return "None";
	case EStatPool::Texture:
		return "Texture";
	case EStatPool::Physical:
		return "Physical";
	default:
		return "Unknown";
	}
}

constexpr const uint32 ToCapacity(EStatPool Pool)
{
	switch (Pool)
	{
	case EStatPool::None:
		return 0;
	case EStatPool::Texture:
		return 1000u * 1024u * 1024u; //1000 MB
	case EStatPool::Physical:
		return 0;
	default:
		return 0;
	}
}

struct FMemoryStatRow
{
	FName RowName;
	EStatPool Pool;
	uint32 UsedByte;
	uint32* PeakByte = nullptr;
	FMemoryStatRow(FName InRowName, EStatPool InPool, uint32 InUsedByte, uint32* InPeakByte)
		:RowName(InRowName), Pool(InPool), UsedByte(InUsedByte), PeakByte(InPeakByte) {}
	uint32 UpdateUsedGetPeak(uint32 InUsedByte)
	{
		UsedByte = InUsedByte;
		if (UsedByte > *PeakByte)
		{
			*PeakByte = UsedByte;
		}
		return *PeakByte;
	}
};

struct FRenderCountStatRow
{
	FName RowName{};
    FStatHistory* History = nullptr;
	uint32 Count = 0;
};

struct FStaticCountStatRow
{
	FName RowName{};
	uint32 Count = 0;
};