#include "FEditorApplication.h"

#include "Runtime/CoreUObject/FGarbageCollector.h"
#include "Runtime/CoreUObject/FReferenceCollector.h"
#include "Runtime/CoreUObject/UAnimatedBillboardComp.h"
#include "Runtime/CoreUObject/UBillBoardComp.h"
#include "Runtime/CoreUObject/UCubeComp.h"
#include "Runtime/CoreUObject/UCylinderComp.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "Runtime/CoreUObject/UPrimitiveComponent.h"
#include "Runtime/CoreUObject/USpotLightComponent.h"
#include "Runtime/Engine/FRayCastingManager.h"
#include "Runtime/Geometry/FAxisAlignedBoundingBox.h"
#include "Runtime/Math/FMatrix.h"
#include "Runtime/Rendering/FMesh.h"
#include <Windows.h>

#include "Runtime/Engine/FSceneView.h"

#include "Runtime/Actors/AActor.h"
#include "Runtime/Actors/AInstancingActor.h"
#include "Runtime/CoreUObject/UPlaneComp.h"
#include "Runtime/CoreUObject/USphereComp.h"
#include "Runtime/Core/FStatRegistry.h"

#include "Editor/Visualizer/IVisualizer.h"

void FEditorApplication::Initialize_ImguiWin32DX11(
    HWND &Window, ID3D11Device *Device, ID3D11DeviceContext *Context) {
  ImguiManager.Initialize_ImplWin32DX11(Window, Device, Context);

#if IS_OBJ_VIEWER
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr; // ini 파일 읽기/쓰기 비활성화

  // DisplaySize 명시적 초기화 (Assert 방지)
  RECT Rect;
  if (GetClientRect(Window, &Rect))
  {
      io.DisplaySize = ImVec2(static_cast<float>(Rect.right - Rect.left),
          static_cast<float>(Rect.bottom - Rect.top));
  }
  else
  {
      io.DisplaySize = ImVec2(1200.0f, 800.0f);
  }
#else
#endif

}

void FEditorApplication::Initialize_Runtime(USceneManager *SceneManager, FRenderView *RenderView) {

  this->RenderView = RenderView;
  this->SceneManager = SceneManager;
  this->CurrentScene = SceneManager->CurrentScene;

  Editor.Initialize(SceneManager);
  STATS.Initialize();
  STATS.Reset();

#if IS_OBJ_VIEWER

#else
  FEditorViewport PerspViewport;
  PerspViewport.TopLeftUV = {0.5f, 0.0f};
  PerspViewport.LengthUV = {0.5f, 0.5f};
  PerspViewport.ViewportCamera.Projection.ProjectionType =
      EProjectionType::Perspective;
  Editor.AddViewport(PerspViewport);

  FEditorViewport TopViewport;
  TopViewport.TopLeftUV = {0.0f, 0.0f};
  TopViewport.LengthUV = {0.5f, 0.5f};
  TopViewport.ViewportCamera.Position = {0.0f, 0.0f, 20.0f};
  TopViewport.ViewportCamera.Pitch = -89.9f;
  TopViewport.ViewportCamera.Yaw = 0.0f;
  TopViewport.ViewportCamera.Projection.ProjectionType =
      EProjectionType::Orthographic;
  TopViewport.ViewportCamera.Projection.Height = 10.0f;
  Editor.AddViewport(TopViewport);

  FEditorViewport FrontViewport;
  FrontViewport.TopLeftUV = {0.0f, 0.5f};
  FrontViewport.LengthUV = {0.5f, 0.5f};
  FrontViewport.ViewportCamera.Position = {-20.0f, 0.0f, 0.0f};
  FrontViewport.ViewportCamera.Pitch = 0.0f;
  FrontViewport.ViewportCamera.Yaw = 0.0f;
  FrontViewport.ViewportCamera.Projection.ProjectionType =
      EProjectionType::Orthographic;
  FrontViewport.ViewportCamera.Projection.Height = 10.0f;
  Editor.AddViewport(FrontViewport);

  FEditorViewport SideViewport;
  SideViewport.TopLeftUV = {0.5f, 0.5f};
  SideViewport.LengthUV = {0.5f, 0.5f};
  SideViewport.ViewportCamera.Position = {0.0f, -20.0f, 0.0f};
  SideViewport.ViewportCamera.Pitch = 0.0f;
  SideViewport.ViewportCamera.Yaw = 90.0f;
  SideViewport.ViewportCamera.Projection.ProjectionType =
      EProjectionType::Orthographic;
  SideViewport.ViewportCamera.Projection.Height = 10.0f;
  Editor.AddViewport(SideViewport);

  // 원근 뷰포트를 활성화하고 상태 복원
  Editor.SetActiveViewportIndex(0);
  Editor.LoadState();

#endif
}

void FEditorApplication::Shutdown() { Editor.Shutdown(); }

void FEditorApplication::Update(float DeltaTime) {
  BeginFrame();
  Tick(DeltaTime);
}

void FEditorApplication::BeginFrame() { ImguiManager.NewFrame(); }

void FEditorApplication::Tick(float DeltaTime) {
#if IS_OBJ_VIEWER
    static bool bFirstInit = true;
    if (bFirstInit)
    {
        bFirstInit = false;
        UStaticMesh* Mesh = FRenderResourceLibrary::Get().GetUStaticMesh("Cube");
        OpenPreviewWindow(Mesh, EPrevType::Mesh);
    }

    for (const auto& Window : PreviewWindows)
    {
        if (Window && Window->IsOpen())
        {
            // 메인 ImGui 뷰포트 영역(작업 영역) 전체 크기 가져오기
            const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(MainViewport->WorkPos);
            ImGui::SetNextWindowSize(MainViewport->WorkSize);

            // 프리뷰 창 UI 처리 (
            Window->Process(Editor, DeltaTime);
        }
    }
#else
    ToolBar.Process(Editor, ConsoleWindow, ControlPanelWindow, PropertyWindow);
    EditorViewportWindow.Process(Editor, DeltaTime);
    WorldOutliner.Process(Editor);
    ControlPanelWindow.Process(Editor);
    PropertyWindow.Process(Editor);
    ConsoleWindow.Process(Editor);
    ContentsDrawer.Process(Editor);
    OverlayStat.Process(Editor, DeltaTime);
    STATS.Reset();

    for (const auto& Window : PreviewWindows)
    {
        if (Window && Window->IsOpen())
        {
            Window->Process(Editor, DeltaTime);
        }
    }
#endif

    Editor.Process();
}

#include "ThirdParty/Imgui/imgui.h"
#include "ThirdParty/Imgui/imgui_internal.h"
#include <Runtime\CoreUObject\UMeshComponent.h>

void FEditorApplication::OpenPreviewWindow(UStaticMesh* InMesh, EPrevType type)
{
    if (!InMesh)
    {
        return;
    }

    // 닫힌 창 정리
    PreviewWindows.erase(
        std::remove_if(PreviewWindows.begin(), PreviewWindows.end(),
            [](const TSharedPtr<FImguiPreviewEditorWindow>& Win) {
                return !Win || !Win->IsOpen();
            }),
        PreviewWindows.end()
    );

    const FString CurrentMatName = (!InMesh->Materials.empty()) ? InMesh->Materials[0] : "";

    // 이미 열려 있는 창인지 검사
    for (const auto& Window : PreviewWindows)
    {
        if (Window && Window->IsOpen())
        {
            if (type == EPrevType::Mesh && Window->prevType == EPrevType::Mesh)
            {
                FString ExpectedTitle = InMesh->MeshId.ToString() + "###PreviewMeshEditor_" + InMesh->MeshId.ToString();
                if (Window->GetTitleString() == ExpectedTitle)
                {
                    Window->BringToFront();
                    return;
                }
            }
            else if (type == EPrevType::Material && Window->prevType == EPrevType::Material)
            {
                // TitleString에 머티리얼 이름이 고유하게 들어가 있으므로 이를 기준으로 중복 검사
                FString ExpectedTitle = CurrentMatName + "###PreviewMaterialEditor_" + CurrentMatName;
                if (Window->GetTitleString() == ExpectedTitle)
                {
                    Window->BringToFront();
                    return;
                }
            }
        }
    }

    ImGuiID TargetDockID = 0;
#if !IS_OBJ_VIEWER
    // 기존에 열려 있는 프리뷰 창의 도크 노드 ID 가져오기 (탭 중첩용)
    for (const auto& Window : PreviewWindows)
    {
        if (Window && Window->IsOpen())
        {
            if (ImGuiWindow* Win = ImGui::FindWindowByName(Window->GetTitleString().c_str()))
            {
                if (Win->DockNode)
                {
                    TargetDockID = Win->DockNode->ID;
                    break;
                }
                if (Win->DockId != 0)
                {
                    TargetDockID = Win->DockId;
                    break;
                }
            }
            if (TargetDockID == 0 && Window->GetInitialDockID() != 0)
            {
                TargetDockID = Window->GetInitialDockID();
                break;
            }
        }
    }

    // 첫 창일 때 독립 도크 노드 생성
    if (TargetDockID == 0)
    {
        TargetDockID = ImGui::DockBuilderAddNode(0, ImGuiDockNodeFlags_None);
        const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
        const ImVec2 DefaultPos = MainViewport ? ImVec2(MainViewport->WorkPos.x + 150.0f, MainViewport->WorkPos.y + 80.0f) : ImVec2(200.0f, 100.0f);
        ImGui::DockBuilderSetNodePos(TargetDockID, DefaultPos);
        ImGui::DockBuilderSetNodeSize(TargetDockID, ImVec2(900.0f, 650.0f));
        ImGui::DockBuilderFinish(TargetDockID);
    }
#endif

    // 새 프리뷰 창 생성 및 등록
    auto NewWindow = MakeShared<FImguiPreviewEditorWindow>();
    NewWindow->OpenPreview(InMesh,TargetDockID, type);
    PreviewWindows.push_back(NewWindow);
}





void FEditorApplication::Render() {

#if IS_OBJ_VIEWER
    for (const auto& Window : PreviewWindows)
    {
        if (Window && Window->IsOpen())
        {
            RenderView->RenderPreviewScene(Window->GetRenderTarget(), Window->GetPreviewViewport().ViewportCamera,
                Window->GetTargetMesh(), Window->GetPreviewMaterial(), Window->PreviewWidth, Window->PreviewHeight, Window->bShowGrid);
        }
    }

    // 필수: ImGui 렌더링을 닫고 백버퍼에 그려야 다음 프레임 NewFrame이 동작함
    RenderView->GetRenderer().BindBackBufferWithDepth();
    ImguiManager.RenderUI();

#else
    TArray<FEditorViewport>& EditorViewports = Editor.GetViewports();
    if (EditorViewports.empty())
        return;

    const int StartIdx = 0;
    const int EndIdx =
        Editor.bIsViewportSplit ? static_cast<int>(EditorViewports.size()) : 1;

    for (int i = StartIdx; i < EndIdx; ++i) {
        auto& EditorViewport = EditorViewports[i];

        FGizmo* Gizmo = Editor.ObjectSelected() ? &Editor.GetGizmo() : nullptr;
        UTextInstanceComponent* Text = Editor.ObjectSelected() ? Editor.GetTextcomp() : nullptr;
        EditorViewport.UpdateViewAndCtx(Editor.GlobalLight, Editor.GetSelectedActor(), Editor.SelectedTransform, Gizmo, Text, Editor.GetGrid(), &VisualizerRegistry);

        // 뷰포트 렌더링 명세 구성
        if (EditorViewport.editorCtx.SelectedActor) {
            if (USceneComponent* RootComp =
                EditorViewport.editorCtx.SelectedActor->GetRootComponent()) {
                EditorViewport.editorCtx.SelectedMeshComp = RootComp->Cast<UMeshComponent>();
            }
        }



        // 뷰포트 렌더링 일괄 수행
        RenderView->RenderView(EditorViewport.sceneView, *SceneManager->CurrentScene, EditorViewport.editorCtx);    
    }


    // 스태틱 메시 프리뷰 렌더링
    for (const auto& Window : PreviewWindows)
    {
        if (Window && Window->IsOpen())
        {
            RenderView->RenderPreviewScene(Window->GetRenderTarget(), Window->GetPreviewViewport().ViewportCamera,
                Window->GetTargetMesh(), Window->GetPreviewMaterial(), Window->PreviewWidth, Window->PreviewHeight, Window->bShowGrid, Window->prevType);
        }
    }

    RenderView->GetRenderer().BindBackBufferWithDepth();
    ImguiManager.RenderUI();
#endif

}

void FEditorApplication::OnWindowSize(UINT Width, UINT Height) {
  // 뷰포트 종횡비 갱신
  FVector2 WindowSize = FVector2(static_cast<float>(Width), static_cast<float>(Height));
  STATS.UpdateWindowSize(WindowSize);
  for (auto &Viewport : Editor.GetViewports()) {
    const FVector2 SizePixels = Viewport.LengthUV * WindowSize;
    auto &Camera = Viewport.ViewportCamera;
    Camera.Projection.Aspect = SizePixels.X / SizePixels.Y;
  }
}

void FEditorApplication::CollectGarbage() {
  FGarbageCollector::Get().CollectGarbage();
}
