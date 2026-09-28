#pragma once

#include "FImguiStatFps.h"
#include "FImguiStatMemory.h"
#include "FImguiStatPicking.h"

struct FImguiOverlayStat
{
	FImguiStatFps StatFps;
	FImguiStatMemory StatMemory;
	FImguiStatPicking StatPicking;

	//TODO: Editor를 복사로 받고있음
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

		if(STATS.IsStatPicking())
			StatPicking.Process(InEditor);
	}
};