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
#include "Runtime/Rendering/FObjectConstantStore.h"
#include "Runtime/Rendering/FRenderer.h"
#include "Runtime/Rendering/ShaderConstants.h"
#include "Runtime/Rendering/FPreviewRenderTarget.h"
#include "Runtime/CoreUObject/UStaticMesh.h"
#include "Runtime/Engine/UScene.h"
#include "FFrustum.h"

#include <algorithm>
#include <execution>
#include <fstream>
#include <numeric>

namespace
{
// [0, Count)를 ChunkSize 단위 구간으로 나눠 Body(Begin, End)를 스레드 풀에서 병렬 실행한다.
// 구간이 하나뿐이면 호출한 스레드에서 바로 실행한다.
template <typename TBody>
void ParallelForRange(size_t Count, const TBody& Body, size_t ChunkSize = 1024)
{
    const size_t NumChunks = (Count + ChunkSize - 1) / ChunkSize;
    if (NumChunks <= 1)
    {
        Body(size_t{ 0 }, Count);
        return;
    }

    TArray<size_t> ChunkIndices(NumChunks);
    std::iota(ChunkIndices.begin(), ChunkIndices.end(), size_t{ 0 });
    std::for_each(std::execution::par, ChunkIndices.begin(), ChunkIndices.end(), [&](size_t Chunk)
    {
        const size_t Begin = Chunk * ChunkSize;
        Body(Begin, (std::min)(Count, Begin + ChunkSize));
    });
}

// 컴포넌트 색과 선택 여부로 최종 ColorOverride를 정한다 (선택되면 밝게 덧칠)
void ComputeColorOverride(const UMeshComponent& MeshComponent, bool bSelected, FVector& OutColor, float& OutAmount)
{
    OutColor = MeshComponent.GetColor();
    OutAmount = MeshComponent.GetColorAmount();

    if (bSelected && OutAmount > 0.0f)
    {
        OutColor = OutColor * 0.7f + FVector{ 0.3f, 0.3f, 0.3f };
    }
    else if (bSelected)
    {
        OutColor = FVector{ 1.0f, 1.0f, 1.0f };
        OutAmount = 0.5f;
    }
}
}

FRenderView::FRenderView(FRenderer &Renderer) : Renderer(Renderer) {}

void FRenderView::CollectScenePrimitives(const UScene& Scene, const FSceneView& View, const AActor* SelectedActor)
{
    ZoneScopedN("CollectScenePrimitives");
    auto& ResLib = FRenderResourceLibrary::Get();
    const FCamera& Camera = *View.Camera;

    // MVP는 만들지 않는다: 셰이더가 World와 b1의 ViewProj(RenderView에서 설정)로 계산한다.
    // 그래서 오브젝트 상수는 카메라와 무관하다.

    // 1) 후보: 씬의 모든 렌더 컴포넌트. bFrustumCulling이 켜져 있으면 오브젝트별 AABB/프러스텀 검사로 거른다.
    //    끄면 화면 밖도 전부 그린다 (GPU 클리핑에 맡김). 전부 보이는 씬에서는 컬링 비용이 순수 손해일 수 있다.

    const FFrustum& Frustum = Camera.CreateFrustum();
    const bool bCullFrustum = bFrustumCulling;

    const TArray<UMeshComponent*>& Candidates = Scene.GetRenderComponents();

    FLodFrameParams LodFrameParams = MakeLodFrameParams(Renderer.GetViewportHeight(), Camera);
    FLodSelectSettings LodSetting = {};
    PrepareMeshLodDistance(LodFrameParams, LodSetting);

    // 2) 병렬 단계: 컴포넌트별 월드 행렬·카메라 거리를 계산한다.
    //    여기서는 읽기만 하는 함수만 호출한다 (상태를 바꾸는 머티리얼 조회 등은 3단계에서 순차로).
    const float NearZ = Camera.Projection.NearZ;
    const float FarZ = Camera.Projection.FarZ;
    const uint64 ShowFlags = static_cast<uint64>(View.ShowFlags);

    // 영구 상수: 병렬 단계에서 상수를 만들어 CPU 사본과 비교하고, 달라진 슬롯만 dirty로 둔다.
    // 업로드는 FlushQueue에서 dirty 슬롯만 한다 (정지한 오브젝트는 카메라가 움직여도 다시 올리지 않는다).
    const bool bUsePersistentConstants = Renderer.SupportsPersistentObjectConstants();
    FObjectConstantStore& ConstantStore = FObjectConstantStore::Get();

    CullResults.resize(Candidates.size());
    {
        ZoneScopedN("Transform (parallel)");
        ParallelForRange(Candidates.size(), [&](size_t Begin, size_t End)
        {
            for (size_t i = Begin; i < End; ++i)
            {
                FPrimitiveCullResult& Result = CullResults[i];
                Result.bVisible = false;

                // UScene::AddRenderComponent가 nullptr를 거르므로 후보는 항상 유효하다
                UMeshComponent* MeshComponent = Candidates[i];

                if (bCullFrustum)
                {
                    const FAxisAlignedBoundingBox WorldBounds = { MeshComponent->GetLocalBounds(), MeshComponent->GetRenderMatrix(Camera)};
                    if (!Frustum.Intersects(WorldBounds))
                    {
                        continue;
                    }
                }

                // 쇼 플래그 확인
                if ((ShowFlags & static_cast<uint64>(MeshComponent->GetShowFlag())) == 0) continue;

                Result.World = MeshComponent->GetRenderMatrix(Camera);

                // 정렬 키용 카메라 거리 (Near~Far를 24비트로 양자화)
                // 월드 위치는 렌더 행렬의 4행에 이미 있다 (기본·빌보드·텍스트 모두 4행 = GetGlobalTransform().Location)
                const FVector WorldLocation{ Result.World.M[3][0], Result.World.M[3][1], Result.World.M[3][2] };
                const FVector CameraToMesh = WorldLocation - Camera.Position;
                float Distance = CameraToMesh.Size();
                float ClipDistance = Distance < FarZ ? Distance : FarZ;
                ClipDistance = ClipDistance > NearZ ? ClipDistance : NearZ;
                const float NormalizedDistance = (ClipDistance - NearZ) / (FarZ - NearZ);
                Result.QuantizedDistance = static_cast<uint32>(0xffffff * NormalizedDistance);

                Result.bVisible = true;

                // 영구 상수 갱신 (슬롯마다 이 스레드만 쓴다). 뷰 모드 셰이딩은 b1이 맡으므로 DisableShading은 0.
                const uint32 PersistentSlot = MeshComponent->GetPersistentConstantSlot();
                if (bUsePersistentConstants && PersistentSlot != FObjectConstantStore::InvalidSlot)
                {
                    const bool bSelected = SelectedActor && MeshComponent->GetActorOwner() == SelectedActor;

                    FObjectConstants Constants{};
                    Constants.World = Result.World;
                    ComputeColorOverride(*MeshComponent, bSelected, Constants.ColorOverride, Constants.ColorOverrideAmount);
                    Constants.UVOffset = MeshComponent->GetRenderUVOffset();
                    ConstantStore.Update(PersistentSlot, Constants);
                }

                // Select LOD Level
                FStaticMesh* StaticMesh = MeshComponent->GetFStaticMesh();
                Result.LodLevel = 0u;
                if (!StaticMesh) continue;

                float WorldScale = 0.f;
                for (uint32 j = 0; j < 3; ++j)
                {
                    float SquaredSum = Result.World.M[j][0] * Result.World.M[j][0]
                                        + Result.World.M[j][1] * Result.World.M[j][1]
                                        + Result.World.M[j][2] * Result.World.M[j][2];
                    if (WorldScale < SquaredSum)
                    {
                        WorldScale = SquaredSum;
                    }
                }
                WorldScale = sqrtf(WorldScale);

                Result.LodLevel = SelectLod(StaticMesh, WorldScale, Distance, MeshComponent->GetLastLod(), LodSetting, LodFrameParams);
                MeshComponent->SetLastLod(Result.LodLevel);
            }
        });
    }

    TracyPlot("Render Components", static_cast<int64_t>(Candidates.size()));

    // 3) 순차 단계: 보이는 컴포넌트의 드로우 항목을 만들어 큐에 넣는다.
    //    정적 메시는 AppendDrawItems(경량 경로)로 FRenderData 생성·복사 없이 포인터와 인덱스 범위만 넣고,
    //    컴포넌트 단위 상수는 컴포넌트당 한 번만 저장한다. 나머지(빌보드·텍스트 등)는 기존 AppendRenderDatas 경로.
    FMaterial* const SimpleMaterial = ResLib.GetMaterial(FName("Simple")).get();
    const float DisableShading = (View.ViewMode == EViewModeIndex::VMI_Unlit) ? 1.0f : 0.0f;

    // 머티리얼의 블렌드 모드에 따라 불투명·반투명 큐로 분기 (Material·Mesh는 항상 유효)
    const auto PushItem = [this](const FDrawItem& Item, uint32 QuantizedDistance)
    {
        const EBlendMode BlendMode = Item.Material->GetBlendMode();
        if (BlendMode == EBlendMode::Additive || BlendMode == EBlendMode::Translucent)
        {
            RenderQueue.PushTranslucent(Item, GetSortKey(Item.Material, Item.Mesh, 0xffffff - QuantizedDistance));
        }
        else
        {
            RenderQueue.PushOpaque(Item, GetSortKey(Item.Material, Item.Mesh, QuantizedDistance));
        }
    };

    for (size_t CandidateIndex = 0; CandidateIndex < Candidates.size(); ++CandidateIndex)
    {
        const FPrimitiveCullResult& Cull = CullResults[CandidateIndex];
        if (!Cull.bVisible) continue;

        UMeshComponent* MeshComponent = Candidates[CandidateIndex];
        const FMatrix& World = Cull.World;
        const uint32 QuantizedDistance = Cull.QuantizedDistance;

        const bool bSelected = SelectedActor && MeshComponent->GetActorOwner() == SelectedActor;
        const uint32 LodLevel = bSelected ? 0u : Cull.LodLevel;

        // 경량 경로 먼저 시도
        TArray<FDrawItem>& DrawItems = ComponentDrawItems;
        DrawItems.clear();
        FVector2 UVOffset;
        const bool bFastPath = MeshComponent->AppendDrawItems(LodLevel, DrawItems, UVOffset);

        TArray<FRenderData>& RenderDatas = ComponentRenderDatas;
        RenderDatas.clear();
        if (!bFastPath)
        {
            MeshComponent->AppendRenderDatas(*View.Camera, RenderDatas, LodLevel);
        }
        // 그릴 데이터가 없으면 행렬 계산 전에 다음 컴포넌트로
        if (DrawItems.empty() && RenderDatas.empty()) continue;

        // 영구 슬롯이 있으면 상수는 병렬 단계에서 이미 갱신됐다. 항목은 슬롯 번호만 들고 간다.
        const uint32 PersistentSlot = MeshComponent->GetPersistentConstantSlot();
        if (bFastPath && bUsePersistentConstants && PersistentSlot != FObjectConstantStore::InvalidSlot)
        {
            for (FDrawItem& Item : DrawItems)
            {
                if (!Item.Material) Item.Material = SimpleMaterial;
                Item.PrimitiveIndex = PersistentSlot;
                Item.bPersistentConstants = true;
                PushItem(Item, QuantizedDistance);
            }
            continue;
        }

        // 여기부터는 이번 프레임에만 쓰는 상수 (빌보드·텍스트 등, 또는 영구 상수를 못 쓰는 환경)
        FVector FinalColorOverride;
        float   FinalColorOverrideAmount = 0.0f;
        ComputeColorOverride(*MeshComponent, bSelected, FinalColorOverride, FinalColorOverrideAmount);

        if (bFastPath)
        {
            // 섹션들이 공유하는 상수를 한 번만 저장한다 (UVScale 등 나머지는 기본값)
            uint32 PrimitiveIndex = 0u;
            FObjectConstants& Constants = RenderQueue.AddPrimitive(PrimitiveIndex);
            Constants.World = World;
            Constants.ColorOverride = FinalColorOverride;
            Constants.ColorOverrideAmount = FinalColorOverrideAmount;
            Constants.DisableShading = DisableShading;
            Constants.UVOffset = UVOffset;

            for (FDrawItem& Item : DrawItems)
            {
                if (!Item.Material) Item.Material = SimpleMaterial;
                Item.PrimitiveIndex = PrimitiveIndex;
                PushItem(Item, QuantizedDistance);
            }
            continue;
        }

        // 기존 경로: 슬롯별 RenderData 순회 처리
        for (FRenderData& Data : RenderDatas)
        {
            // 인스턴스 데이터가 있으면 인스턴싱 큐로 분류
            if (!Data.Instances.empty())
            {
                Data.bSelected = bSelected;
                RenderQueue.PushInstancing(Data);
                continue;
            }

            // 컴포넌트가 포인터를 채워 줬으면 문자열 맵 조회를 건너뛴다
            FStaticMesh* Mesh = Data.MeshPtr ? Data.MeshPtr : ResLib.GetMesh(Data.MeshId).get();
            if (!Mesh) continue;
            FMaterial* Material = Data.MaterialPtr ? Data.MaterialPtr : ResLib.GetMaterial(Data.MaterialId).get();

            // 컴포넌트가 채운 상수(UVScale/UVOffset 등)를 기반으로 공통 값만 덮어쓴다
            uint32 PrimitiveIndex = 0u;
            FObjectConstants& Constants = RenderQueue.AddPrimitive(PrimitiveIndex);
            Constants = Data.Constants;
            Constants.World = World;
            Constants.ColorOverride = FinalColorOverride;
            Constants.ColorOverrideAmount = FinalColorOverrideAmount;
            Constants.DisableShading = DisableShading;

            PushItem({ Mesh, Material ? Material : SimpleMaterial, Data.startidx, Data.indicesCount, PrimitiveIndex }, QuantizedDistance);
        }
    }
}

void FRenderView::RenderView(const FSceneView& View, const UScene& Scene, const FEditorRenderContext& EditorCtx)
{
    ZoneScopedN("RenderView");
    TracyD3D11Zone(Renderer.GetGpuProfiler(), "Viewport");

    // 뷰포트 시작
    BeginView(View.TopLeftUV, View.LengthUV, View.ViewMode, View.LightConstants);
    // 이 뷰의 카메라 행렬을 b1에 둔다 (오브젝트 상수에는 카메라 값이 없다)
    Renderer.SetViewProjection(View.ViewProj);

    // 씬 컴포넌트 수집
    CollectScenePrimitives(Scene, View, EditorCtx.SelectedActor);

    // 정렬
    {
        ZoneScopedN("RenderQueue Sort");
        RenderQueue.Sort();
    }

    // 기본 씬 오브젝트 패스
    FlushBasePass(*View.Camera);

    // 에디터 라인 패스
    if (EditorCtx.Grid) {
        ZoneScopedN("Grid");
        TracyD3D11Zone(Renderer.GetGpuProfiler(), "Grid");
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
    
    {
        ZoneScopedN("LinePass");
        FlushLinePass(*View.Camera);
    }

    // 후처리 외곽선 패스
    {
        ZoneScopedN("PostProcess Outline");
        TracyD3D11Zone(Renderer.GetGpuProfiler(), "PostProcess Outline");
        RenderPostProcessPass(*View.Camera, EditorCtx.SelectedActor, View.TopLeftUV, View.LengthUV);
    }

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
        const FRenderData& Data = TextComp->GetPureRenderData();
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
    const FRenderData& Data = textcomp->GetPureRenderData();
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

    // 화면 위치는 셰이더가 World와 b1의 ViewProj로 계산한다
    Renderer.SetViewProjection(Camera.CreateViewProjectionMatrix());
    FObjectConstants Constants{};
    Constants.World = MeshComp->GetRenderMatrix(Camera);
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
    // 라인 정점은 월드 좌표라 World는 단위 행렬(기본값), 카메라 행렬은 b1로 넘긴다
    Renderer.SetViewProjection(ViewProjection);
    FObjectConstants Constants{};
    Constants.DisableShading = 1.0f;
    Renderer.FlushLineBatch(Constants, PipelineId);
}

void FRenderView::DrawItem(const FDrawItem& Item)
{
    // FMaterial 자체에 연결된 파이프라인 및 텍스처로 바로 드로우
    // 화면 위치는 셰이더가 World와 b1의 ViewProj(RenderView에서 설정)로 계산한다
    const FObjectConstants& Constants = Item.bPersistentConstants
        ? FObjectConstantStore::Get().GetConstants(Item.PrimitiveIndex)
        : RenderQueue.GetPrimitiveConstants()[Item.PrimitiveIndex];
    Renderer.Draw(*Item.Mesh, *Item.Material, Constants,
        Item.StartIndex, Item.IndexCount,
        /*Slot*/ 0u, /*bApplyViewMode*/ true, /*bConstantsInD3DClip*/ true);
}


void FRenderView::FlushQueue(const FCamera& Camera)
{
    ZoneScopedN("FlushQueue");
    TracyPlot("Opaque Draws", static_cast<int64_t>(RenderQueue.GetOpaqueItems().size()));
    TracyPlot("Translucent Draws", static_cast<int64_t>(RenderQueue.GetTranslucentItems().size()));

    // 불투명 패스
    {
        ZoneScopedN("Opaque");
        TracyD3D11Zone(Renderer.GetGpuProfiler(), "Opaque");
        const auto& OpaqueKeys = RenderQueue.GetOpaqueSortKeys();
        const TArray<FDrawItem>& OpaqueItems = RenderQueue.GetOpaqueItems();
        const TArray<FObjectConstants>& PrimitiveConstants = RenderQueue.GetPrimitiveConstants();
        const uint32 OpaqueCount = static_cast<uint32>(OpaqueKeys.size());

        // 드로우마다 Map/Unmap하지 않고, 상수는 미리 올려 둔 뒤 드로우마다 슬롯 오프셋만 바인딩한다.
        //  - 영구 슬롯(정적 메시): 병렬 수집 단계에서 dirty가 된 슬롯만 영구 버퍼에 올린다
        //  - 이번 프레임 슬롯(빌보드·텍스트 등): PrimitiveConstants를 링 버퍼에 순서대로 복사한다
        // 머티리얼 색은 b3(머티리얼 바인딩)로 간다. 큐 항목의 Mesh·Material은 수집 단계에서 항상 유효하게 채워진다.
        const uint32 PrimitiveCount = static_cast<uint32>(PrimitiveConstants.size());
        if (OpaqueCount > 0u && Renderer.SupportsConstantBufferOffset())
        {
            if (Renderer.SupportsPersistentObjectConstants())
            {
                Renderer.UploadPersistentObjectConstants();
            }
            // 업로드 중 버퍼 생성에 실패했으면 영구 슬롯 항목은 CPU 사본으로 한 개씩 그린다 (DrawItem)
            const bool bPersistentReady = Renderer.SupportsPersistentObjectConstants();

            bool bTransientReady = PrimitiveCount == 0u;
            if (!bTransientReady && Renderer.BeginObjectConstants(PrimitiveCount))
            {
                ZoneScopedN("Upload Object Constants");
                for (uint32 p = 0; p < PrimitiveCount; ++p)
                {
                    Renderer.WriteObjectConstants(p, PrimitiveConstants[p]);
                }
                Renderer.EndObjectConstants();
                bTransientReady = true;
            }

            for (uint32 i = 0; i < OpaqueCount; ++i)
            {
                const FDrawItem& Item = OpaqueItems[OpaqueKeys[i].second];
                if (Item.bPersistentConstants ? bPersistentReady : bTransientReady)
                {
                    Renderer.DrawWithObjectConstants(Item.PrimitiveIndex, Item.bPersistentConstants,
                        *Item.Mesh, *Item.Material, Item.StartIndex, Item.IndexCount);
                }
                else
                {
                    DrawItem(Item);
                }
            }
        }
        else
        {
            for (const auto& [SortKey, Index] : OpaqueKeys)
            {
                DrawItem(OpaqueItems[Index]);
            }
        }
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
    {
        ZoneScopedN("Translucent");
        TracyD3D11Zone(Renderer.GetGpuProfiler(), "Translucent");
        for (const auto& [SortKey, Index] : RenderQueue.GetTranslucentSortKeys())
        {
            DrawItem(RenderQueue.GetTranslucentItems()[Index]);
        }
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

    STATS.UpdateRenderQueueNum(RenderQueue.GetOpaqueItems().size(), RenderQueue.GetTranslucentItems().size(), RenderQueue.GetTextRenderQ().size(), RenderQueue.GetInstancingRenderQ().size());

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

uint64 FRenderView::GetSortKey(FMaterial* InMaterial, FStaticMesh* InMesh, uint32 Depth)
{
    // InMaterial·InMesh는 수집 단계에서 항상 유효하다 (메시가 없는 항목은 큐에 넣지 않는다)
    const FRenderPipeline* Pipeline = InMaterial->GetPipeline().get();
    if (!Pipeline)
    {
        return 0xffffff;
    }

    uint64 SortKey = 0u;
    if (InMaterial->GetBlendMode() == EBlendMode::Opaque)
    {
        SortKey = 0u;

        SortKey <<= 10;
        SortKey |= (0x3ff & Pipeline->SortID);

        SortKey <<= 12;
        SortKey |= (0xfff & InMaterial->SortID);

        SortKey <<= 16;
        SortKey |= (0xffff & InMesh->SortID);

        SortKey <<= 24;
        SortKey |= (0xffffff & Depth);
    }
    else if (InMaterial->GetBlendMode() == EBlendMode::Translucent)
    {
        SortKey = 1u;

        SortKey <<= 38 + 24;
        SortKey |= (0xffffff & Depth);
    }

    return SortKey;
}

FLodFrameParams FRenderView::MakeLodFrameParams(float ViewportHeight, const FCamera& Camera)
{
    FLodFrameParams Output;
    if (Camera.Projection.ProjectionType == EProjectionType::Perspective)
    {
        float FOV = Camera.Projection.FOV * 3.141592653 / 180.f;
        Output.ProjScale = ViewportHeight / (2 * tanf(FOV / 2));
        Output.bIsOrthogonal = false;
    }
    else
    {
        Output.ProjScale = ViewportHeight / Camera.Projection.Height;
        Output.bIsOrthogonal = true;
    }
    return Output;
}

void FRenderView::PrepareMeshLodDistance(const FLodFrameParams& Params, const FLodSelectSettings& Setting)
{
    for (const auto& [Key, Mesh]: FRenderResourceLibrary::Get().GetAllUStaticMeshMap())
    {
        FStaticMesh* StaticMesh = Mesh->GetStaticMeshAsset().get();
        if (StaticMesh && StaticMesh->GetMeshLodCount() > 1)
        {
            for (uint32 i = 0; i < MAX_MESH_LOD; ++i)
            {
                float LodError = StaticMesh->GetMeshLodErrors()[i];
                StaticMesh->LodSwitchDistance[i] = LodError * Params.ProjScale / Setting.AllowedErrorPixels;
            }
        }
    }
}

uint8 FRenderView::SelectLod(const FStaticMesh* StaticMesh, float Scale, float Distance, uint8 PrevLod, const FLodSelectSettings& Setting, const FLodFrameParams& Params)
{
    uint32 MeshLodCount = StaticMesh->GetMeshLodCount();

    if (MeshLodCount <= 1)
    {
        return 0u;
    }

    // ForceLod 분기는 제거했다: 유일한 호출부(CollectScenePrimitives)가 기본값(-1)만 넘긴다
    uint8 SelectedLod = 0u;
    for (int32 CurrentLod = static_cast<int32>(MeshLodCount) - 1; CurrentLod >= 0; --CurrentLod)
    {
        float LodSwitchDistance = StaticMesh->LodSwitchDistance[CurrentLod];
        
        if (CurrentLod > PrevLod)
        {
            LodSwitchDistance *= (1 + Setting.Hysteresis);
        }
        if (CurrentLod == PrevLod)
        {
            LodSwitchDistance *= (1 - Setting.Hysteresis);
        }
        if (Params.bIsOrthogonal)
        {
            if (StaticMesh->GetMeshLodErrors()[CurrentLod] * Scale * Params.ProjScale <= Setting.AllowedErrorPixels)
            {
                SelectedLod = static_cast<uint8>(CurrentLod);
                break;
            }
        }
        else
        {
            if (Distance > LodSwitchDistance * Scale)
            {
                SelectedLod = static_cast<uint8>(CurrentLod);
                break;
            }
        }

    }
    return SelectedLod;
}
