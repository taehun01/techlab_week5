#pragma once

#include "Editor/EditorViewport/FEditorViewport.h"
#include "Editor/Gizmo/FGizmo.h"
#include "Editor/Grid/FGrid.h"
#include "Editor/Core/FEditorState.h"
#include "Runtime/Core/IntTypes.h"
#include "Runtime/Core/TArray.h"
#include "Runtime/CoreUObject/UObject.h"
#include "Runtime/CoreUObject/TWeakObjectPtr.h"
#include "Runtime/Actors/AActor.h"
#include "Runtime/Engine/USceneManager.h"
#include "Runtime/Rendering/ShaderConstants.h"
#include "Runtime/CoreUObject/UTextInstanceComponent.h"

enum class EEditorPrimitiveType : uint8 {
  Cube,
  Cylinder,
  Sphere,
  Billboard,
  Spotlight,
};

class FEditor {
public:
  FTransform SelectedTransform;
  FVector SelectedEulerDegDisplay;

  // TODO: 이건 Scene에 들어가야함. 아마 아래와 같은 컴포넌트가 부착된 액터로 들어가야할 것
  // https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UDirectionalLightComponent
  FLightConstants GlobalLight;

  FEditorState State;

public:
  void Initialize(USceneManager *SceneManager);
  void Shutdown();

  void Process();

  void NewScene();
  void SaveScene(const FString &Path);
  void LoadScene(const FString &Path);
  bool CheckSceneExists();

  void AddViewport(FEditorViewport Viewport);
  void DeleteViewport(int32 IndexOfViewport);
  FEditorViewport* GetActiveViewport(); // 임시로 0번 반환

  void UpdateCamera();

  bool SelectActor(AActor *Actor);
  void UnSelectActor();
  AActor *GetSelectedActor() const { return SelectedActor.Get(); }
  [[nodiscard]] bool ActorSelected() const { return SelectedActor.IsValid(); }
  [[nodiscard]] bool ObjectSelected() const { return SelectedActor.IsValid(); }

  [[nodiscard]] TArray<FEditorViewport> &GetViewports() {
    return EditorViewports;
  }
  int32 GetActiveViewportIndex() const { return ActiveViewportIndex; }
  void SetActiveViewportIndex(int32 Index) { ActiveViewportIndex = Index; }
  [[nodiscard]] UScene *GetCurrentScene() const {
    return SceneManager ? SceneManager->CurrentScene : nullptr;
  }
  void SpawnActorToCurrentScene(UClass* Type, int Count = 1);
  void SpawnInstancingToCurrentScene(int Count);
  // 피킹 등에서 현재 씬의 렌더링 대상 컴포넌트가 필요할 때 사용
  [[nodiscard]] TArray<UMeshComponent*> GetMeshComponents() const;

  // 피킹 성능 비교용 (컨트롤 패널에서 방식 선택, 뷰포트에서 측정)
  // 시간은 모두 Flush를 포함한 총시간. Flush 값은 그중 옥트리 갱신이 차지한 몫이다.
  struct FPickingStat
  {
    double LastMs = 0.0;
    double TotalMs = 0.0;
    double LastFlushMs = 0.0;
    double TotalFlushMs = 0.0;
    int32 Count = 0;

    void Add(double Ms, double FlushMs = 0.0)
    {
      LastMs = Ms;
      TotalMs += Ms;
      LastFlushMs = FlushMs;
      TotalFlushMs += FlushMs;
      ++Count;
    }
    [[nodiscard]] double GetAverageMs() const { return Count > 0 ? TotalMs / Count : 0.0; }
    [[nodiscard]] double GetAverageFlushMs() const { return Count > 0 ? TotalFlushMs / Count : 0.0; }
    void Reset() { *this = FPickingStat{}; }
  };
  bool bUseOctreePicking = true;
  bool bUseSceneBVHPicking = true; // 켜져 있으면 옥트리/브루트포스 선택보다 우선한다
  FPickingStat SceneBVHPickingStat;
  FPickingStat OctreePickingStat;
  FPickingStat BruteForcePickingStat;
  FGizmo &GetGizmo() { return Gizmo; }
  FGrid &GetGrid() { return Grid; }
  FRenderResourceLibrary *GetRendererLibrary();

  void ClearSelectionForGC();

  void SaveState();
  void LoadState();
  UTextInstanceComponent* GetTextcomp() { return SelectedActorTextComp; }

  void ResetSplitViewportCameras();

  //다중 뷰포트
  bool bIsViewportSplit = false;
  FVector2 CenterUV = { 0.5,0.5 };


private:
  USceneManager *SceneManager =
      nullptr; // 씬을 다중으로 가질 수 있도록 구조개선 가능-이경우 에디터쪽에
               // 클래스를 추가해 씬과 FEditorViewport들을 연관
  TArray<FEditorViewport> EditorViewports;
  int32 ActiveViewportIndex = 0;

  FGizmo Gizmo;
  FGrid Grid;
  TWeakObjectPtr<AActor> SelectedActor;
  TWeakObjectPtr<UTextInstanceComponent> SelectedActorTextComp;
};
