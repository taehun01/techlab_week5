#pragma once
#include "Source/ThirdParty/Imgui/imgui.h"
#include "Source/Editor/Core/FEditor.h"
#include "Source/Runtime/Core/FStatRegistry.h"
#include <string>

struct FImguiStatFps final
{
	float DeltaTime; // ms

	const void SetDeltaTime(const float InDeltaTime) // second
	{
		DeltaTime = InDeltaTime * 1000.f;
	}
	const float GetFPS() const
	{
		return 1000.f / DeltaTime;
	}
	const void Process(FEditor InEditor, const float InDeltaTime)
	{
		const FVector2 TopLeft = InEditor.GetViewports()[0].TopLeftUV;
		const FVector2 Length = InEditor.GetViewports()[0].LengthUV;

		FVector2 PosNDC = TopLeft;
		PosNDC.X += Length.X;
		PosNDC.Y += Length.Y * 0.2f;

		FVector2 PosPixel = PosNDC * STATS.GetWindowSize();
		PosPixel.X -= 100.f;

		ImFont* Font = ImGui::GetFont();
		const float FontSize = 21.f;
		const float RowMargin = 25.f;

		SetDeltaTime(InDeltaTime);
		char FpsBuf[16];
		char DeltaTimeBuf[16];
		std::snprintf(FpsBuf, sizeof(FpsBuf), "%.2f FPS", GetFPS());
		std::snprintf(DeltaTimeBuf, sizeof(DeltaTimeBuf), "%.2f ms", DeltaTime);

		ImDrawList* DrawList = ImGui::GetForegroundDrawList();
		DrawList->AddText(Font, FontSize, ImVec2(PosPixel.X, PosPixel.Y), IM_COL32(75, 255, 75, 255), FpsBuf);
		DrawList->AddText(Font, FontSize, ImVec2(PosPixel.X, PosPixel.Y + RowMargin), IM_COL32(75, 255, 75, 255), DeltaTimeBuf);
	}
};
