#pragma once
#include "Source/ThirdParty/Imgui/imgui.h"
#include "Source/Editor/Core/FEditor.h"
#include "Source/Runtime/Core/FStatRegistry.h"
#include <algorithm>
#include <string>

struct FImguiStatFps final
{
	// ImGui가 최근 60프레임 평균으로 계산한 FPS
	const float GetFPS() const
	{
		return ImGui::GetIO().Framerate;
	}
	const float GetFrameMs() const
	{
		const float Fps = GetFPS();
		return Fps > 0.f ? 1000.f / Fps : 0.f;
	}
	const void Process(FEditor& InEditor) const
	{
		const FVector2 TopLeft = InEditor.GetViewports()[0].TopLeftUV;
		const ImVec2 DisplaySize = ImGui::GetIO().DisplaySize;

		// 메뉴바에 가리지 않도록 작업 영역(WorkPos) 아래, 뷰포트 왼쪽 위에서 시작
		const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
		const float Margin = 10.f;
		const ImVec2 PosPixel(
			std::max(TopLeft.X * DisplaySize.x, MainViewport->WorkPos.x) + Margin,
			std::max(TopLeft.Y * DisplaySize.y, MainViewport->WorkPos.y) + Margin);

		ImFont* Font = ImGui::GetFont();
		const float FontSize = 21.f;
		const float RowMargin = 25.f;

		char FpsBuf[64];
		char ResolutionBuf[64];
		std::snprintf(FpsBuf, sizeof(FpsBuf), "FPS : %.0f (%.0f ms)", GetFPS(), GetFrameMs());
		std::snprintf(ResolutionBuf, sizeof(ResolutionBuf), "Resolution : %.0f x %.0f", DisplaySize.x, DisplaySize.y);

		// 가운데 줄은 FImguiStatPicking이 그린다
		ImDrawList* DrawList = ImGui::GetForegroundDrawList();
		DrawList->AddText(Font, FontSize, PosPixel, IM_COL32(75, 255, 75, 255), FpsBuf);
		DrawList->AddText(Font, FontSize, ImVec2(PosPixel.x, PosPixel.y + RowMargin * 2.f), IM_COL32(75, 255, 75, 255), ResolutionBuf);
	}
};
