#include "FEditor.h"
#include "Runtime/Actors/AActor.h"
#include "Runtime/Actors/ACubeActor.h"
#include "Runtime/Actors/ASphereActor.h"
#include "Runtime/Actors/ACylinderActor.h"
#include "Runtime/Actors/ABillboardActor.h"
#include "Runtime/Actors/ASpotlightActor.h"
#include "Runtime/CoreUObject/UCubeComp.h"
#include "Runtime/CoreUObject/UCylinderComp.h"
#include "Runtime/CoreUObject/UObject.h"
#include "Runtime/CoreUObject/USphereComp.h"
#include "Runtime/Engine/FTimeManager.h"
#include "Runtime/Input/FInputManager.h"
#include "Runtime/Actors/AInstancingActor.h"
#include "Runtime/Rendering/FRenderResourceLibrary.h"
#include "Runtime/Math/Random.h"
#include "Runtime/CoreUObject/FGarbageCollector.h"
#include <numbers>


void FEditor::Initialize(USceneManager *SceneManager) {
  State.ReadFromFile();
  Gizmo.Initialize();

  SelectedActorTextComp = NewObject<UTextInstanceComponent>();
  if (SelectedActorTextComp)
  {
    SelectedActorTextComp->Initialize();
    FGarbageCollector::Get().AddRoot(SelectedActorTextComp.Get());
    SelectedActorTextComp->SetInheritRotation(false);
    SelectedActorTextComp->SetMeshID(FName("Rect"));
    SelectedActorTextComp->SetMaterialID(FName("SelectedActor_Text"));
    SelectedActorTextComp->SetFont(FName("bazziotf"));
  }

  this->SceneManager = SceneManager;
}

void FEditor::Shutdown() {
  if (SelectedActorTextComp) {
    FGarbageCollector::Get().RemoveRoot(SelectedActorTextComp.Get());
  }
  SaveState();
  State.FlushToFile();
}

FRenderResourceLibrary *FEditor::GetRendererLibrary() {
  return &FRenderResourceLibrary::Get();
}

void FEditor::Process() {
  // 씬의 액터 업데이트
  
    if (FInputManager::Get().IsKeyDown(VK_DELETE) && SelectedActor)
    {
        AActor* Target = SelectedActor;
        UnSelectActor();
        Target->Destroy();
    }
    
  if (SceneManager && SceneManager->CurrentScene) {
    SceneManager->CurrentScene->Update(FTimeManager::Get().GetDeltaTime());
  }

  if (SelectedActor) {
    SelectedActor->SetTransform(SelectedTransform);
  }

  SaveState();
  State.Tick(FTimeManager::Get().GetDeltaTime());

}

void FEditor::SaveState() {
  if (EditorViewports.empty()) { return; }

  // 항상 메인 원근 뷰포트 카메라를 저장
  const FEditorViewport& Viewport = EditorViewports[0];
  const FCamera& Camera = Viewport.ViewportCamera;

  State.SetCameraLocation(Camera.Position);
  State.SetCameraPitch(Camera.Pitch);
  State.SetCameraYaw(Camera.Yaw);
  State.SetCameraFOV(Camera.Projection.FOV);
  State.SetGridCellSize(Grid.GetCellSize());
  State.SetGizmoMode(static_cast<uint8>(Gizmo.Mode));
  State.SetGizmoSpace(static_cast<uint8>(Gizmo.GetSpace()));
  State.SetSelectedActor(SelectedActor ? SelectedActor->GetUUID() : static_cast<uint32>(-1));
  State.SetIsViewportSplit(bIsViewportSplit);
  State.SetCenterUV(CenterUV);
}

void FEditor::LoadState()
{
    if (EditorViewports.empty()) { return; }

    // 항상 메인 원근 뷰포트 카메라에 복원
    FEditorViewport& Viewport = EditorViewports[0];
    FCamera& Camera = Viewport.ViewportCamera;

    Camera.Position = State.GetCameraLocation();
    Camera.Pitch = State.GetCameraPitch();
    Camera.Yaw = State.GetCameraYaw();
    Camera.Projection.FOV = State.GetCameraFOV();
    Grid.SetCellSize(State.GetGridCellSize());
    Gizmo.Mode = static_cast<EGizmoMode>(State.GetGizmoMode());
    Gizmo.SetGizmoSpace(static_cast<EGizmoSpace>(State.GetGizmoSpace()));
    bIsViewportSplit = State.GetIsViewportSplit();
    CenterUV = State.GetCenterUV();
}

void FEditor::NewScene() {
  UnSelectActor();
  SceneManager->SetScene(NewObject<UScene>());
  State.ResetToDefaults();
  LoadState();
}

void FEditor::SaveScene(const FString &Path) { SceneManager->SaveScene(Path); }

void FEditor::LoadScene(const FString &Path) 
{

  // 씬 로드
  SceneManager->LoadScene(Path);
  SelectedActor = nullptr;
}

bool FEditor::CheckSceneExists() {
  if (SceneManager->CurrentScene == nullptr)
    return false;
  return true;
}

void FEditor::AddViewport(FEditorViewport Viewport) {
  EditorViewports.push_back(Viewport);
}

void FEditor::DeleteViewport(int32 IndexOfViewport) {
  EditorViewports.erase(EditorViewports.begin() + IndexOfViewport);
}

FEditorViewport *FEditor::GetActiveViewport() {
  if (EditorViewports.empty()) {
    return nullptr;
  }
  if (ActiveViewportIndex >= 0 && ActiveViewportIndex < static_cast<int32>(EditorViewports.size())) {
    return &EditorViewports[ActiveViewportIndex];
  }
  return &EditorViewports[0];
}

bool FEditor::SelectActor(AActor *Actor) {
  if (SelectedActor) {
    UnSelectActor();
  }

  SelectedActor = Actor;
  if (SelectedActor) {
    SelectedTransform = SelectedActor->GetTransform();
    SelectedEulerDegDisplay = SelectedTransform.Rotation.GetEulerXYZ();
    if (Gizmo.Mode == EGizmoMode::None) {
      Gizmo.Mode = EGizmoMode::Translate;
    }

    if (SelectedActorTextComp) {
      SelectedActorTextComp->SetActorOwner(SelectedActor.Get());
      FTransform RelativeTrans;
      RelativeTrans.Location = FVector{ 0.0f, 0.0f, 1.5f }; 
      SelectedActorTextComp->SetRelativeTransform(RelativeTrans);
      SelectedActorTextComp->SetText(L"UUID : " + std::to_wstring(SelectedActor->GetUUID()));
    }
  }

  return true;
}

void FEditor::UnSelectActor() {
  if (SelectedActor) {
    SelectedActor->SetTransform(SelectedTransform);
  }
  SelectedActor = nullptr;
  if (SelectedActorTextComp) {
    SelectedActorTextComp->SetActorOwner(nullptr);
  }
}

TArray<UMeshComponent*> FEditor::GetMeshComponents() const {
  if (!SceneManager || !SceneManager->CurrentScene) {
    return {};
  }
  const auto& Meshes = SceneManager->CurrentScene->GetRenderComponents();
  TArray<UMeshComponent*> Result;
  Result.reserve(Meshes.size());
  for (auto* Mesh : Meshes) {
    Result.push_back(Mesh);
  }
  return Result;
}

void FEditor::ClearSelectionForGC() {
  SelectedActor = nullptr;
  Gizmo.EndInteraction();
  Gizmo.HoveredHandle = EGizmoHandle::None;
}

void FEditor::SpawnActorToCurrentScene(UClass* Type, int Size) {
    if (!SceneManager || !SceneManager->CurrentScene) {
        return;
    }

    if (Size <= 0) { return; }

    const float Min = State.GetSpawnActorMinLocation();
    const float Max = State.GetSpawnActorMaxLocation();
    if (Min > Max) { return; }

    for (int i = 0; i < Size; ++i)
    {

        FVector Location
        {
            Random::GetFloat(Min, Max, 2),
            Random::GetFloat(Min, Max, 2),
            Random::GetFloat(Min, Max, 2),
        };

        FTransform Transform;
        Transform.Location = Location;
        Transform.Scale3D = FVector{ 0.5f, 0.5f, 0.5f };

        AActor* NewActor = SceneManager->CurrentScene->SpawnActor(Type);
        if (!NewActor) { return; }


        FTransform CurrentTransform = NewActor->GetTransform();
        CurrentTransform.Location = Location;
        NewActor->SetTransform(CurrentTransform);

        // 액터 시작 및 선택
        NewActor->BeginPlay();
        SelectActor(NewActor);
    }
}

void FEditor::SpawnInstancingToCurrentScene(int Count)
{
    if (!SceneManager || !SceneManager->CurrentScene || Count <= 0) return;

    // 무작위 색상 계산
    const float Hue = static_cast<float>(std::rand()) / RAND_MAX;
    const float S = 0.85f;
    const float V = 1.0f;
    const float H6 = Hue * 6.0f;
    const int   HI = static_cast<int>(H6);
    const float F  = H6 - static_cast<float>(HI);
    const float P  = V * (1.0f - S);
    const float Q  = V * (1.0f - S * F);
    const float T  = V * (1.0f - S * (1.0f - F));
    FVector4 Color;
    switch (HI % 6)
    {
    case 0: Color = {V, T, P, 1.0f}; break;
    case 1: Color = {Q, V, P, 1.0f}; break;
    case 2: Color = {P, V, T, 1.0f}; break;
    case 3: Color = {P, Q, V, 1.0f}; break;
    case 4: Color = {T, P, V, 1.0f}; break;
    default:Color = {V, P, Q, 1.0f}; break;
    }

    AActor* TargetActor = SceneManager->CurrentScene->SpawnActor(AInstancingActor::StaticClass());
    
    if (!TargetActor) return;
    TargetActor->BeginPlay();

    auto* Comp = TargetActor->GetRootComponent()->Cast<UInstancePrimitiveComponent>();
    if (!Comp) return;

    // 일정 반경 및 높이 이내 좌표 추가
    const float MaxDistance = 25.0f;
    const float MaxHeight = 15.0f;
    const float TwoPi = 6.2831853f;
    const FVector Center = TargetActor->GetTransform().Location;

    for (int i = 0; i < Count; ++i)
    {
        float Angle = (static_cast<float>(std::rand()) / RAND_MAX) * TwoPi;
        float Dist = std::sqrt(static_cast<float>(std::rand()) / RAND_MAX) * MaxDistance;
        float OffsetZ = ((static_cast<float>(std::rand()) / RAND_MAX) * 2.0f - 1.0f) * MaxHeight;
        FVector Pos;
        Pos.X = Center.X + std::cos(Angle) * Dist;
        Pos.Y = Center.Y + std::sin(Angle) * Dist;
        Pos.Z = Center.Z + OffsetZ;
        Comp->AddInstance(Pos, Color);
    }

    // 액터 선택
    SelectActor(TargetActor);
}

void FEditor::ResetSplitViewportCameras()
{
    // 뷰포트가 4개 이상 존재하는지 확인
    if (EditorViewports.size() < 4) return;

    // 0번: Perspective (필요 시 기본 원근 시점으로 리셋)
    EditorViewports[0].ViewportCamera.Projection.ProjectionType = EProjectionType::Perspective;

    // 1번: Top Viewport
    EditorViewports[1].ViewportCamera.Position = { 0.0f, 0.0f, 20.0f };
    EditorViewports[1].ViewportCamera.Pitch = -89.9f;
    EditorViewports[1].ViewportCamera.Yaw = 0.0f;
    EditorViewports[1].ViewportCamera.Projection.ProjectionType = EProjectionType::Orthographic;
    EditorViewports[1].ViewportCamera.Projection.Height = 10.0f;

    // 2번: Front Viewport
    EditorViewports[2].ViewportCamera.Position = { -20.0f, 0.0f, 0.0f };
    EditorViewports[2].ViewportCamera.Pitch = 0.0f;
    EditorViewports[2].ViewportCamera.Yaw = 0.0f;
    EditorViewports[2].ViewportCamera.Projection.ProjectionType = EProjectionType::Orthographic;
    EditorViewports[2].ViewportCamera.Projection.Height = 10.0f;

    // 3번: Side Viewport
    EditorViewports[3].ViewportCamera.Position = { 0.0f, -20.0f, 0.0f };
    EditorViewports[3].ViewportCamera.Pitch = 0.0f;
    EditorViewports[3].ViewportCamera.Yaw = 90.0f;
    EditorViewports[3].ViewportCamera.Projection.ProjectionType = EProjectionType::Orthographic;
    EditorViewports[3].ViewportCamera.Projection.Height = 10.0f;
}