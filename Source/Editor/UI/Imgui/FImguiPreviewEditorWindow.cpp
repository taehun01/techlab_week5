#include "FImguiPreviewEditorWindow.h"
#include "Runtime/Engine/FRenderView.h"
#include "Runtime/Rendering/FRenderer.h"
#include "Runtime/Rendering/FRenderResourceLibrary.h"
#include "Runtime/Rendering/ShaderConstants.h"
#include "Runtime/Input/FInputManager.h"
#include "ThirdParty/Imgui/imgui.h"
#include "ThirdParty/Imgui/imgui_internal.h"
#include "Editor/UI/Imgui/FImguiDragDrop.h"
#include <algorithm>
#include <filesystem>
#include <Runtime/Core/TObjectIterator.h>

FImguiPreviewEditorWindow::FImguiPreviewEditorWindow()
{
	PreviewViewport.ViewportCamera.Projection.ProjectionType = EProjectionType::Perspective;
	PreviewViewport.ViewportCamera.Projection.FOV = 60.0f;
	PreviewViewport.ViewportCamera.Projection.Aspect = 1.0f;

	CameraController.CameraMoveSpeed = 5.0f;
	CameraController.CameraRotateSpeed = 0.5f;
}

FImguiPreviewEditorWindow::~FImguiPreviewEditorWindow()
{
	DestroyPreviewResource();
}

void FImguiPreviewEditorWindow::OpenPreview(UStaticMesh* InMesh, ImGuiID InDockID, EPrevType type)
{
	if (!InMesh)
	{
		return;
	}

	bIsOpen = true;
	InitialDockID = InDockID;
	bFocusRequested = true;
	bNeedInitialDock = true;
	prevType = type;
	bIsDirty = false;

#if IS_OBJ_VIEWER
	TitleString = "OBJ_Viewer###PreviewEditor";
	TargetMesh = InMesh;
#else
	switch (type)
	{
	case EPrevType::Mesh:
	{
		OriginalMesh = InMesh;
		TitleString = OriginalMesh->MeshId.ToString() + "###PreviewMeshEditor_" + OriginalMesh->MeshId.ToString();

		// 원본 메시를 복제하여 프리뷰 렌더링용 TargetMesh로 사용
		TargetMesh = OriginalMesh->ClonePreviewMesh();
		break;
	}

	case EPrevType::Material:
	{
		OriginalMesh = InMesh; // 원본 구체 메시 보관

		OriginalMatKey = InMesh->Materials[0];

		TitleString = OriginalMatKey + "###PreviewMaterialEditor_" + OriginalMatKey;

		// 프리뷰 구체 메시 복제
		TargetMesh = InMesh->ClonePreviewMesh();

		// 원본 머티리얼 복제 (전역 등록 없이 독립 인스턴스로 멤버 변수에만 보관)
		if (auto OrigMat = FRenderResourceLibrary::Get().GetMaterial(OriginalMatKey))
		{
			PreviewMaterialInstance = OrigMat->Clone();
		}
		break;
	}

	default:
		break;
	}
#endif

	if (InitialDockID != 0)
	{
		ImGui::DockBuilderDockWindow(TitleString.c_str(), InitialDockID);
	}

	FocusOnMesh();
}

void FImguiPreviewEditorWindow::BringToFront()
{
	bIsOpen = true;
	bFocusRequested = true;
	FocusOnMesh();
}

void FImguiPreviewEditorWindow::FocusOnMesh()
{
	if (!TargetMesh.IsValid())
	{
		return;
	}

	const FAxisAlignedBoundingBox& Bounds = TargetMesh->GetBounds();
	MeshCenter = (Bounds.Min + Bounds.Max) * 0.5f;
	MeshExtent = (Bounds.Max - Bounds.Min).Size();

	constexpr float MinDistance = 2.0f;
	constexpr float MaxDistance = 100.0f;
	const float CalculatedDistance = (MeshExtent > 0.1f) ? (MeshExtent * 1.5f) : 5.0f;
	const float Distance = std::clamp(CalculatedDistance, MinDistance, MaxDistance);

	PreviewViewport.ViewportCamera.Pitch = -20.0f;
	PreviewViewport.ViewportCamera.Yaw = 45.0f;

	const FMatrix Rotation = FMatrix::MakeRotation(FVector(0.0f, PreviewViewport.ViewportCamera.Pitch, PreviewViewport.ViewportCamera.Yaw));
	const FVector Forward{ Rotation.M[0][0], Rotation.M[0][1], Rotation.M[0][2] };

	PreviewViewport.ViewportCamera.Position = MeshCenter - Forward * Distance;
}

void FImguiPreviewEditorWindow::Process(FEditor& Editor, float DeltaTime)
{
	if (!bIsOpen)
	{
		return;
	}

#if IS_OBJ_VIEWER
	const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(MainViewport->WorkPos, ImGuiCond_Always);
	ImGui::SetNextWindowSize(MainViewport->WorkSize, ImGuiCond_Always);

	const ImGuiWindowFlags ViewerFlags = ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus;

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.14f, 1.0f));
	const bool bWindowVisible = ImGui::Begin(TitleString.c_str(), nullptr, ViewerFlags);
	ImGui::PopStyleColor();

	if (bWindowVisible)
#else
	ImGui::SetNextWindowSize(ImVec2(850.0f, 600.0f), ImGuiCond_FirstUseEver);

	// 첫 프레임 생성 시 도크 노드에 강제 바인딩 (탭 중첩 보장)
	if (bNeedInitialDock && InitialDockID != 0)
	{
		ImGui::SetNextWindowDockID(InitialDockID, ImGuiCond_Always);
		ImGui::DockBuilderDockWindow(TitleString.c_str(), InitialDockID);
		bNeedInitialDock = false;
	}
	else if (InitialDockID != 0)
	{
		ImGui::SetNextWindowDockID(InitialDockID, ImGuiCond_FirstUseEver);
	}

	if (bFocusRequested)
	{
		ImGui::SetNextWindowFocus();
		bFocusRequested = false;
	}

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.14f, 1.0f));
	const bool bWindowVisible = ImGui::Begin(TitleString.c_str(), &bIsOpen, ImGuiWindowFlags_NoCollapse);
	ImGui::PopStyleColor();
#endif

	if (bWindowVisible)
	{
		// 상단 툴바
		ImGui::Checkbox("Grid", &bShowGrid);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(120.0f);
		ImGui::SliderFloat("Speed", &CameraSpeed, 1.0f, 100.0f, "%.2f");
		ImGui::SameLine();
		if (ImGui::Button("Focus (F)"))
		{
			FocusOnMesh();
		}

		ImGui::SameLine();
		if (bIsDirty)
		{
			// 수정 사항이 있을 때 버튼 강조
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.45f, 0.1f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.55f, 0.2f, 1.0f));
		}
		else
		{
			// 수정 사항이 없으면 어두운 버튼
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.22f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.32f, 1.0f));
		}

#if IS_OBJ_VIEWER

		ImGui::PopStyleColor(2);

#else
		const std::string SaveBtnLabel = bIsDirty ? "Save *" : "Save";
		const bool bSaveClicked = ImGui::Button(SaveBtnLabel.c_str(), ImVec2(65.0f, 0.0f));
		ImGui::PopStyleColor(2);

		// Ctrl + S 단축키 검사
		const bool bCtrlSPressed = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
			ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false);

		if (bSaveClicked || bCtrlSPressed)
		{
			SaveAsset();
		}

#endif

		ImGui::SetNextItemWidth(200.0f);
		if (prevType == EPrevType::Material)
		{
			const std::string CurrentMatName = !OriginalMatKey.empty() ? OriginalMatKey : "Select Material";

			if (ImGui::BeginCombo("##MaterialSelectCombo", CurrentMatName.c_str()))
			{
				const auto& AllMaterials = FRenderResourceLibrary::Get().GetAllMaterials();
				for (const auto& [MatKey, MatAsset] : AllMaterials)
				{
					const bool bIsSelected = (OriginalMatKey == MatKey);

					if (ImGui::Selectable(MatKey.c_str(), bIsSelected))
					{
						// 다른 머티리얼을 선택한 경우 타겟 교체
						OriginalMatKey = MatKey;
						TitleString = OriginalMatKey + "###PreviewMaterialEditor_" + OriginalMatKey;

						if (auto OrigMat = FRenderResourceLibrary::Get().GetMaterial(OriginalMatKey))
						{
							PreviewMaterialInstance = OrigMat->Clone();
						}

						bIsDirty = false;
					}

					if (bIsSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
		}
		else if (prevType == EPrevType::Mesh)
		{
			const std::string CurrentMeshName = TargetMesh.IsValid() ? TargetMesh->MeshId.ToString() : "Select Mesh";

			if (ImGui::BeginCombo("##MeshSelectCombo", CurrentMeshName.c_str()))
			{
				const auto& AllMeshes = FRenderResourceLibrary::Get().AllUStaticMeshMap;
				for (const auto& [MeshKey, MeshAsset] : AllMeshes)
				{
					const FString ItemName = MeshKey;

					// 프리뷰용 전용 구체 메시는 메시 에디터 목록에서 제외
					if (ItemName == "Sphere_Mat")
					{
						continue;
					}

					const bool bIsSelected = (TargetMesh.IsValid() && TargetMesh->MeshId == MeshKey);

					if (ImGui::Selectable(ItemName.c_str(), bIsSelected))
					{
						if (auto OrigMesh = FRenderResourceLibrary::Get().GetUStaticMesh(ItemName))
						{
							if (TargetMesh.IsValid())
							{
							
#if !IS_OBJ_VIEWER
								TargetMesh->Destroy();
#else
#endif
							}


							OriginalMesh = OrigMesh;
							TargetMesh = OrigMesh->ClonePreviewMesh();
							TitleString = OriginalMesh->MeshId.ToString() + "###PreviewMeshEditor_" + OriginalMesh->MeshId.ToString();
							bIsDirty = false;
							FocusOnMesh();
						}
					}

					if (bIsSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
		}

		ImGui::Separator();

		const float DetailsWidth = 260.0f;
		const ImVec2 Avail = ImGui::GetContentRegionAvail();

		const float ViewWidth = std::max(100.0f, Avail.x - DetailsWidth - 10.0f);
		const float ViewHeight = std::max(100.0f, Avail.y);

		const uint32 NewWidth = static_cast<uint32>(ViewWidth);
		const uint32 NewHeight = static_cast<uint32>(ViewHeight);

		if (NewWidth != PreviewWidth || NewHeight != PreviewHeight)
		{
			PreviewWidth = NewWidth;
			PreviewHeight = NewHeight;

			if (PreviewHeight > 0)
			{
				PreviewViewport.ViewportCamera.Projection.Aspect = ViewWidth / ViewHeight;
			}
		}

		// 좌측 뷰포트
		ImGui::BeginChild("PreviewViewport", ImVec2(ViewWidth, ViewHeight), false,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		const ImVec2 ViewportPos = ImGui::GetCursorScreenPos();

		if (RenderTarget.IsValid())
		{
			ImGui::Image(reinterpret_cast<ImTextureID>(RenderTarget.ShaderResourceView.Get()),
				ImVec2(ViewWidth, ViewHeight));
		}

		ProcessViewportInput(Editor, ViewportPos, ImVec2(ViewWidth, ViewHeight), DeltaTime);

		ImGui::EndChild(); 

		ImGui::SameLine(); 
		// 우측 세부 정보 패널
		switch (prevType)
		{
		case EPrevType::Mesh:
			ImGui::BeginChild("MeshDetailsPanel", ImVec2(DetailsWidth, ViewHeight), true);
			DrawMeshDetailsPanel();
			ImGui::EndChild();
			break;
		case EPrevType::Material:
			ImGui::BeginChild("MaterialDetailsPanel", ImVec2(DetailsWidth, ViewHeight), true);
			DrawMaterialDetailsPanel();
			ImGui::EndChild();
			break;
		default:
			break;
		}
	}
	ImGui::End();

	if (!bIsOpen)
	{
		DestroyPreviewResource();
	}
}

void FImguiPreviewEditorWindow::SaveAsset()
{
	if (!bIsDirty || !TargetMesh.IsValid())
	{
		return;
	}

	switch (prevType)
	{
	case EPrevType::Material:
	{
		if (PreviewMaterialInstance && !OriginalMatKey.empty())
		{
			if (auto OrigMat = FRenderResourceLibrary::Get().GetMaterial(OriginalMatKey))
			{
				//복제본의 모든 텍스처 슬롯을 원본에 복사
				const size_t SlotCount = static_cast<size_t>(EMaterialTextureSlot::Count);
				for (size_t i = 0; i < SlotCount; ++i)
				{
					const auto Slot = static_cast<EMaterialTextureSlot>(i);
					OrigMat->SetTexture(Slot, PreviewMaterialInstance->GetTexture(Slot));
				}

				// 델리게이트 브로드캐스트 (자체 및 참조 메시 썸네일 일괄 갱신)
				FRenderResourceLibrary::Get().OnMaterialSaved.Broadcast(OriginalMatKey);
			}
		}
		break;
	}
	case EPrevType::Mesh:
	{
		if (OriginalMesh && TargetMesh.IsValid())
		{
			const FString MeshKey = OriginalMesh->MeshId.ToString();

			// 변경된 슬롯들을 순회하며 라이브러리 의존성 테이블 갱신
			const size_t SlotCount = std::min(OriginalMesh->Materials.size(), TargetMesh->Materials.size());
			for (size_t i = 0; i < SlotCount; ++i)
			{
				const FString& OldMat = OriginalMesh->Materials[i];
				const FString& NewMat = TargetMesh->Materials[i];

				if (OldMat != NewMat)
				{
					FRenderResourceLibrary::Get().UpdateMeshMaterialDependency(MeshKey, OldMat, NewMat);
				}
			}

			// 복사본 데이터를 원본에 덮어쓰기
			OriginalMesh->Materials = TargetMesh->Materials;

			// 메시 썸네일 최종 갱신
			FRenderResourceLibrary::Get().UpdateMeshThumbnail(MeshKey);
		}
		break;
	}
	default:
		break;
	}

	bIsDirty = false;
}

void FImguiPreviewEditorWindow::DestroyPreviewResource()
{
	if (TargetMesh.IsValid())
	{
		TargetMesh->Destroy();
	}

	// 임시 머티리얼 인스턴스 해제
	PreviewMaterialInstance.reset();
	RenderTarget.Release();
}

void FImguiPreviewEditorWindow::ProcessViewportInput(FEditor& Editor, const ImVec2& ViewportPos, const ImVec2& ViewportSize, float DeltaTime)
{
	const ImVec2 MousePos = ImGui::GetMousePos();
	const bool bHovered = (MousePos.x >= ViewportPos.x && MousePos.x <= ViewportPos.x + ViewportSize.x &&
		MousePos.y >= ViewportPos.y && MousePos.y <= ViewportPos.y + ViewportSize.y);

	if (!bHovered && !ImGui::IsWindowFocused())
	{
		return;
	}

	if (bHovered && FInputManager::Get().IsKeyJustPressed('F'))
	{
		FocusOnMesh();
		return;
	}

	if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && (bHovered || ImGui::IsWindowFocused()))
	{
		const float Wheel = ImGui::GetIO().MouseWheel;
		if (Wheel != 0.0f)
		{
			CameraSpeed += Wheel * 0.1f;
			CameraSpeed = std::clamp(CameraSpeed, 1.0f, 15.0f);
		}

		CameraController.CameraRotateSpeed = Editor.State.GetCameraSensitivity();
		CameraController.CameraMoveSpeed = CameraSpeed;

		CameraController.UpdateMouseInput(PreviewViewport.ViewportCamera);
		CameraController.UpdateKeyInput(PreviewViewport.ViewportCamera, DeltaTime);
	}
}

void FImguiPreviewEditorWindow::DrawMeshDetailsPanel()
{
	if (!TargetMesh.IsValid())
	{
		ImGui::TextDisabled("No mesh selected");
		return;
	}

	ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "Static Mesh Details");
	ImGui::Separator();

	ImGui::Text("Name: %s", TargetMesh->MeshId.ToString().c_str());

	const TSharedPtr<FStaticMesh> MeshAsset = TargetMesh->GetStaticMeshAsset();
	if (MeshAsset)
	{
		ImGui::Spacing();
		ImGui::Text("Vertices: %u", MeshAsset->GetVertexCount());
		ImGui::Text("Triangles: %u", MeshAsset->GetIndexCount() / 3);

		const FAxisAlignedBoundingBox& Bounds = MeshAsset->GetLocalBounds();
		const FVector Size = Bounds.Max - Bounds.Min;
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Bounding Box");
		ImGui::Text("Size: %.1f, %.1f, %.1f", Size.X, Size.Y, Size.Z);
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "Materials");

	const auto& AllMaterialMap = FRenderResourceLibrary::Get().GetAllMaterials();
	TArray<FString> AvailableMaterials;
	AvailableMaterials.reserve(AllMaterialMap.size());
	for (const auto& [MatKey, _] : AllMaterialMap)
	{
		AvailableMaterials.push_back(MatKey);
	}
	std::sort(AvailableMaterials.begin(), AvailableMaterials.end());

	for (int SlotIdx = 0; SlotIdx < static_cast<int>(TargetMesh->Materials.size()); ++SlotIdx)
	{
		ImGui::PushID(SlotIdx);
		FString& CurrentSlotMat = TargetMesh->Materials[SlotIdx];

		ImGui::TextDisabled("Slot [%d]", SlotIdx);

		ID3D11ShaderResourceView* ThumbnailSRV = nullptr;
		if (auto MatTex = FRenderResourceLibrary::Get().GetMaterialThumbnail(CurrentSlotMat))
		{
			ThumbnailSRV = MatTex->GetSRV();
		}

		constexpr float ThumbWidth = 72.0f;
		constexpr float ThumbHeight = 72.0f;
		const ImTextureID TexId = reinterpret_cast<ImTextureID>(ThumbnailSRV);

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 2.0f));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
		ImGui::ImageButton("##MatThumb", TexId, ImVec2(ThumbWidth, ThumbHeight), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ContentDragPayloadType))
			{
				const auto* DragData = static_cast<const FContentDragPayload*>(Payload->Data);
				if (DragData && DragData->Kind == FContentDragPayload::EKind::Material)
				{
					CurrentSlotMat = DragData->Key;
					bIsDirty = true;
				}
			}
			ImGui::EndDragDropTarget();
		}

		const ImVec2 Min = ImGui::GetItemRectMin();
		const ImVec2 Max = ImGui::GetItemRectMax();
		constexpr float LineHeight = 3.5f;

		ImGui::GetWindowDrawList()->AddRectFilled(
			ImVec2(Min.x + 2.0f, Max.y - LineHeight - 2.0f),
			ImVec2(Max.x - 2.0f, Max.y - 2.0f),
			IM_COL32(46, 204, 113, 255)
		);

		ImGui::SameLine();

		const float YOffset = (ThumbHeight - ImGui::GetFrameHeight()) * 0.5f;
		if (YOffset > 0.0f)
		{
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + YOffset);
		}

		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::BeginCombo("##MatCombo", CurrentSlotMat.c_str()))
		{
			for (const FString& MatName : AvailableMaterials)
			{
				const bool bMatSelected = (CurrentSlotMat == MatName);
				if (ImGui::Selectable(MatName.c_str(), bMatSelected))
				{
					CurrentSlotMat = MatName;
					bIsDirty = true;
				}

				if (bMatSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::Spacing();
		ImGui::PopID();
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "Camera (Free Flight)");
	ImGui::Text("Pos: %.1f, %.1f, %.1f",
		PreviewViewport.ViewportCamera.Position.X,
		PreviewViewport.ViewportCamera.Position.Y,
		PreviewViewport.ViewportCamera.Position.Z);
	ImGui::Text("Yaw: %.1f, Pitch: %.1f",
		PreviewViewport.ViewportCamera.Yaw,
		PreviewViewport.ViewportCamera.Pitch);

	ImGui::Spacing();
	ImGui::TextDisabled("RMB + WASD: Fly Camera");
	ImGui::TextDisabled("Key F: Focus Mesh");

	if (ImGui::Button("Focus Mesh (F)", ImVec2(-1.0f, 25.0f)))
	{
		FocusOnMesh();
	}
}

void FImguiPreviewEditorWindow::DrawMaterialDetailsPanel()
{
	// 복제본 인스턴스(PreviewMaterialInstance) 유효성 검사
	if (!PreviewMaterialInstance)
	{
		ImGui::TextDisabled("No material preview instance available");
		return;
	}

	ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Material Details");
	ImGui::Separator();
	ImGui::Text("Material Name: %s", OriginalMatKey.c_str());

	ImGui::Spacing();
	ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Texture Parameters");
	ImGui::TextDisabled("Base Color / Diffuse");

	// PreviewMaterialInstance에서 텍스처 조회
	auto DiffuseTex = PreviewMaterialInstance->GetDiffuseMap();
	ID3D11ShaderResourceView* DiffuseSRV = DiffuseTex ? DiffuseTex->GetSRV() : nullptr;

	constexpr float ThumbWidth = 72.0f;
	constexpr float ThumbHeight = 72.0f;
	const ImTextureID TexId = reinterpret_cast<ImTextureID>(DiffuseSRV);

	ImGui::PushID("DiffuseSlot");

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 2.0f));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
	ImGui::ImageButton("##DiffuseThumb", TexId, ImVec2(ThumbWidth, ThumbHeight), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();

	// 텍스처 드래그 앤 드롭 수신
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ContentDragPayloadType))
		{
			const auto* DragData = static_cast<const FContentDragPayload*>(Payload->Data);
			if (DragData && DragData->Kind == FContentDragPayload::EKind::Texture)
			{
				std::filesystem::path FilePath(DragData->Path);
				std::string CleanKey = FilePath.stem().string();
				std::transform(CleanKey.begin(), CleanKey.end(), CleanKey.begin(), [](unsigned char c) {
					return static_cast<char>(std::tolower(c));
					});

				auto NewTex = FRenderResourceLibrary::Get().GetTexture(CleanKey);

				if (!NewTex)
				{
					if (auto Renderer = FRenderResourceLibrary::Get().GetRenderer())
					{
						NewTex = Renderer->CreateTexture(FilePath.wstring().c_str());
						if (NewTex)
						{
							FRenderResourceLibrary::Get().RegisterTexture(CleanKey, NewTex);
						}
					}
				}

				if (NewTex)
				{
					// 원본이 아닌 복제본(PreviewMaterialInstance)에 세팅
					PreviewMaterialInstance->SetDiffuseMap(NewTex);
					bIsDirty = true;
				}
			}
		}
		ImGui::EndDragDropTarget();
	}

	// 텍스처 인디케이터 바 (주황색)
	const ImVec2 Min = ImGui::GetItemRectMin();
	const ImVec2 Max = ImGui::GetItemRectMax();
	constexpr float LineHeight = 3.5f;

	ImGui::GetWindowDrawList()->AddRectFilled(
		ImVec2(Min.x + 2.0f, Max.y - LineHeight - 2.0f),
		ImVec2(Max.x - 2.0f, Max.y - 2.0f),
		IM_COL32(230, 126, 34, 255)
	);

	ImGui::SameLine();
	const float YOffset = (ThumbHeight - ImGui::GetFrameHeight()) * 0.5f;
	if (YOffset > 0.0f)
	{
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + YOffset);
	}

	if (DiffuseTex)
	{
		ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Texture Bound");
	}
	else
	{
		ImGui::TextDisabled("None (Drop Texture Here)");
	}

	ImGui::PopID();

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "Camera (Free Flight)");
	ImGui::Text("Pos: %.1f, %.1f, %.1f",
		PreviewViewport.ViewportCamera.Position.X,
		PreviewViewport.ViewportCamera.Position.Y,
		PreviewViewport.ViewportCamera.Position.Z);

	if (ImGui::Button("Focus Mesh (F)", ImVec2(-1.0f, 25.0f)))
	{
		FocusOnMesh();
	}
}