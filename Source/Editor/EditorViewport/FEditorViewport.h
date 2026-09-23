#pragma once
#include "Runtime/Math/FVector2.h"
#include "Runtime/Engine/FCamera.h"
#include "Runtime/Engine/FSceneview.h"
#include <Runtime\Rendering\ShaderConstants.h>

#include "Runtime/Engine/ShowFlags.h"
#include <Runtime\Geometry\FTransform.h>
#include <Editor\Gizmo\FGizmo.h>
#include <Editor\Gizmo\FGizmo.h>
#include <Runtime\Actors\AActor.h>
#include <Runtime\CoreUObject\UTextInstanceComponent.h>
#include <Runtime\Core\IntTypes.h>
#include <ThirdParty\Imgui\imgui.h>

class FEditorViewport final {
	bool bFocused = false;
	bool bHovered = false;
public:

	FEditorViewport() : sceneView{}, editorCtx{}
	{};

	FSceneView sceneView;
	FEditorRenderContext editorCtx;

	FCamera ViewportCamera;
	// 전체 클라이언트 영역 기준 고정 UV: 좌상단 (0,0), 우하단 (1,1).
	// 픽셀 위치/크기는 사용할 때 클라이언트 크기를 곱해 계산한다.
	FVector2 TopLeftUV = { 0.0f, 0.0f };
	FVector2 LengthUV = { 1.0f, 1.0f };

	// 뷰포트 렌더 모드 및 쇼 플래그
	EViewModeIndex ViewMode = EViewModeIndex::VMI_Lit;
	uint64 ShowFlags = static_cast<uint64>(EEngineShowFlags::SF_Primitives) |
		static_cast<uint64>(EEngineShowFlags::SF_BillboardText) |
		static_cast<uint64>(EEngineShowFlags::SF_BoundBox);


	void UpdateViewAndCtx(FLightConstants GlobalLight, AActor& SelectedActor, FTransform Transform, FGizmo& Gizmo, UTextInstanceComponent& Textcomp, FGrid& Grid, FVisualizerRegistry* Visual);

	void UpdateFocusedAndHovered(bool bFocused, bool bHovered);

	void Process();

	[[nodiscard]] bool HasShowFlag(EEngineShowFlags Flag) const {
		return (ShowFlags & static_cast<uint64>(Flag)) != 0;
	}
	
	void ToggleShowFlag(EEngineShowFlags Flag) {
		ShowFlags ^= static_cast<uint64>(Flag);
	}


	[[nodiscard]] bool IsFocused() const { return bFocused; }
	[[nodiscard]] bool IsHovered() const { return bHovered; }

};