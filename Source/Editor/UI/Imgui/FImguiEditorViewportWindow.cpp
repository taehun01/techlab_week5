#include "FImguiEditorViewportWindow.h"

#include "Runtime/CoreUObject/UPrimitiveComponent.h"
#include "Runtime/Engine/FRayCastingManager.h"
#include "Runtime/Input/FInputManager.h"
#include "Runtime/Math/FVector.h"
#include "Runtime/Core/Log.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/Actors/AActor.h"
#include "ThirdParty/Imgui/imgui.h"
#include "ThirdParty/Imgui/imgui_internal.h"
#include <Runtime\CoreUObject\UMeshComponent.h>

void FImguiEditorViewportWindow::Process(FEditor& Editor, float DeltaTime)
{
    TArray<FEditorViewport>& Viewports = Editor.GetViewports();
    if (Viewports.empty()) return;

    const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
    if (!MainViewport || MainViewport->Size.x <= 0.0f || MainViewport->Size.y <= 0.0f)
    {
        return;
    }
    const FVector2 ClientSize{ MainViewport->Size.x, MainViewport->Size.y };

    BeginWindow();

    const ImVec2 WinPos = ImGui::GetWindowPos();
    const ImVec2 WinSize = ImGui::GetWindowSize();
    const FVector2 MousePos = FInputManager::Get().GetMousePosition();

    // 뷰포트 모드 전환 버튼
    constexpr float ButtonWidth = 60.0f;
    constexpr float ButtonHeight = 18.0f;

    const ImVec2 BtnMin{ WinPos.x + WinSize.x - ButtonWidth - 16.0f, WinPos.y + 20.0f };
    const ImVec2 BtnMax{ BtnMin.x + ButtonWidth, BtnMin.y + ButtonHeight };

    const bool bBtnHovered = (MousePos.X >= BtnMin.x && MousePos.X <= BtnMax.x && MousePos.Y >= BtnMin.y && MousePos.Y <= BtnMax.y);
    const bool bBtnClicked = bBtnHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

    if (bBtnHovered)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    
    ImDrawList* FG = ImGui::GetWindowDrawList();
    const ImU32 BgColor = bBtnClicked ? IM_COL32(35, 55, 85, 255) : (bBtnHovered ? IM_COL32(65, 85, 120, 240) : IM_COL32(45, 55, 75, 220));
    
    FG->AddRectFilled(BtnMin, BtnMax, BgColor, 3.0f);
    FG->AddRect(BtnMin, BtnMax, IM_COL32(110, 130, 160, 255), 3.0f);
    
    const char* BtnLabel = Editor.bIsViewportSplit ? "Single" : "4-Split";
    const ImVec2 LabelSize = ImGui::CalcTextSize(BtnLabel);
    
    FG->AddText(ImVec2(BtnMin.x + (ButtonWidth - LabelSize.x) * 0.5f, BtnMin.y + (ButtonHeight - LabelSize.y) * 0.5f), IM_COL32(235, 235, 235, 255), BtnLabel);
    
    if (bBtnClicked)
    {
        Editor.bIsViewportSplit = !Editor.bIsViewportSplit;

        // 4-Split 모드로 진입했을 때만 카메라 트랜스폼 초기화 실행
        if (Editor.bIsViewportSplit)
        {
            Editor.ResetSplitViewportCameras();
        }
        EndWindow();
        return;
    }




    if (Editor.bIsViewportSplit)
    {
        // 구분선 조작
        const bool bDragging = ProcessSplitterDrag(Editor.CenterUV, WinPos, WinSize, MousePos);

        // 뷰포트 영역 동기화
        SyncSplitViewports(Editor.CenterUV, Viewports, WinPos, WinSize, ClientSize);

        // 분할선 및 라벨 표시
        DrawSplitterOverlay(Editor.CenterUV, WinPos, WinSize);

        if (bDragging)
        {
            EndWindow();
            return;
        }

        // 뷰포트 인터랙션
        ProcessViewportInteraction(Editor, Viewports, WinPos, WinSize, ClientSize, MousePos, DeltaTime);
    }
    else
    {
        // 단일 뷰포트 모드: 항상 0번 Perspective 뷰포트를 전체 창으로 동기화
        Editor.SetActiveViewportIndex(0);
        FEditorViewport& ActiveVP = Viewports[0];

        SyncViewportRect(ActiveVP, ClientSize);

        for (size_t i = 1; i < Viewports.size(); ++i)
        {
            Viewports[i].UpdateFocusedAndHovered(false, false);
        }

        const FVector2 TopLeftPixels = ActiveVP.TopLeftUV * ClientSize;
        const FVector2 SizePixels = ActiveVP.LengthUV * ClientSize;
        const FViewportInput Input = GatherInput(TopLeftPixels, SizePixels);

        ActiveVP.UpdateFocusedAndHovered(Input.bFocused, Input.bHovered);

        if (Input.bFocused || Input.bHovered)
        {
            UpdateSelection(Editor, ActiveVP, Input);
            UpdateGizmo(Editor, ActiveVP, Input);
            UpdateCamera(Editor, ActiveVP, Input, DeltaTime);
        }
    }

    if (Editor.bIsViewportSplit)
    {
        // 4분할 모드: 활성화된 모든 뷰포트의 버튼 표시
        for (FEditorViewport& VP : Viewports)
        {
            VP.Process();
        }
    }
    else
    {
        // 단일 뷰포트 모드: 메인(0번) 뷰포트의 버튼만 표시
        Viewports[0].Process();
    }
    

    EndWindow();
}



bool FImguiEditorViewportWindow::ProcessSplitterDrag(FVector2& CenterUV, const ImVec2& WinPos, const ImVec2& WinSize, const FVector2& MousePos)
{
    const float CenterX = WinPos.x + WinSize.x * CenterUV.X;
    const float CenterY = WinPos.y + WinSize.y * CenterUV.Y;

    // 마우스 거리 판정
    const bool bNearV = std::abs(MousePos.X - CenterX) < 5.0f && (MousePos.Y >= WinPos.y && MousePos.Y <= WinPos.y + WinSize.y);
    const bool bNearH = std::abs(MousePos.Y - CenterY) < 5.0f && (MousePos.X >= WinPos.x && MousePos.X <= WinPos.x + WinSize.x);

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (bNearV && bNearH)
        {
            bDraggingV = true;
            bDraggingH = true;
        }
        else if (bNearV)
        {
            bDraggingV = true;
        }
        else if (bNearH)
        {
            bDraggingH = true;
        }
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        bDraggingV = false;
        bDraggingH = false;
    }

    if (bDraggingV && bDraggingH)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        CenterUV.X = std::clamp(CenterUV.X + ImGui::GetIO().MouseDelta.x / WinSize.x, 0.15f, 0.85f);
        CenterUV.Y = std::clamp(CenterUV.Y + ImGui::GetIO().MouseDelta.y / WinSize.y, 0.15f, 0.85f);
    }
    else if (bDraggingV)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        CenterUV.X = std::clamp(CenterUV.X + ImGui::GetIO().MouseDelta.x / WinSize.x, 0.15f, 0.85f);
    }
    else if (bDraggingH)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        CenterUV.Y = std::clamp(CenterUV.Y + ImGui::GetIO().MouseDelta.y / WinSize.y, 0.15f, 0.85f);
    }
    else if (bNearV && bNearH)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    }
    else if (bNearV)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    else if (bNearH)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }

    return bDraggingV || bDraggingH;
}

void FImguiEditorViewportWindow::SyncSplitViewports(const FVector2& CenterUV, TArray<FEditorViewport>& Viewports, const ImVec2& WinPos, const ImVec2& WinSize, const FVector2& ClientSize)
{
    if (Viewports.size() < 4) return;
    if (ClientSize.X <= 0.0f || ClientSize.Y <= 0.0f || WinSize.x <= 0.0f || WinSize.y <= 0.0f) return;

    const float BaseU = WinPos.x / ClientSize.X;
    const float BaseV = WinPos.y / ClientSize.Y;
    const float SpanU = WinSize.x / ClientSize.X;
    const float SpanV = WinSize.y / ClientSize.Y;

    // Top 좌상단
    Viewports[0].TopLeftUV = { BaseU, BaseV };
    Viewports[0].LengthUV = { SpanU * CenterUV.X, SpanV * CenterUV.Y };

    // Perspective 우상단
    Viewports[1].TopLeftUV = { BaseU + SpanU * CenterUV.X, BaseV };
    Viewports[1].LengthUV = { SpanU * (1.0f - CenterUV.X), SpanV * CenterUV.Y };

    // Front 좌하단
    Viewports[2].TopLeftUV = { BaseU, BaseV + SpanV * CenterUV.Y };
    Viewports[2].LengthUV = { SpanU * CenterUV.X, SpanV * (1.0f - CenterUV.Y) };

    // Side 우하단
    Viewports[3].TopLeftUV = { BaseU + SpanU * CenterUV.X, BaseV + SpanV * CenterUV.Y };
    Viewports[3].LengthUV = { SpanU * (1.0f - CenterUV.X), SpanV * (1.0f - CenterUV.Y) };

    for (auto& VP : Viewports)
    {
        VP.ViewportCamera.Projection.Aspect = (VP.LengthUV.X * ClientSize.X) / (VP.LengthUV.Y * ClientSize.Y);
    }
}

void FImguiEditorViewportWindow::DrawSplitterOverlay(const FVector2& CenterUV, const ImVec2& WinPos, const ImVec2& WinSize) const
{
    const float CenterX = WinPos.x + WinSize.x * CenterUV.X;
    const float CenterY = WinPos.y + WinSize.y * CenterUV.Y;

    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    // 분할선
    DrawList->AddLine(ImVec2(CenterX, WinPos.y), ImVec2(CenterX, WinPos.y + WinSize.y), IM_COL32(70, 70, 70, 255), 2.0f);
    DrawList->AddLine(ImVec2(WinPos.x, CenterY), ImVec2(WinPos.x + WinSize.x, CenterY), IM_COL32(70, 70, 70, 255), 2.0f);

    // 라벨 크기 계산
    constexpr auto LabelColor = IM_COL32(200, 200, 200, 255);
    const ImVec2 TopSize = ImGui::CalcTextSize("[Perspective]");
    const ImVec2 PerspSize = ImGui::CalcTextSize("[Top]");
    const ImVec2 FrontSize = ImGui::CalcTextSize("[Front]");
    const ImVec2 SideSize = ImGui::CalcTextSize("[Side]");

    constexpr float OffsetX = 8.0f;
    constexpr float OffsetY = 6.0f;

    // 사분면 우하단 라벨 표시
    DrawList->AddText(ImVec2(CenterX - TopSize.x - OffsetX, CenterY - TopSize.y - OffsetY), LabelColor, "[Perspective]");
    DrawList->AddText(ImVec2(WinPos.x + WinSize.x - PerspSize.x - OffsetX, CenterY - PerspSize.y - OffsetY), LabelColor, "[Top]");
    DrawList->AddText(ImVec2(CenterX - FrontSize.x - OffsetX, WinPos.y + WinSize.y - FrontSize.y - OffsetY), LabelColor, "[Front]");
    DrawList->AddText(ImVec2(WinPos.x + WinSize.x - SideSize.x - OffsetX, WinPos.y + WinSize.y - SideSize.y - OffsetY), LabelColor, "[Side]");
}

void FImguiEditorViewportWindow::ProcessViewportInteraction(FEditor& Editor, TArray<FEditorViewport>& Viewports, const ImVec2& WinPos, const ImVec2& WinSize, const FVector2& ClientSize, const FVector2& MousePos, float DeltaTime)
{
    const float CenterX = WinPos.x + WinSize.x * Editor.CenterUV.X;
    const float CenterY = WinPos.y + WinSize.y * Editor.CenterUV.Y;

    const bool bAnyMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
                               ImGui::IsMouseDown(ImGuiMouseButton_Right) ||
                               ImGui::IsMouseDown(ImGuiMouseButton_Middle);

    // 마우스 위치 뷰포트 인덱스 계산
    int HoveredIndex = 0;
    if (MousePos.Y < CenterY)
    {
        HoveredIndex = (MousePos.X < CenterX) ? 0 : 1;
    }
    else
    {
        HoveredIndex = (MousePos.X < CenterX) ? 2 : 3;
    }

    // 마우스 클릭 시 활성 뷰포트 락
    if (bAnyMouseDown)
    {
        if (LockedViewportIndex == -1)
        {
            LockedViewportIndex = HoveredIndex;
        }
        HoveredIndex = LockedViewportIndex;
    }
    else
    {
        LockedViewportIndex = -1;
    }

    // 비활성 뷰포트 상태 해제
    for (int i = 0; i < static_cast<int>(Viewports.size()); ++i)
    {
        if (i != HoveredIndex)
        {
            Viewports[i].UpdateFocusedAndHovered(false, false);
        }
    }

    // 활성 뷰포트 입력 및 업데이트
    FEditorViewport& ActiveVP = Viewports[HoveredIndex];
    const FVector2 TopLeftPixels = ActiveVP.TopLeftUV * ClientSize;
    const FVector2 SizePixels = ActiveVP.LengthUV * ClientSize;
    const FViewportInput Input = GatherInput(TopLeftPixels, SizePixels);

    ActiveVP.UpdateFocusedAndHovered(Input.bFocused, Input.bHovered);

    if (Input.bFocused || Input.bHovered)
    {
        Editor.SetActiveViewportIndex(HoveredIndex);
        UpdateSelection(Editor, ActiveVP, Input);
        UpdateGizmo(Editor, ActiveVP, Input);
        UpdateCamera(Editor, ActiveVP, Input, DeltaTime);
    }
}

void FImguiEditorViewportWindow::BeginWindow() const
{
    constexpr ImGuiWindowFlags WindowFlags =
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(30.0f, 30.0f));

    ImGui::Begin("Viewport", nullptr, WindowFlags);

    // 3D 는 이 창 아래에 그려지므로 창 자체는 항상 가장 뒤에 둔다.
    ImGui::BringWindowToDisplayBack(ImGui::GetCurrentWindow());

    ImGui::PopStyleVar(3);
}

void FImguiEditorViewportWindow::EndWindow() const
{
    ImGui::End();
}

void FImguiEditorViewportWindow::SyncViewportRect(FEditorViewport &Viewport,
                                                  const FVector2 &ClientSize) const
{
    const FVector2 WindowPos{ImGui::GetWindowPos().x, ImGui::GetWindowPos().y};
    const FVector2 WindowSize{ImGui::GetWindowSize().x, ImGui::GetWindowSize().y};

    // 창을 접거나 탭으로 숨기면 0 이 될 수 있으므로 나눗셈 전에 막는다.
    if (WindowSize.X <= 0.0f || WindowSize.Y <= 0.0f || ClientSize.X <= 0.0f || ClientSize.Y <= 0.0f)
    {
        return;
    }

    Viewport.ViewportCamera.Projection.Aspect = WindowSize.X / WindowSize.Y;

    // 픽셀 -> 0~1 비율. 창 크기가 바뀌어도 이 값은 그대로 쓸 수 있다.
    Viewport.TopLeftUV = FVector2{WindowPos.X / ClientSize.X, WindowPos.Y / ClientSize.Y};
    Viewport.LengthUV = FVector2{WindowSize.X / ClientSize.X, WindowSize.Y / ClientSize.Y};
}

FImguiEditorViewportWindow::FViewportInput
FImguiEditorViewportWindow::GatherInput(const FVector2 &ViewportTopLeftPixels,
                                        const FVector2 &ViewportSizePixels) const
{
    // 뷰포트 영역 전체를 덮는 클릭 판정용 아이템.
    // 다른 ImGui 창이 위에 있으면 IsItemHovered()/IsItemClicked() 가 false 가
    // 되어 자연스럽게 focus 중재가 된다.
    ImGui::SetCursorScreenPos(ImVec2(ViewportTopLeftPixels.X, ViewportTopLeftPixels.Y));
    ImGui::InvisibleButton(
        "##ViewportInput", ImVec2(ViewportSizePixels.X, ViewportSizePixels.Y),
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

    FViewportInput Input;
    Input.SizePixels = ViewportSizePixels;
    Input.LocalMouse = FInputManager::Get().GetMousePosition() - ViewportTopLeftPixels;

    Input.bHovered = ImGui::IsItemHovered();
    Input.bFocused = ImGui::IsWindowFocused();
    Input.bPickRequested = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    Input.bLeftDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    Input.bLeftReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);

    return Input;
}

void FImguiEditorViewportWindow::ClampWindowToWorkArea() const
{
    const float WorkTop = ImGui::GetMainViewport()->WorkPos.y;
    const ImVec2 WindowPos = ImGui::GetWindowPos();

    if (WindowPos.y < WorkTop)
    {
        ImGui::SetWindowPos(ImVec2(WindowPos.x, WorkTop));
    }
}

void FImguiEditorViewportWindow::UpdateSelection(FEditor &Editor,
                                                 const FEditorViewport &Viewport,
                                                 const FViewportInput &Input)
{
    if (Input.bPickRequested)
    {
        HandlePicking(Editor, Viewport, Input.LocalMouse, Input.SizePixels);
    }
}

void FImguiEditorViewportWindow::UpdateGizmo(FEditor &Editor,
                                             const FEditorViewport &Viewport,
                                             const FViewportInput &Input)
{
    FGizmo &Gizmo = Editor.GetGizmo();

    if (Input.bLeftDown)
    {
        Gizmo.UpdateInteraction(Editor, Input.LocalMouse);
    }

    if (Input.bLeftReleased)
    {
        Gizmo.EndInteraction();
    }

    if (Input.bHovered)
    {
        UpdateGizmoHover(Editor, Viewport, Input.LocalMouse, Input.SizePixels);
    }
    else
    {
        Gizmo.HoveredHandle = EGizmoHandle::None;
    }
}

void FImguiEditorViewportWindow::UpdateCamera(FEditor &Editor, FEditorViewport &Viewport,
                                              const FViewportInput &Input, float DeltaTime)
{
    if (!Input.bFocused)
    {
        return;
    }

    FCamera &Camera = Viewport.ViewportCamera;

    // 직교 투영 뷰포트 조작
    if (Camera.Projection.ProjectionType == EProjectionType::Orthographic)
    {
        // 마우스 휠 확대 축소
        if (Input.bHovered)
        {
            const float Wheel = ImGui::GetIO().MouseWheel;
            if (Wheel != 0.0f)
            {
                const float ZoomFactor = (Wheel > 0.0f) ? 0.85f : 1.15f;
                Camera.Projection.Height = std::clamp(Camera.Projection.Height * ZoomFactor, 0.5f, 500.0f);
            }
        }

        // 우클릭 드래그 평면 이동
        if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && (Input.bHovered || Input.bFocused))
        {
            const ImVec2 MouseDelta = ImGui::GetIO().MouseDelta;
            if (MouseDelta.x != 0.0f || MouseDelta.y != 0.0f)
            {
                const FMatrix Rotation = FMatrix::MakeRotation(FVector(0.0f, Camera.Pitch, Camera.Yaw));
                const FVector Right{ Rotation.M[1][0], Rotation.M[1][1], Rotation.M[1][2] };
                const FVector Up{ Rotation.M[2][0], Rotation.M[2][1], Rotation.M[2][2] };

                const float PixelsY = Input.SizePixels.Y > 0.0f ? Input.SizePixels.Y : 500.0f;
                const float WorldUnitsPerPixel = Camera.Projection.Height / PixelsY;

                Camera.Position -= Right * (MouseDelta.x * WorldUnitsPerPixel);
                Camera.Position += Up * (MouseDelta.y * WorldUnitsPerPixel);
            }
            return;
        }

        if (!Editor.GetGizmo().IsInteracting())
        {
            UpdateShortcuts(Editor);
        }
        return;
    }

    // 원근 투영 뷰포트 조작
    CameraController.CameraRotateSpeed = Editor.State.GetCameraSensitivity();
    CameraController.CameraMoveSpeed = Editor.State.GetCameraSpeed();

    CameraController.UpdateMouseInput(Camera);

    // 우클릭 중에는 WASD 가 카메라 비행에 쓰이므로 단축키와 겹치지 않게 나눈다.
    if (FInputManager::Get().IsMouseDown(EMouseButton::Right))
    {
        // 우클릭 상태 휠 스크롤 속도 조절
        const float Wheel = ImGui::GetIO().MouseWheel;
        if (Wheel != 0.0f)
        {
            float CurSpeed = Editor.State.GetCameraSpeed();
            CurSpeed += Wheel * 0.5f;
            CurSpeed = std::clamp(CurSpeed, 1.0f, 15.0f);
            Editor.State.SetCameraSpeed(CurSpeed);
            CameraController.CameraMoveSpeed = CurSpeed;
        }

        CameraController.UpdateKeyInput(Camera, DeltaTime);
        return;
    }

    // 기즈모를 드래그하는 중에는 모드가 바뀌면 안 된다.
    if (!Editor.GetGizmo().IsInteracting())
    {
        UpdateShortcuts(Editor);
    }
}

void FImguiEditorViewportWindow::UpdateShortcuts(FEditor &Editor) const
{
    FInputManager &Input = FInputManager::Get();
    FGizmo &Gizmo = Editor.GetGizmo();

    // 백틱(`) : 월드/로컬 공간 전환.
    // Translate/Rotate 에서만 의미가 있어 None/Scale 은 제외한다.
    if (Input.IsKeyJustPressed(VK_OEM_3))
    {
        if (Gizmo.Mode != EGizmoMode::None && Gizmo.Mode != EGizmoMode::Scale)
        {
            Gizmo.SetGizmoSpace(
                static_cast<EGizmoSpace>((static_cast<uint8>(Gizmo.GetSpace()) + 1) % 2));
        }
    }

    if (Input.IsKeyJustPressed('Q'))
    {
        Gizmo.Mode = EGizmoMode::None;
    }
    else if (Input.IsKeyJustPressed('W'))
    {
        Gizmo.Mode = EGizmoMode::Translate;
    }
    else if (Input.IsKeyJustPressed('E'))
    {
        Gizmo.Mode = EGizmoMode::Rotate;
    }
    else if (Input.IsKeyJustPressed('R'))
    {
        Gizmo.Mode = EGizmoMode::Scale;
    }
    else if (Input.IsKeyJustPressed(VK_SPACE))
    {
        Gizmo.Mode = static_cast<EGizmoMode>((static_cast<uint8>(Gizmo.Mode) + 1) % 4);
    }
}

void FImguiEditorViewportWindow::HandlePicking(FEditor &Editor,
                                               const FEditorViewport &Viewport,
                                               const FVector2 &LocalMousePixels,
                                               const FVector2 &ViewportSizePixels)
{
    // 기즈모 핸들 위를 눌렀으면 피킹 대신 조작을 시작한다.
    if (Editor.GetSelectedActor() != nullptr)
    {
        FGizmo &Gizmo = Editor.GetGizmo();
        if (Gizmo.HoveredHandle != EGizmoHandle::None)
        {
            Gizmo.BeginInteraction(Editor.SelectedTransform, Gizmo.HoveredHandle,
                                   LocalMousePixels, Viewport.ViewportCamera,
                                   ViewportSizePixels);
            return;
        }
    }

    TArray<UMeshComponent*> Components = Editor.GetMeshComponents();

    UMeshComponent* HitComponent = nullptr;
    FVector ImpactPoint;

    const bool bHit = FRayCastingManager::RayIntersectsMeshes(
        FRayCastingManager::CreateRayFromScreenPosition(
            Viewport.ViewportCamera, LocalMousePixels, ViewportSizePixels),
            Viewport.ViewportCamera,
        Components, HitComponent, ImpactPoint);

    // 피킹은 액터 단위로 선택한다. 소유 액터가 없으면 선택할 수 없다.
    if (!bHit || !HitComponent || !HitComponent->GetActorOwner())
    {
        Editor.UnSelectActor();
        return;
    }

    AActor *OwnerActor = HitComponent->GetActorOwner();
    Editor.SelectActor(OwnerActor);

    const char *ActorClass =
        OwnerActor->GetClass() ? OwnerActor->GetClass()->GetDisplayName().c_str() : "Unknown";
    const char *CompClass =
        HitComponent->GetClass() ? HitComponent->GetClass()->GetDisplayName().c_str() : "Unknown";

    UE_LOG("[Picking] Actor: %s (UUID: %u), Component: %s (UUID: %u)", ActorClass,
           OwnerActor->GetUUID(), CompClass, HitComponent->GetUUID());
}

void FImguiEditorViewportWindow::UpdateGizmoHover(FEditor &Editor,
                                                  const FEditorViewport &Viewport,
                                                  const FVector2 &LocalMousePixels,
                                                  const FVector2 &ViewportSizePixels)
{
    FRay Ray = FRayCastingManager::CreateRayFromScreenPosition(
        Viewport.ViewportCamera, LocalMousePixels, ViewportSizePixels);

    FGizmo &Gizmo = Editor.GetGizmo();
    Gizmo.HoveredHandle = Gizmo.HitTest(Editor.SelectedTransform, Ray, Viewport.ViewportCamera);
}
