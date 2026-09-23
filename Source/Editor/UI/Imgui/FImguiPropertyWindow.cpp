#include "FImguiPropertyWindow.h"
#include "Runtime/CoreUObject/USceneComponent.h"
#include "Runtime/CoreUObject/UPrimitiveComponent.h"
#include "Runtime/CoreUObject/USpotLightComponent.h"
#include "Runtime/CoreUObject/UTextInstanceComponent.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/Actors/AActor.h"
#include "ThirdParty/Imgui/imgui.h"
#include "ThirdParty/Imgui/imgui_internal.h"
#include "ThirdParty/Imgui/imgui_impl_dx11.h"
#include "ThirdParty/Imgui/imgui_impl_win32.h"
#include <string>
#include "FImguiDragDrop.h"
#include "Runtime/Rendering/FMaterial.h"
#include "Runtime/Rendering/FRenderResourceLibrary.h"
#include "Runtime/CoreUObject/UStaticMeshComponent.h"
#include "Runtime/CoreUObject/UStaticMesh.h"
#include <algorithm>
#include "Runtime/Core/FObjectIterator.h"
#include "Runtime/Core/TObjectIterator.h"

void FImguiPropertyWindow::Process(FEditor& Editor)
{
	ImGui::Begin("Jungle Property Window");

	if (AActor* SelectedActor = Editor.GetSelectedActor())
	{
		ShowActorHeader(*SelectedActor);
		ImGui::Separator();

		ShowComponentHierarchy(*SelectedActor);
		ImGui::Separator();

		ShowComponentSections(Editor, *SelectedActor);
	}
	else
	{
		ImGui::TextDisabled("No selection");
	}

	ShowGizmoSettings(Editor);

	ImGui::End();
}

void FImguiPropertyWindow::ShowActorHeader(const AActor& Actor) const
{
	const char* ActorClassName = Actor.GetClass() ? Actor.GetClass()->GetDisplayName().c_str() : "None";
	ImGui::Text("Actor Class: %s", ActorClassName);
	ImGui::Text("Actor UUID: %u", Actor.GetUUID());
}

void FImguiPropertyWindow::ShowComponentHierarchy(const AActor& Actor) const
{
	ImGui::TextDisabled("Components Hierarchy");

	const USceneComponent* RootComp = Actor.GetRootComponent();
	if (RootComp)
	{
		const char* RootName = RootComp->GetClass() ? RootComp->GetClass()->GetDisplayName().c_str() : "RootComponent";
		ImGui::BulletText("[Root] %s (ID: %u)", RootName, RootComp->GetUUID());
	}

	for (const USceneComponent* Comp : Actor.GetAttachedComponents())
	{
		if (!Comp || Comp == RootComp)
		{
			continue;
		}

		const char* SubName = Comp->GetClass() ? Comp->GetClass()->GetDisplayName().c_str() : "SubComponent";
		ImGui::Indent(15.0f);
		ImGui::BulletText("└── [Sub] %s (ID: %u)", SubName, Comp->GetUUID());
		ImGui::Unindent(15.0f);
	}
}

void FImguiPropertyWindow::ShowComponentSections(FEditor& Editor, AActor& Actor)
{
	USceneComponent* RootComp = Actor.GetRootComponent();

	for (USceneComponent* Comp : Actor.GetAttachedComponents())
	{
		if (!Comp)
		{
			continue;
		}

		const bool bIsRoot = (Comp == RootComp);
		const char* CompTypeName = Comp->GetClass() ? Comp->GetClass()->GetDisplayName().c_str() : "Component";

		// ### 뒤쪽이 실제 ID 라서, 앞의 표시 이름이 바뀌어도 접힘 상태가 유지된다.
		std::string SectionTitle = (bIsRoot ? "[Root] " : "[Sub] ") + std::string(CompTypeName)
			+ " (ID: " + std::to_string(Comp->GetUUID()) + ")###CompHeader_" + std::to_string(Comp->GetUUID());

		if (!ImGui::CollapsingHeader(SectionTitle.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			continue;
		}

		// 컴포넌트마다 위젯 ID 를 분리해야 같은 라벨끼리 충돌하지 않는다.
		ImGui::PushID(Comp);
		ShowComponentDetails(Editor, Actor, *Comp, bIsRoot);
		ImGui::PopID();

		ImGui::Spacing();
	}
}

void FImguiPropertyWindow::ShowComponentDetails(FEditor& Editor, AActor& Actor,
	USceneComponent& Comp, bool bIsRoot)
{
	ShowTransform(Editor, Comp, bIsRoot);

	if (Comp.IsA<UStaticMeshComponent>())
	{
		ShowStaticMeshSettings(static_cast<UStaticMeshComponent&>(Comp));
	}

	if (Comp.IsA<UTextInstanceComponent>())
	{
		ShowTextSettings(static_cast<UTextInstanceComponent&>(Comp));
	}

	if (Comp.IsA<USpotLightComponent>())
	{
		ShowSpotLightSettings(static_cast<USpotLightComponent&>(Comp));
	}
	else if (Comp.IsA<UMeshComponent>())
	{
		ShowPrimitiveSettings(Actor, static_cast<UMeshComponent&>(Comp), bIsRoot);
	}
}


void FImguiPropertyWindow::ShowStaticMeshSettings(UStaticMeshComponent& StaticMeshComp) const {
	ImGui::Separator();
	ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.6f, 1.0f), "Static Mesh Settings");

	const auto& AllUStaticMeshMap = FRenderResourceLibrary::Get().GetAllUStaticMeshMap();

	FObjectIterator AnyMesh(UStaticMesh::StaticClass());
	if (!AnyMesh)
	{
		ImGui::TextDisabled("No Static Meshes available");
		return;
	}

	UStaticMesh* CurrentStaticMesh = StaticMeshComp.GetStaticMesh();
	FString CurrentMeshName = CurrentStaticMesh ? CurrentStaticMesh->MeshId.ToString() : "None";

	// 스태틱 메시 썸네일 SRV 조회
	ID3D11ShaderResourceView* MeshThumbnailSRV = nullptr;
	if (CurrentStaticMesh)
	{
		if (auto MeshTex = FRenderResourceLibrary::Get().GetMeshThumbnail(CurrentStaticMesh->MeshId))
		{
			MeshThumbnailSRV = MeshTex->GetSRV();
		}
	}

	// 썸네일 이미지 버튼 
	constexpr float ThumbWidth = 72.0f;
	constexpr float ThumbHeight = 72.0f;
	const ImTextureID TexId = reinterpret_cast<ImTextureID>(MeshThumbnailSRV);

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 2.0f));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
	ImGui::ImageButton("##StaticMeshThumb", TexId, ImVec2(ThumbWidth, ThumbHeight), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();

	// 3. [드래그 앤 드롭 수신] 썸네일 위에 스태틱 메시 놓았을 때 교체
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ContentDragPayloadType))
		{
			const auto* DragData = static_cast<const FContentDragPayload*>(Payload->Data);
			if (DragData && DragData->Kind == FContentDragPayload::EKind::Mesh)
			{
				auto it = AllUStaticMeshMap.find(DragData->Key);
				if (it != AllUStaticMeshMap.end() && it->second)
				{
					StaticMeshComp.SetStaticMesh(it->second);
					// 새 메시의 기본 머티리얼 목록으로 컴포넌트 슬롯 초기화
					for (int32 i = 0; i < static_cast<int32>(it->second->Materials.size()); ++i)
					{
						StaticMeshComp.SetMaterial(i, FName(it->second->Materials[i]));
					}
					CurrentStaticMesh = it->second;
					CurrentMeshName = CurrentStaticMesh->MeshId.ToString();
				}
			}
		}
		ImGui::EndDragDropTarget();
	}

	// 썸네일 하단 스태틱 메시 인디케이터
	const ImVec2 Min = ImGui::GetItemRectMin();
	const ImVec2 Max = ImGui::GetItemRectMax();
	constexpr float LineHeight = 3.5f;

	ImGui::GetWindowDrawList()->AddRectFilled(
		ImVec2(Min.x + 2.0f, Max.y - LineHeight - 2.0f),
		ImVec2(Max.x - 2.0f, Max.y - 2.0f),
		IM_COL32(52, 152, 219, 255)
	);

	// 우측 콤보박스 배치 및 세로 중앙 정렬
	ImGui::SameLine();
	const float YOffset = (ThumbHeight - ImGui::GetFrameHeight()) * 0.5f;
	if (YOffset > 0.0f)
	{
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + YOffset);
	}

	ImGui::SetNextItemWidth(-1.0f);
	if (ImGui::BeginCombo("##StaticMeshCombo", CurrentMeshName.c_str()))
	{
		TArray<UStaticMesh*> Meshes;
		Meshes.reserve(AllUStaticMeshMap.size());
		for (const auto& [Key, MeshPtr] : AllUStaticMeshMap) //정해진 ustaticmeshmap 만 순회하기 위해서
		{
			if (MeshPtr)
			{
				Meshes.push_back(MeshPtr);
			}
		}

		std::sort(Meshes.begin(), Meshes.end(),
			[](const UStaticMesh* A, const UStaticMesh* B)
			{
				return A->MeshId.Compare(B->MeshId) < 0;
			});

		for (UStaticMesh* Mesh : Meshes)
		{
			ImGui::PushID(static_cast<int>(Mesh->GetUUID()));

			const FString ItemName = Mesh->MeshId.ToString();
			const bool bIsSelected = (Mesh == CurrentStaticMesh);

			if (ImGui::Selectable(ItemName.c_str(), bIsSelected))
			{
				StaticMeshComp.SetStaticMesh(Mesh);
				// 새 메시의 기본 머티리얼 목록으로 컴포넌트 슬롯 초기화
				for (int32 i = 0; i < static_cast<int32>(Mesh->Materials.size()); ++i)
				{
					StaticMeshComp.SetMaterial(i, FName(Mesh->Materials[i]));
				}
				CurrentStaticMesh = Mesh;
				CurrentMeshName = ItemName;
			}

			if (bIsSelected)
			{
				ImGui::SetItemDefaultFocus();
			}

			ImGui::PopID();
		}
		ImGui::EndCombo();
	}

	// 콤보박스 영역 드롭 교체
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ContentDragPayloadType))
		{
			const auto* DragData = static_cast<const FContentDragPayload*>(Payload->Data);
			if (DragData && DragData->Kind == FContentDragPayload::EKind::Mesh)
			{
				auto it = AllUStaticMeshMap.find(DragData->Key);
				if (it != AllUStaticMeshMap.end() && it->second)
				{
					StaticMeshComp.SetStaticMesh(it->second);
					// 새 메시의 기본 머티리얼 목록으로 컴포넌트 슬롯 초기화
					for (int32 i = 0; i < static_cast<int32>(it->second->Materials.size()); ++i)
					{
						StaticMeshComp.SetMaterial(i, FName(it->second->Materials[i]));
					}
					CurrentStaticMesh = it->second;
					CurrentMeshName = CurrentStaticMesh->MeshId.ToString();
				}
			}
		}
		ImGui::EndDragDropTarget();
	}

	// 머티리얼 슬롯 리스트 및 변경 콤보 박스
	if (CurrentStaticMesh)
	{
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "Materials");

		const auto& AllMaterialMap = FRenderResourceLibrary::Get().GetAllMaterials();

		TArray<FString> AvailableMaterials;
		AvailableMaterials.reserve(AllMaterialMap.size());
		for (const auto& [MatKey, _] : AllMaterialMap)
		{
			AvailableMaterials.push_back(MatKey);
		}
		std::sort(AvailableMaterials.begin(), AvailableMaterials.end());

		const int32 SlotCount = static_cast<int32>(CurrentStaticMesh->Materials.size());
		for (int32 SlotIdx = 0; SlotIdx < SlotCount; ++SlotIdx)
		{
			ImGui::PushID(SlotIdx);

			const FString CurrentSlotMat = StaticMeshComp.GetMaterial(SlotIdx).ToString();

			ImGui::TextDisabled("Slot [%d]", SlotIdx);

			ID3D11ShaderResourceView* ThumbnailSRV = nullptr;
			if (auto MatTex = FRenderResourceLibrary::Get().GetMaterialThumbnail(CurrentSlotMat))
			{
				ThumbnailSRV = MatTex->GetSRV();
			}

			constexpr float ThumbSize = 85.0f;
			const ImTextureID MatTexId = reinterpret_cast<ImTextureID>(ThumbnailSRV);

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
			ImGui::ImageButton("##MatThumb", MatTexId, ImVec2(ThumbSize, ThumbSize), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::PopStyleColor();

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ContentDragPayloadType))
				{
					const auto* DragData = static_cast<const FContentDragPayload*>(Payload->Data);
					if (DragData && DragData->Kind == FContentDragPayload::EKind::Material)
					{
						StaticMeshComp.SetMaterial(SlotIdx, FName(DragData->Key));
					}
				}
				ImGui::EndDragDropTarget();
			}

			const ImGuiStyle& Style = ImGui::GetStyle();
			const ImVec2 MatMin = ImGui::GetItemRectMin();
			const ImVec2 MatMax = ImGui::GetItemRectMax();
			constexpr float MatLineHeight = 3.0f;

			ImGui::GetWindowDrawList()->AddRectFilled(
				ImVec2(MatMin.x + Style.FramePadding.x, MatMax.y - Style.FramePadding.y - MatLineHeight),
				ImVec2(MatMax.x - Style.FramePadding.x, MatMax.y - Style.FramePadding.y),
				IM_COL32(46, 204, 113, 255)
			);

			ImGui::SameLine();

			const float MatYOffset = (ThumbSize - ImGui::GetFrameHeight()) * 0.5f;
			if (MatYOffset > 0.0f)
			{
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + MatYOffset);
			}

			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::BeginCombo("##MatCombo", CurrentSlotMat.c_str()))
			{
				for (const FString& MatName : AvailableMaterials)
				{
					const bool bMatSelected = (CurrentSlotMat == MatName);
					if (ImGui::Selectable(MatName.c_str(), bMatSelected))
					{
						StaticMeshComp.SetMaterial(SlotIdx, FName(MatName));
					}

					if (bMatSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ContentDragPayloadType))
				{
					const auto* DragData = static_cast<const FContentDragPayload*>(Payload->Data);
					if (DragData && DragData->Kind == FContentDragPayload::EKind::Material)
					{
						StaticMeshComp.SetMaterial(SlotIdx, FName(DragData->Key));
					}
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::Spacing();
			ImGui::PopID();
		}
	}

	// UV 애니메이션 토글
	ImGui::Spacing();
	ImGui::Checkbox("UV Scroll (bIsMovingUV)", &StaticMeshComp.bIsMovingUV);
}

void FImguiPropertyWindow::ShowTransform(FEditor& Editor, USceneComponent& Comp, bool bIsRoot) const
{
	ImGui::TextDisabled("Transform");

	if (bIsRoot)
	{
		// 루트 컴포넌트 트랜스폼은 에디터 기즈모와 동기화
		ImGui::DragFloat3("Translation", &Editor.SelectedTransform.Location.X, 0.01f);
		if (ImGui::DragFloat3("Rotation (deg)", &Editor.SelectedEulerDegDisplay.X, 0.5f))
		{
			Editor.SelectedTransform.Rotation = FQuaternion::FromEulerXYZDeg(Editor.SelectedEulerDegDisplay);
		}
		ImGui::DragFloat3("Scale", &Editor.SelectedTransform.Scale3D.X, 0.01f);
		return;
	}

	// 서브 컴포넌트 상대 트랜스폼 편집
	if (auto* PrimComp = Comp.Cast<UPrimitiveComponent>())
	{
		FTransform& RelTransform = PrimComp->GetRelativeTransform();
		ImGui::DragFloat3("Rel Location", &RelTransform.Location.X, 0.01f);

		FVector RelEuler = RelTransform.Rotation.ToEulerXYZDeg();
		if (ImGui::DragFloat3("Rel Rotation (deg)", &RelEuler.X, 0.5f))
		{
			RelTransform.Rotation = FQuaternion::FromEulerXYZDeg(RelEuler);
		}
		ImGui::DragFloat3("Rel Scale", &RelTransform.Scale3D.X, 0.01f);
	}
}

void FImguiPropertyWindow::ShowTextSettings(UTextInstanceComponent& TextComp) const
{
	ImGui::Separator();
	ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Text Settings");


	// 아틀라스 텍스처가 있는 폰트만 노출한다. 폰트를 추가하려면 텍스처와 Instance_Text_* 머티리얼을 함께 등록할 것.
	const char* fontItems[] = { "maplestorybold" };
	const FName Materials[] = { FName("Instance_Text_Maple") };

	// 선택된 컴포넌트의 현재 머티리얼로부터 콤보 인덱스를 구한다. (컴포넌트마다 폰트가 다를 수 있음)
	int currFontIndex = 0;
	const FName CurrentMaterial = TextComp.GetMaterialID();
	for (int i = 0; i < IM_ARRAYSIZE(Materials); ++i)
	{
		if (Materials[i] == CurrentMaterial)
		{
			currFontIndex = i;
			break;
		}
	}

	if (ImGui::Combo("Font", &currFontIndex, fontItems, IM_ARRAYSIZE(fontItems)))
	{
		const char* selectedFont = fontItems[currFontIndex];
		TextComp.SetMaterialID((Materials[currFontIndex]));
		TextComp.SetFont(FName(selectedFont));
	}

	static char utfBuffer[512]{};
	WideCharToMultiByte(CP_UTF8, 0, TextComp.GetText().c_str(), -1, &utfBuffer[0], sizeof(utfBuffer), NULL, NULL);

	if (ImGui::InputText("Text Content", &utfBuffer[0], sizeof(utfBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
	{
		FString Buffer{ &utfBuffer[0] };
		uint32 convertResult = MultiByteToWideChar(CP_UTF8, 0, Buffer.c_str(), Buffer.length(), NULL, 0);
		FWString newText(convertResult, 0);
		MultiByteToWideChar(CP_UTF8, 0, Buffer.c_str(), Buffer.length(), newText.data(), convertResult);
		TextComp.SetText(newText);
	}
}

void FImguiPropertyWindow::ShowSpotLightSettings(USpotLightComponent& LightComp) const
{
	ImGui::Separator();
	ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Spot Light Settings");

	FVector LightCol = LightComp.GetLightColor();
	if (ImGui::ColorEdit3("Light Color", &LightCol.X))
	{
		LightComp.SetLightColor(LightCol);
	}

	float LightIntensity = LightComp.GetIntensity();
	if (ImGui::DragFloat("Intensity", &LightIntensity, 0.05f, 0.0f, 50.0f))
	{
		LightComp.SetIntensity(LightIntensity);
	}

	float SpotAngle = LightComp.GetSpotAngle();
	if (ImGui::SliderFloat("Spot Angle", &SpotAngle, 1.0f, 89.0f))
	{
		LightComp.SetSpotAngle(SpotAngle);
	}

	float LightRange = LightComp.GetRange();
	if (ImGui::DragFloat("Range", &LightRange, 0.1f, 0.1f, 100.0f))
	{
		LightComp.SetRange(LightRange);
	}
}

void FImguiPropertyWindow::ShowPrimitiveSettings(AActor& Actor, UMeshComponent& MeshComp,
	bool bIsRoot) const
{
	ImGui::Separator();
	ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Primitive Settings");

	FVector CurrentColor = MeshComp.GetColor();
	if (ImGui::ColorEdit3("Color", &CurrentColor.X))
	{
		MeshComp.SetColor(CurrentColor);
		if (bIsRoot)
		{
			Actor.SetColor(CurrentColor);
		}
	}

	ShowTextureSlot(MeshComp);
}

void FImguiPropertyWindow::ShowTextureSlot(UMeshComponent& MeshComp) const
{
	constexpr float SlotSize = 64.0f;
	TSharedPtr<FMaterial> Material = FRenderResourceLibrary::Get().GetMaterial(MeshComp.GetPureRenderData().MaterialId);
	TSharedPtr<FTexture> CurrentTexture = Material ? Material->GetTexture() : nullptr;

	ImGui::Spacing();
	ImGui::TextDisabled("Texture");

	if (CurrentTexture && CurrentTexture->GetSRV())
	{
		// ImGui 1.93의 ImTextureID는 ImU64라서 포인터를 정수로 한 번 거친다.
		const ImTextureID TexId = static_cast<ImTextureID>(
			reinterpret_cast<intptr_t>(CurrentTexture->GetSRV()));
		ImGui::Image(TexId, ImVec2(SlotSize, SlotSize));
	}
	else
	{
		// 비어 있어도 드롭받을 아이템은 있어야 하므로 자리를 만든다.
		ImGui::Button("No\nTexture", ImVec2(SlotSize, SlotSize));
	}

	// 드롭 타깃은 아이템을 그린 직후여야 한다.
	if (!ImGui::BeginDragDropTarget())
	{
		return;
	}

	if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ContentDragPayloadType))
	{
		// 타입 이름이 같아도 크기가 다르면 다른 구조체일 수 있다.
		if (Material && Payload->DataSize == static_cast<int>(sizeof(FContentDragPayload)))
		{
			const auto* Dropped = static_cast<const FContentDragPayload*>(Payload->Data);

			if (Dropped->Kind == FContentDragPayload::EKind::Texture)
			{
				Material->SetTextureByName(Dropped->Key);
			}
		}
	}
	ImGui::EndDragDropTarget();
}

void FImguiPropertyWindow::ShowGizmoSettings(FEditor& Editor) const
{
	static const char* GizmoModes[4] = { "None", "Translation", "Rotation", "Scale" };
	int SelectedItem = static_cast<int>(Editor.GetGizmo().Mode);
	if (ImGui::Combo("Gizmo Mode", &SelectedItem, GizmoModes, 4))
	{
		Editor.GetGizmo().Mode = static_cast<EGizmoMode>(SelectedItem);
	}

	if (SelectedItem == 3) // Scale
	{
		static const char* GizmoSpaces[] = { "Local" };
		SelectedItem = static_cast<int>(Editor.GetGizmo().GetSpace()) - 1;
		if (ImGui::Combo("Gizmo Space", &SelectedItem, GizmoSpaces, 1))
		{
			Editor.GetGizmo().SetGizmoSpace(static_cast<EGizmoSpace>(SelectedItem - 1));
		}
	}
	else if (SelectedItem != 0) // Translation, Rotation
	{
		static const char* GizmoSpaces[] = { "World", "Local" };
		SelectedItem = static_cast<int>(Editor.GetGizmo().GetSpace());
		if (ImGui::Combo("Gizmo Space", &SelectedItem, GizmoSpaces, 2))
		{
			Editor.GetGizmo().SetGizmoSpace(static_cast<EGizmoSpace>(SelectedItem));
		}
	}
}
