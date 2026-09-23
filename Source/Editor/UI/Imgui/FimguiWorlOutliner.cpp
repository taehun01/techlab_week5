#include "FimguiWorlOutliner.h"
#include "Runtime/Actors/AActor.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/CoreUObject/USceneComponent.h"
#include "Runtime/Engine/UScene.h"
#include "ThirdParty/Imgui/imgui.h"
#include <algorithm>
#include <string>

void FimguiWorldOutliner::Process(FEditor &Editor) {
  ImGui::Begin("World Outliner");

  UScene *Scene = Editor.GetCurrentScene();
  if (!Scene) {
    ImGui::TextDisabled("No Active Scene");
    ImGui::End();
    return;
  }

  // 검색 필터 버퍼
  static char FilterBuffer[128] = "";
  ImGui::SetNextItemWidth(-1.0f);
  ImGui::InputTextWithHint("##OutlinerFilter", "Search...", FilterBuffer,
                           sizeof(FilterBuffer));

  std::string FilterStr = FilterBuffer;
  std::transform(FilterStr.begin(), FilterStr.end(), FilterStr.begin(),
                 ::tolower);

  ImGui::Separator();

  const auto &Actors = Scene->GetActors();
  AActor *SelectedActor = Editor.GetSelectedActor();

  // 액터 목록 표시
  ImGui::BeginChild("ActorList",
                    ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing()), false);

  for (AActor *Actor : Actors) {
    if (!Actor) {
      continue;
    }

    // 액터 이름 생성
    const char *ClassName = Actor->GetClass()
                                ? Actor->GetClass()->GetDisplayName().c_str()
                                : "Actor";
    std::string ActorLabel = std::string(ClassName) +
                             " (ID: " + std::to_string(Actor->GetUUID()) + ")";

    // 검색어 필터링
    if (!FilterStr.empty()) {
      std::string LowerLabel = ActorLabel;
      std::transform(LowerLabel.begin(), LowerLabel.end(), LowerLabel.begin(),
                     ::tolower);
      if (LowerLabel.find(FilterStr) == std::string::npos) {
        continue;
      }
    }

    const bool bIsSelected = (Actor == SelectedActor);
    ImGuiTreeNodeFlags NodeFlags =
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (bIsSelected) {
      NodeFlags |= ImGuiTreeNodeFlags_Selected;
    }

    const auto &Components = Actor->GetAttachedComponents();
    if (Components.empty()) {
      NodeFlags |=
          ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    // 트리 노드 렌더링
    const bool bNodeOpen = ImGui::TreeNodeEx(
        reinterpret_cast<void *>(static_cast<uintptr_t>(Actor->GetUUID())),
        NodeFlags, "%s", ActorLabel.c_str());

    // 클릭 시 액터 선택
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
      Editor.SelectActor(Actor);
    }

    // 액터 우클릭 팝업
    if (ImGui::BeginPopupContextItem()) {
      if (ImGui::MenuItem("Select")) {
        Editor.SelectActor(Actor);
      }
      if (ImGui::MenuItem("Deselect")) {
        Editor.UnSelectActor();
      }
      ImGui::EndPopup();
    }

    // 자식 컴포넌트 목록 전개
    if (bNodeOpen && !Components.empty()) {
      for (USceneComponent *Comp : Components) {
        if (!Comp) {
          continue;
        }

        const char *CompClassName =
            Comp->GetClass() ? Comp->GetClass()->GetDisplayName().c_str()
                             : "Component";
        std::string CompLabel = std::string(CompClassName) +
                                " (ID: " + std::to_string(Comp->GetUUID()) +
                                ")";

        ImGuiTreeNodeFlags CompFlags = ImGuiTreeNodeFlags_Leaf |
                                       ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                       ImGuiTreeNodeFlags_SpanAvailWidth;
        ImGui::TreeNodeEx(
            reinterpret_cast<void *>(static_cast<uintptr_t>(Comp->GetUUID())),
            CompFlags, "%s", CompLabel.c_str());
      }

      ImGui::TreePop();
    }
  }

  ImGui::EndChild();

  ImGui::Separator();

  // 하단 컨트롤 영역
  if (SelectedActor) {
    if (ImGui::Button("Delete")) {
      SelectedActor->Destroy();
    }
  }

  ImGui::End();
}
