#include "FRenderView.h"

#include "Editor/Gizmo/FGizmo.h"
#include "Editor/Grid/FGrid.h"
#include "Editor/Visualizer/FVisualizerRegistry.h"
#include "Editor/Visualizer/IVisualizer.h"
#include "Runtime/Actors/AActor.h"
#include "Editor/UI/Imgui/FImguiPreviewEditorWindow.h"
#include "Runtime/CoreUObject/UBillBoardComp.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/Engine/FCamera.h"
#include "Runtime/Engine/FSceneView.h"
#include "Runtime/Math/FVector2.h"
#include "Runtime/Rendering/FRenderResourceLibrary.h"
#include "Runtime/Rendering/FRenderer.h"
#include "Runtime/Rendering/ShaderConstants.h"
#include "Runtime/Rendering/FPreviewRenderTarget.h"
#include "Runtime/CoreUObject/UStaticMesh.h"
#include "Runtime/Engine/UScene.h"

#include <fstream>

FRenderView::FRenderView(FRenderer &Renderer) : Renderer(Renderer) {}

void FRenderView::CollectScenePrimitives(const UScene& Scene, const FSceneView& View, const AActor* SelectedActor)
{
    auto& ResLib = FRenderResourceLibrary::Get();

    for (auto& MeshComponent : Scene.GetRenderComponents())
    {
        if (!MeshComponent) continue;

        // 쇼 플래그 확인
        if ((static_cast<uint64>(View.ShowFlags) & static_cast<uint64>(MeshComponent->GetShowFlag())) == 0)
        {
            continue;
        }

        bool bSelected = false;
        if (MeshComponent->GetActorOwner() && MeshComponent->GetActorOwner() == SelectedActor)
        {
            bSelected = true;
        }


        const auto& RenderDatas = MeshComponent->GetRenderDatas(*View.Camera);

        // 공통 Matrix 및 Color 계산 (루프 밖 1회 수행)
        const FMatrix World = MeshComponent->GetRenderMatrix(*View.Camera);
        const FMatrix MVP = World * View.ViewProj;

        FVector FinalColorOverride = MeshComponent->GetColor();
        float   FinalColorOverrideAmount = MeshComponent->GetColorAmount();

        if (bSelected && FinalColorOverrideAmount > 0.0f)
        {
            FinalColorOverride = FinalColorOverride * 0.7f + FVector{ 0.3f, 0.3f, 0.3f };
        }
        else if (bSelected)
        {
            FinalColorOverride = FVector{ 1.0f, 1.0f, 1.0f };
            FinalColorOverrideAmount = 0.5f;
        }

        const float DisableShading = (View.ViewMode == EViewModeIndex::VMI_Unlit) ? 1.0f : 0.0f;

        // 슬롯별 RenderData 순회 처리
        for (FRenderData Data : RenderDatas)
        {
            Data.bSelected = bSelected;

            // 인스턴스 데이터가 있으면 인스턴싱 큐로 분류
            if (!Data.Instances.empty())
            {
                RenderQueue.PushInstancing(Data);
                continue;
            }

            Data.Constants.MVP = MVP;
            Data.Constants.World = World;
            Data.Constants.ColorOverride = FinalColorOverride;
            Data.Constants.ColorOverrideAmount = FinalColorOverrideAmount;
            Data.Constants.DisableShading = DisableShading;

            // 머티리얼의 블렌드 모드에 따라 불투명 및 반투명 패스 자동 분기
            auto Material = ResLib.GetMaterial(Data.MaterialId);
            if (Material && (Material->GetBlendMode() == EBlendMode::Additive || Material->GetBlendMode() == EBlendMode::Translucent))
            {
                RenderQueue.PushTranslucent(Data);
            }
            else
            {
                RenderQueue.PushOpaque(Data);
            }
        }
    }
}

void FRenderView::RenderView(const FSceneView& View, const UScene& Scene, const FEditorRenderContext& EditorCtx)
{
    // 뷰포트 시작
    BeginView(View.TopLeftUV, View.LengthUV, View.ViewMode, View.LightConstants);

    // 씬 컴포넌트 수집
    CollectScenePrimitives(Scene, View, EditorCtx.SelectedActor);

    // 기본 씬 오브젝트 패스
    FlushBasePass(*View.Camera);

    // 에디터 라인 패스
    if (EditorCtx.Grid) {
        DrawGrid(*View.Camera, *EditorCtx.Grid);
    }

    const bool bShowBounds = (View.ShowFlags & static_cast<uint64>(EEngineShowFlags::SF_BoundBox)) != 0;
    
    if (bShowBounds && EditorCtx.SelectedMeshComp && EditorCtx.VisualizerRegistry) {

        UClass* ClassType = EditorCtx.SelectedMeshComp->GetClass();
        FVisualizerRegistry& Registry = *EditorCtx.VisualizerRegistry;

        IVisualizer* Visualizer = Registry.FindVisualizer(ClassType);

        if (Visualizer)
        {
            Visualizer->Draw(
                *EditorCtx.SelectedMeshComp,
                *this,
                *View.Camera,
                FVector4{0.0f, 1.0f, 0.0f, 1.0f}
            );
        }
    }
    
    FlushLinePass(*View.Camera);

    // 후처리 외곽선 패스
    RenderPostProcessPass(*View.Camera, EditorCtx.SelectedActor, View.TopLeftUV, View.LengthUV);

    // 오버레이 패스
    if (EditorCtx.Gizmo && EditorCtx.SelectedActor)
    {
        RenderOverlayPass(*View.Camera, View, EditorCtx.SelectedTransform, *EditorCtx.Gizmo, EditorCtx.TextComp);
    }
}

void FRenderView::BeginView(FVector2 TopLeftUV, FVector2 LengthUV, EViewModeIndex ViewMode, const FLightConstants& LightConstants)
{
    // 에디터 뷰포트 렌더타겟 바인딩
    Renderer.BindEditorViewportRenderTargets();
    Renderer.SetViewportUV(TopLeftUV, LengthUV);
    Renderer.ClearDepth();
    Renderer.SetRenderMode(ViewMode);
    Renderer.UpdateLightConstants(LightConstants, ViewMode);
}

void FRenderView::DrawGrid(const FCamera& Camera, FGrid& Grid)
{
    Grid.DrawLine(Renderer, Camera);

    FGridLineConstants Constants{};
    Constants.MVP = Camera.CreateViewProjectionMatrix();
    Constants.CameraPosition = Camera.Position;
    Constants.FadeStartDistance = 300.0f;
    Constants.FadeEndDistance = 500.0f;
    Renderer.FlushLineBatch(Constants, FName("Grid"));
}

void FRenderView::FlushBasePass(const FCamera& Camera)
{
    FlushQueue(Camera);
}

void FRenderView::FlushLinePass(const FCamera& Camera)
{
    FlushLineBatch(Camera.CreateViewProjectionMatrix());
}

void FRenderView::RenderPostProcessPass(const FCamera& Camera, const AActor* SelectedActor, FVector2 TopLeftUV, FVector2 LengthUV)
{
    RenderOutline(Camera, SelectedActor, TopLeftUV, LengthUV);
}

void FRenderView::RenderOverlayPass(const FCamera& Camera, const FSceneView& SceneView, const FTransform& SelectedTransform, const FGizmo& Gizmo, UTextInstanceComponent* TextComp)
{
    // 뷰포트 영역 재설정
    Renderer.SetViewportUV(SceneView.TopLeftUV, SceneView.LengthUV);

    // 기즈모 렌더링
    Renderer.ClearDepth();
    Gizmo.Draw(Renderer, SelectedTransform, Camera);

    // 텍스트 오버레이 렌더링
    if (TextComp && (SceneView.ShowFlags & static_cast<uint64>(EEngineShowFlags::SF_BillboardText)))
    {
        Renderer.ClearDepth();
        FRenderData Data = TextComp->GetPureRenderData();
        if (!Data.Instances.empty())
        {
            Renderer.AddTextInstanceArray(Data.Instances, Data.MeshId, Data.MaterialId);
            Renderer.DrawTextInstances(Camera, Data.MeshId, Data.MaterialId);
            Renderer.ClearTextInstances();
        }
    }
}

void FRenderView::RenderGizmo(const FTransform &Transform,
                              const FCamera &Camera, FVector2 TopLeftUV,
                              FVector2 LengthUV, const FGizmo &Gizmo) {
  Renderer.SetViewportUV(TopLeftUV, LengthUV);
  Renderer.ClearDepth();
  Gizmo.Draw(Renderer, Transform, Camera);
}

void FRenderView::RenderGridAndFlush(const FCamera &Camera, FVector2 TopLeftUV,
                                     FVector2 LengthUV, FGrid &Grid) {
  Renderer.SetViewportUV(TopLeftUV, LengthUV);
  Grid.DrawLine(Renderer, Camera);

  FGridLineConstants Constants{};
  Constants.MVP = Camera.CreateViewProjectionMatrix();
  Constants.CameraPosition = Camera.Position;
  Constants.FadeStartDistance = 3.0f;
  Constants.FadeEndDistance = 75.0f;
  Renderer.FlushLineBatch(Constants, FName("Grid"));
}

void FRenderView::RenderLine(const FVector &Start, const FVector &End,
                             const FVector4 &Color) {
  FLineBatcher &LineBatcher = Renderer.GetLineBatcher();
  LineBatcher.DrawLine(Start, End, Color);
}

void FRenderView::RenderBoxCenterExtent(const FVector &Center,
                                        const FVector &Extent,
                                        const FVector4 &Color) {
  FLineBatcher &LineBatcher = Renderer.GetLineBatcher();
  LineBatcher.DrawBoxCenterExtent(Center, Extent, Color);
}

void FRenderView::RenderBoxMinMax(const FVector &Min, const FVector &Max,
                                  const FVector4 &Color) {
  FLineBatcher &LineBatcher = Renderer.GetLineBatcher();
  LineBatcher.DrawBoxMinMax(Min, Max, Color);
}

void FRenderView::RenderQuad(
    const FVector& A,
    const FVector& B,
    const FVector& C,
    const FVector& D,
    const FVector4& Color
)
{
    FLineBatcher& LineBatcher = Renderer.GetLineBatcher();
    LineBatcher.DrawQuad(A, B, C, D, Color);
}

void FRenderView::RenderSphere(const FVector &Center, float Radius,
                               const FVector4 &Color, uint32 Segments) {
  FLineBatcher &LineBatcher = Renderer.GetLineBatcher();
  LineBatcher.DrawSphere(Center, Radius, Color, Segments);
}

void FRenderView::RenderUUIDText(const FCamera& Camera, FVector2 TopLeftUV,
                                 FVector2 LengthUV, UTextInstanceComponent* textcomp, const FSceneView& SceneView)
{
    if (!textcomp) return;

    Renderer.SetViewportUV(TopLeftUV, LengthUV);
    Renderer.ClearDepth();

    // BuildRenderData()로 Font 기반 인스턴스 데이터 획득 후 드로우
    FRenderData Data = textcomp->GetPureRenderData();
    if (!Data.Instances.empty())
    {
        Renderer.AddTextInstanceArray(Data.Instances, Data.MeshId, Data.MaterialId);
        Renderer.DrawTextInstances(Camera, Data.MeshId, Data.MaterialId);
        Renderer.ClearTextInstances();
    }
}

void FRenderView::RenderOutline(const FCamera &Camera,
                                const AActor *SelectedActor,
                                FVector2 TopLeftUV,
                                FVector2 LengthUV) {
  DrawStencilMask(Camera, SelectedActor);
  Renderer.RenderOutline(TopLeftUV, LengthUV);
}

void FRenderView::DrawStencilMask(const FCamera& Camera,
                                  const AActor* SelectedActor) {
    if (!SelectedActor) return;

    USceneComponent* RootComp = SelectedActor->GetRootComponent();
    if (!RootComp) return;

    UMeshComponent* MeshComp = RootComp->Cast<UMeshComponent>();
    if (!MeshComp) return;

    // FRenderData에서 MeshId 읽어 ResLib로 실제 Mesh 획득
    const FRenderData& RD = MeshComp->GetPureRenderData();
    auto Mesh = FRenderResourceLibrary::Get().GetMesh(RD.MeshId);
    if (!Mesh) return;

    const FMatrix ModelMatrix = MeshComp->GetRenderMatrix(Camera);
    FObjectConstants Constants{};
    Constants.World = ModelMatrix;
    Constants.MVP   = Constants.World * Camera.CreateViewProjectionMatrix();
    Constants.DisableShading = 1.0f;

    auto OutlineMaterial = FRenderResourceLibrary::Get().GetMaterial(FName("Outline"));
    if (OutlineMaterial) {
        OutlineMaterial->GetPipeline()->SetStencilRef(1);
        Renderer.Draw(*Mesh, *OutlineMaterial, Constants, 0, -1, 0, false);
    }
}

void FRenderView::RenderPostProcess(const FCamera &Camera, FVector2 TopLeftUV,
                                    FVector2 LengthUV, AActor *SelectedActor) {
  // 에디터 뷰포트 설정 후 후처리 수행
  Renderer.SetViewportUV(TopLeftUV, LengthUV);
  RenderOutline(Camera, SelectedActor, TopLeftUV, LengthUV);
}
void FRenderView::SetViewportUV(FVector2 TopLeftUV, FVector2 LengthUV)
{
    Renderer.SetViewportUV(TopLeftUV, LengthUV);
}

void FRenderView::SetRenderMode(EViewModeIndex InMode)
{
    Renderer.SetRenderMode(InMode);
}

void FRenderView::UpdateLightConstants(const FLightConstants& Constants, const EViewModeIndex InMode)
{
    Renderer.UpdateLightConstants(Constants, InMode);
}

void FRenderView::DrawInstances(const FCamera& Camera)
{
    Renderer.DrawInstances(Camera);
}

void FRenderView::ClearTextInstances()
{
    Renderer.ClearTextInstances();
}

void FRenderView::FlushLineBatch(const FMatrix& ViewProjection, const FName& PipelineId)
{
    FObjectConstants Constants{};
    Constants.MVP = ViewProjection;
    Constants.DisableShading = 1.0f;
    Renderer.FlushLineBatch(Constants, PipelineId);
}

void FRenderView::DrawRenderData(const FRenderData& Data)
{
    auto& ResLib = FRenderResourceLibrary::Get();
    auto Mesh = ResLib.GetMesh(Data.MeshId);
    auto Material = ResLib.GetMaterial(Data.MaterialId);

    if (!Material)
    {
        Material = ResLib.GetMaterial(FName("Simple"));
    }

    if (!Mesh || !Material) return;

    // FMaterial 자체에 연결된 파이프라인 및 텍스처로 바로 드로우
    Renderer.Draw(*Mesh, *Material, Data.Constants, Data.startidx, Data.indicesCount);
}


void FRenderView::FlushQueue(const FCamera& Camera)
{
    // 불투명 패스
    for (const FRenderData& Data : RenderQueue.GetOpaqueRenderQ())
    {
        DrawRenderData(Data);
    }

    // 인스턴싱 패스
    if (!RenderQueue.IsInstancingRQEmpty())
    {
        for (const FRenderData& Data : RenderQueue.GetInstancingRenderQ())
        {
            Renderer.AddTextInstanceArray(Data.Instances, Data.MeshId, Data.MaterialId);
        }
        Renderer.DrawInstances(Camera);
        Renderer.ClearTextInstances();
    }

    // 반투명 패스
    for (const FRenderData& Data : RenderQueue.GetTranslucentRenderQ())
    {
        DrawRenderData(Data);
    }

    // 텍스트 패스
    if (!RenderQueue.IsTextRQEmpty())
    {
        const FRenderData& First = RenderQueue.GetTextRenderQ()[0];
        FName TextMeshId = First.MeshId;
        FName TextMaterialId = First.MaterialId;

        for (const FRenderData& Data : RenderQueue.GetTextRenderQ())
        {
            Renderer.AddTextInstanceArray(Data.Instances, Data.MeshId, Data.MaterialId);
        }
        Renderer.DrawTextInstances(Camera, TextMeshId, TextMaterialId);
        Renderer.ClearTextInstances();
    }

    STATS.UpdateRenderQueueNum(RenderQueue.GetOpaqueRenderQ().size(), RenderQueue.GetTranslucentRenderQ().size(), RenderQueue.GetTextRenderQ().size(), RenderQueue.GetInstancingRenderQ().size());

    RenderQueue.Clear();
}

void FRenderView::RenderPreviewScene( 
    FPreviewRenderTarget& RenderTarget,
    const FCamera& Camera,
    UStaticMesh* TargetMesh,
    TSharedPtr<FMaterial> OverrideMaterial,
    uint32 Width,
    uint32 Height,
    bool bDrawGrid,
    EPrevType prevType)
{
    switch (prevType)
    {
    case EPrevType::Mesh:
        Renderer.RenderMeshPreviewScene(RenderTarget, Camera, TargetMesh, Width, Height, bDrawGrid);
        break;
    case EPrevType::Material:


        Renderer.RenderMaterialPreviewScene(RenderTarget, Camera, TargetMesh->GetStaticMeshAsset(), OverrideMaterial, Width, Height, bDrawGrid);
        break;
    default:
        break;
    }
    
}


