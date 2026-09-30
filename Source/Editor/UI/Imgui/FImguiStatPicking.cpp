#include "FImguiStatPicking.h"
#include "Source/Runtime/Core/FStatRegistry.h"
#include <algorithm>

const void FImguiStatPicking::Process(FEditor& InEditor) const
{
	const FVector2 TopLeft = InEditor.GetViewports()[0].TopLeftUV;
	const ImVec2 DisplaySize = ImGui::GetIO().DisplaySize;

	// FImguiStatFps와 같은 기준점에서 FPS 줄 바로 아래에 그린다
	const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	const float Margin = 10.f;
	const ImVec2 PosPixel(
		std::max(TopLeft.X * DisplaySize.x, MainViewport->WorkPos.x) + Margin,
		std::max(TopLeft.Y * DisplaySize.y, MainViewport->WorkPos.y) + Margin);

	ImFont* Font = ImGui::GetFont();
	const float FontSize = 28.f;
	const float RowMargin = 34.f;

	char PickingBuf[128];
	std::snprintf(PickingBuf, sizeof(PickingBuf), "Picking Time %.3f ms : Num Attempts %d : Accumulated Time %.3f ms",
		STATS.GetPickingTime(), STATS.GetNumAttempts(), STATS.GetAccumulatedTime());

	ImDrawList* DrawList = ImGui::GetForegroundDrawList();
	DrawList->AddText(Font, FontSize, ImVec2(PosPixel.x, PosPixel.y + RowMargin), IM_COL32(0, 170, 0, 255), PickingBuf);
}
