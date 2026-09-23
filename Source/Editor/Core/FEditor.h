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
