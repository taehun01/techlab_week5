#pragma once
#include "Editor/Core/FEditor.h"

class AActor;
class USceneComponent;
class UPrimitiveComponent;
class USpotLightComponent;
class UTextInstanceComponent;
class UStaticMeshComponent;

// 선택된 액터의 컴포넌트 속성을 편집하는 창
class FImguiPropertyWindow final {
public:
	FImguiPropertyWindow() = default;
	~FImguiPropertyWindow() = default;

	// 복사 생성 금지
	FImguiPropertyWindow(const FImguiPropertyWindow&) = delete;
	// 복사 대입 금지
	FImguiPropertyWindow& operator=(const FImguiPropertyWindow&) = delete;

	void Process(FEditor& Editor);

private:
	// 액터 클래스명과 UUID
	void ShowActorHeader(const AActor& Actor) const;

	// 요약 트리
	void ShowComponentHierarchy(const AActor& Actor) const;

	// 컴포넌트별 섹션
	void ShowComponentSections(FEditor& Editor, AActor& Actor);
	void ShowComponentDetails(FEditor& Editor, AActor& Actor, USceneComponent& Comp, bool bIsRoot);

	// 트랜스폼 편집
	void ShowTransform(FEditor& Editor, USceneComponent& Comp, bool bIsRoot) const;

	// 컴포넌트 타입별 속성
	void ShowStaticMeshSettings(UStaticMeshComponent& StaticMeshComp) const;
	void ShowTextSettings(UTextInstanceComponent& TextComp) const;
	void ShowSpotLightSettings(USpotLightComponent& LightComp) const;
	void ShowPrimitiveSettings(AActor& Actor, UMeshComponent& MeshComp, bool bIsRoot) const;

	// 머티리얼의 텍스처 미리보기 겸 드롭 타깃.
	void ShowTextureSlot(UMeshComponent& MeshComp) const;

	// 창 하단의 기즈모 모드/공간 선택.
	void ShowGizmoSettings(FEditor& Editor) const;
};
