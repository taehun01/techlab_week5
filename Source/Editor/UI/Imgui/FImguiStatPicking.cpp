#include "FImguiStatPicking.h"
#include "Source/Runtime/Core/FStatRegistry.h"

const void FImguiStatPicking::Process(FEditor& InEditor) const
{
	const FVector2 TopLeft = InEditor.GetViewports()[0].TopLeftUV;
	const FVector2 Length = InEditor.GetViewports()[0].LengthUV;

	FVector2 PosNDC = TopLeft;
	PosNDC.X += Length.X;
	PosNDC.Y += Length.Y * 0.2f;

	const ImVec2 DisplaySize = ImGui::GetIO().DisplaySize;
	FVector2 PosPixel = PosNDC * FVector2(DisplaySize.x, DisplaySize.y);
	PosPixel.X -= 100.f;

	ImFont* Font = ImGui::GetFont();
	const float FontSize = 21.f;
	const float RowMargin = 25.f;

	// FPS 두 줄(FPS, ms) 아래에 한 줄 띄우고 시작
	PosPixel.Y += RowMargin * 3.f;

	char PickingTimeBuf[64];
	char NumAttemptsBuf[64];
	char AccumulatedTimeBuf[64];
	std::snprintf(PickingTimeBuf, sizeof(PickingTimeBuf), "%.2f ms PickingTime", STATS.GetPickingTime());
	std::snprintf(NumAttemptsBuf, sizeof(NumAttemptsBuf), "%d NumAttempts", STATS.GetNumAttempts());
	std::snprintf(AccumulatedTimeBuf, sizeof(AccumulatedTimeBuf), "%.2f ms AccumulatedTime", STATS.GetAccumulatedTime());

	// 글자 폭을 재서 뷰포트 오른쪽 끝에 맞춰 그린다 (긴 줄이 화면 밖으로 나가지 않게)
	const float RightEdgeX = PosNDC.X * DisplaySize.x - 10.f;
	ImDrawList* DrawList = ImGui::GetForegroundDrawList();
	auto DrawRightAligned = [&](const char* Text, float Y)
	{
		const float TextWidth = Font->CalcTextSizeA(FontSize, FLT_MAX, 0.f, Text).x;
		DrawList->AddText(Font, FontSize, ImVec2(RightEdgeX - TextWidth, Y), IM_COL32(75, 255, 75, 255), Text);
	};
	DrawRightAligned(PickingTimeBuf, PosPixel.Y);
	DrawRightAligned(NumAttemptsBuf, PosPixel.Y + RowMargin);
	DrawRightAligned(AccumulatedTimeBuf, PosPixel.Y + RowMargin * 2.f);
}
