#include "FEditorViewport.h"
#include "ThirdParty/Imgui/imgui.h"
#include "ThirdParty/Imgui/imgui_internal.h"
#include "ThirdParty/Imgui/imgui_impl_dx11.h"
#include "ThirdParty/Imgui/imgui_impl_win32.h"


void FEditorViewport::UpdateViewAndCtx(FLightConstants GlobalLight, AActor& SelectedActor, FTransform Transform, 
    FGizmo& Gizmo, UTextInstanceComponent& Textcomp, FGrid& Grid, FVisualizerRegistry* Visual)
{

    sceneView.Camera = &ViewportCamera;
    sceneView.ViewProj = ViewportCamera.CreateViewProjectionMatrix(),
        sceneView.TopLeftUV = TopLeftUV,
        sceneView.LengthUV = LengthUV,
        sceneView.ViewMode = ViewMode,
        sceneView.ShowFlags = ShowFlags,
        sceneView.LightConstants = GlobalLight;

    // 에디터 렌더링 컨텍스트 구성

    editorCtx.SelectedActor = &SelectedActor;
    editorCtx.SelectedTransform = Transform;
    editorCtx.Gizmo = &Gizmo;
    editorCtx.TextComp = &Textcomp;
    editorCtx.Grid = &Grid;
    editorCtx.VisualizerRegistry = Visual;


}

void FEditorViewport::UpdateFocusedAndHovered(bool bFocused, bool bHovered)
{
	this->bFocused = bFocused; this->bHovered = bHovered;
	return;
}

#include "Runtime/Input/FInputManager.h" // 프로젝트의 InputManager 헤더 위치에 맞게 포함

void FEditorViewport::Process()
{
    // 1. 초기 코드의 원본 좌표 계산식 그대로 복원
    const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
    if (!MainViewport) return;

    const ImVec2 MainOrigin = MainViewport->Pos;
    const ImVec2 MainSize = MainViewport->Size;

    // 원래 정확했던 픽셀 오프셋 (12.0f, 32.0f) 유지
    const float StartX = MainOrigin.x + (TopLeftUV.X * MainSize.x) + 12.0f;
    const float StartY = MainOrigin.y + (TopLeftUV.Y * MainSize.y) + 32.0f;

    // 2. FInputManager 마우스 판정
    const FVector2 EngineMousePos = FInputManager::Get().GetMousePosition();
    const ImVec2 MousePos(EngineMousePos.X, EngineMousePos.Y);
    const bool bLButtonClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);

    // [핵심] ForegroundDrawList(최상단 강제) 대신 WindowDrawList 사용
    // 메인 뷰포트 창 컨텍스트 안에서 불릴 경우 해당 윈도우 레이어로 그려지므로
    // 그 위에 뜨는 프리뷰 모달 창보다 낮은 z-order를 갖게 됩니다.
    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    // 3. 버튼 렌더링 람다 (기존 유지)
    auto DrawOverlayButton = [&](const char* Label, float PosX, float PosY, float Width, float Height, bool& outClicked) -> float
        {
            const ImVec2 Min(PosX, PosY);
            const ImVec2 Max(PosX + Width, PosY + Height);

            const bool bHovered = (MousePos.x >= Min.x && MousePos.x <= Max.x && MousePos.y >= Min.y && MousePos.y <= Max.y);
            outClicked = bHovered && bLButtonClicked;

            const ImU32 BgColor = outClicked ? IM_COL32(45, 45, 45, 240)
                : (bHovered ? IM_COL32(75, 75, 75, 230) : IM_COL32(35, 35, 35, 210));

            DrawList->AddRectFilled(Min, Max, BgColor, 10.0f);
            DrawList->AddRect(Min, Max, IM_COL32(90, 90, 90, 200), 10.0f);

            const ImVec2 TextSize = ImGui::CalcTextSize(Label);
            const ImVec2 TextPos(Min.x + (Width - TextSize.x) * 0.5f, Min.y + (Height - TextSize.y) * 0.5f);
            DrawList->AddText(TextPos, IM_COL32(230, 230, 230, 255), Label);

            return Width + 6.0f;
        };

    const float BtnHeight = 22.0f;
    float CurX = StartX;

    // --- (1) 투영 모드 토글 버튼 ---
    bool bIsPerspective = (ViewportCamera.Projection.ProjectionType == EProjectionType::Perspective);
    const char* ProjLabel = bIsPerspective ? "Perspective" : "Orthographic";
    const float ProjWidth = ImGui::CalcTextSize(ProjLabel).x + 18.0f;

    bool bProjClicked = false;
    CurX += DrawOverlayButton(ProjLabel, CurX, StartY, ProjWidth, BtnHeight, bProjClicked);
    if (bProjClicked)
    {
        ViewportCamera.Projection.ProjectionType = bIsPerspective ? EProjectionType::Orthographic : EProjectionType::Perspective;
    }

    // --- (2) 뷰 모드 토글 버튼 ---
    const char* ViewModeLabel = "Lit";
    if (ViewMode == EViewModeIndex::VMI_Unlit) ViewModeLabel = "Unlit";
    else if (ViewMode == EViewModeIndex::VMI_Wireframe) ViewModeLabel = "Wireframe";
    const float ViewModeWidth = ImGui::CalcTextSize(ViewModeLabel).x + 18.0f;

    bool bModeClicked = false;
    CurX += DrawOverlayButton(ViewModeLabel, CurX, StartY, ViewModeWidth, BtnHeight, bModeClicked);
    if (bModeClicked)
    {
        if (ViewMode == EViewModeIndex::VMI_Lit) ViewMode = EViewModeIndex::VMI_Unlit;
        else if (ViewMode == EViewModeIndex::VMI_Unlit) ViewMode = EViewModeIndex::VMI_Wireframe;
        else ViewMode = EViewModeIndex::VMI_Lit;
    }

    // --- (3) Show 드롭다운 버튼 ---
    const char* ShowLabel = "Show";
    const float ShowWidth = ImGui::CalcTextSize(ShowLabel).x + 18.0f;

    char ShowPopupID[32];
    snprintf(ShowPopupID, sizeof(ShowPopupID), "ShowPopup##%p", this);

    bool bShowClicked = false;
    const float ShowBtnX = CurX;
    CurX += DrawOverlayButton(ShowLabel, ShowBtnX, StartY, ShowWidth, BtnHeight, bShowClicked);

    if (bShowClicked)
    {
        ImGui::OpenPopup(ShowPopupID);
    }

    ImGui::SetNextWindowPos(ImVec2(ShowBtnX, StartY + BtnHeight + 4.0f));

    if (ImGui::BeginPopup(ShowPopupID))
    {
        bool bPrimitives = HasShowFlag(EEngineShowFlags::SF_Primitives);
        if (ImGui::Checkbox("Primitives", &bPrimitives))
        {
            ToggleShowFlag(EEngineShowFlags::SF_Primitives);
        }

        bool bBillboard = HasShowFlag(EEngineShowFlags::SF_BillboardText);
        if (ImGui::Checkbox("Billboard Text", &bBillboard))
        {
            ToggleShowFlag(EEngineShowFlags::SF_BillboardText);
        }

        bool bBoundBox = HasShowFlag(EEngineShowFlags::SF_BoundBox);
        if (ImGui::Checkbox("BoundBox", &bBoundBox))
        {
            ToggleShowFlag(EEngineShowFlags::SF_BoundBox);
        }

        ImGui::EndPopup();
    }
}