#pragma once

#include "Editor/Core/FEditor.h"
#include "Editor/EditorViewport/FEditorViewport.h"
#include "Editor/Grid/FGrid.h"
#include "Runtime/Input/FCameraInputController.h"
#include "Runtime/Rendering/FPreviewRenderTarget.h"
#include "Runtime/CoreUObject/UStaticMesh.h"
#include "ThirdParty/Imgui/imgui.h"

enum class EPrevType
{
	Mesh = 0,
	Material,
};
	
	// 프리뷰 및 세부 속성 편집 창
class FImguiPreviewEditorWindow final
{
public:
	FImguiPreviewEditorWindow();
	~FImguiPreviewEditorWindow();

	FImguiPreviewEditorWindow(const FImguiPreviewEditorWindow&) = delete;
	FImguiPreviewEditorWindow& operator=(const FImguiPreviewEditorWindow&) = delete;


	void OpenPreview(UStaticMesh* InMesh, ImGuiID InDockID = 0, EPrevType type = EPrevType::Mesh);

	
	void Close() { bIsOpen = false; }
	void BringToFront();

	[[nodiscard]] bool IsOpen() const { return bIsOpen; }
	[[nodiscard]] UStaticMesh* GetTargetMesh() const { return TargetMesh.Get(); }


	void Process(FEditor& Editor, float DeltaTime);


	void FocusOnMesh();
	FPreviewRenderTarget& GetRenderTarget() { return RenderTarget; }
	FEditorViewport& GetPreviewViewport() { return PreviewViewport; }
	[[nodiscard]] const FString& GetTitleString() const { return TitleString; }
	[[nodiscard]] ImGuiID GetInitialDockID() const { return InitialDockID; }

	// 뷰포트 해상도
	uint32 PreviewWidth = 512;
	uint32 PreviewHeight = 512;
	// 그리드 및 카메라 설정
	bool bShowGrid = true;

	EPrevType prevType = EPrevType::Mesh;

	void SaveAsset();

	TSharedPtr<FMaterial> GetPreviewMaterial() { return PreviewMaterialInstance; }

	void DestroyPreviewResource();

private:

	void ProcessViewportInput(FEditor& Editor, const ImVec2& ViewportPos, const ImVec2& ViewportSize, float DeltaTime);



	void DrawMeshDetailsPanel();
	void DrawMaterialDetailsPanel();

	bool bIsOpen = false;
	bool bFocusRequested = false;
	bool bNeedInitialDock = false;
	ImGuiID InitialDockID = 0;
	TWeakObjectPtr<UStaticMesh> TargetMesh;


	FEditorViewport PreviewViewport;
	FCameraInputController CameraController;

	// 프리뷰 렌더타겟
	FPreviewRenderTarget RenderTarget;


	float CameraSpeed = 3.0f;

	FVector MeshCenter = { 0.0f, 0.0f, 0.0f };
	float MeshExtent = 5.0f;

	// 캐싱된 창 제목 문자열
	FString TitleString;



	//TODO 되게 미련한 방법...직렬화 역직렬화를 사용해서 undo buffer를 만들고 싶음
	TSharedPtr<FMaterial> PreviewMaterialInstance; // 프리뷰 전용 복사본 
	FString OriginalMatKey;                                      // 원본 머티리얼 키
	UStaticMesh* OriginalMesh = nullptr; // 원본 메시 포인터
	bool bIsDirty = false;
};
