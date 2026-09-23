#pragma once

#include "FImguiStatFps.h"
#include "FImguiStatMemory.h"

struct FImguiOverlayStat
{
	FImguiStatFps StatFps;
	FImguiStatMemory StatMemory;

	const void Process(FEditor InEditor, const float InDeltaTime)
	{
		if (STATS.IsStatFps())
		{
			StatFps.Process(InEditor, InDeltaTime);
		}
		if (STATS.IsStatMemory())
		{
			StatMemory.Process(InEditor);
		}
	}
};