#pragma once

#include "FImguiStatFps.h"
#include "FImguiStatMemory.h"
#include "FImguiStatPicking.h"

struct FImguiOverlayStat
{
	FImguiStatFps StatFps;
	FImguiStatMemory StatMemory;
	FImguiStatPicking StatPicking;

	const void Process(FEditor& InEditor)
	{
		if (STATS.IsStatFps())
		{
			StatFps.Process(InEditor);
		}
		if (STATS.IsStatMemory())
		{
			StatMemory.Process(InEditor);
		}

		if(STATS.IsStatPicking())
			StatPicking.Process(InEditor);
	}
};
