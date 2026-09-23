#include "FImguiContentsDrawer.h"
#include "ThirdParty/Imgui/imgui.h"
#include "ThirdParty/Imgui/imgui_internal.h"
#include "ThirdParty/Imgui/imgui_impl_dx11.h"
#include "ThirdParty/Imgui/imgui_impl_win32.h"
#include "Runtime/Core/FString.h"
#include "Runtime/Core/Log.h"
#include "Runtime/Engine/FRenderView.h"
#include "Runtime/Rendering/FRenderResourceLibrary.h"
#include "Runtime/Rendering/FRenderer.h"
#include "Runtime/Rendering/FTexture.h"
#include "ThirdParty/stb/stb_image.h"
#include <algorithm>
#include <cctype>
#include "FImguiDragDrop.h"
#include "Editor/Application/FEditorApplication.h"

constexpr uint32_t HashStr(const char* str, uint32_t hash = 2166136261u)
{
	return *str ? HashStr(str + 1, (hash ^ static_cast<uint8_t>(*str)) * 16777619u) : hash;
}

FImguiContentsDrawer::FImguiContentsDrawer() : LeftPanelWidth(200.0f)
{
	RootPath = GetResourcesDirectory();
	CurrentPath = RootPath;

	// These folders expose in-memory assets and must exist before navigation.
	if (!RootPath.empty())
	{
		for (const auto* FolderName : { L"StaticMesh", L"Materials" })
		{
			const auto FolderPath = RootPath / FolderName;
			std::error_code Error;
			std::filesystem::create_directories(FolderPath, Error);
			if (Error)
			{
				UE_LOG_WARN("[Content Drawer] Failed to create folder: %s (%s)",
					FolderPath.string().c_str(), Error.message().c_str());
			}
		}
	}
}

void FImguiContentsDrawer::Process(FEditor& Editor)
{
	ImGui::Begin("Content Drawer");
	if (RootPath.empty())
	{
		ImGui::TextUnformatted("Resources directory not found.");
		ImGui::End();
		return;
	}

	// GetContentRegionAvail은 Begin 다음에 불러야 이 창의 남은 영역이 나온다.
	// Begin 이전에 부르면 직전 창의 값이라 자식 패널이 창 밖으로 삐져나간다.
	ImVec2 ContentSize = ImGui::GetContentRegionAvail();

	ImGui::BeginChild("LeftPanel", ImVec2(LeftPanelWidth, ContentSize.y), true);
	RenderFolderTree();
	ImGui::EndChild();

	ImGui::SameLine();

	// 폭 0은 남은 공간을 전부 쓰라는 뜻
	ImGui::BeginChild("RightPanel", ImVec2(0.0f, ContentSize.y), true);
	RenderContentView();
	ImGui::EndChild();

	ImGui::End();
}

void FImguiContentsDrawer::RefreshEntries()
{
	Entries.clear();
	CachedPath = CurrentPath;
	bNeedsRefresh = false;

	// StaticMesh 폴더일 때는 라이브러리의 썸네일 맵 기준 목록 표시
	if (CurrentPath.filename() == "StaticMesh")
	{
		std::error_code Ec;
		if (!std::filesystem::exists(CurrentPath, Ec))
		{
			std::filesystem::create_directories(CurrentPath, Ec);
		}

		for (const auto& [Key, Thumb] : FRenderResourceLibrary::Get().GetAllMeshThumbnailMap())
		{
			if (Key == "Sphere_Mat") // 머터리얼 전용 ustaticmesh
			{
				continue;
			}

			FContentEntry Item;
			Item.DisplayName = Key.ToString();
			Item.Extension = ".staticmesh";
			Item.bIsDirectory = false;
			Item.Path = CurrentPath / (Key.ToString() + ".staticmesh");
			Entries.push_back(std::move(Item));
		}
		std::sort(Entries.begin(), Entries.end(),
			[](const FContentEntry& A, const FContentEntry& B)
			{
				return A.DisplayName < B.DisplayName;
			});
		return;
	}

	if (CurrentPath.filename() == "Materials")
	{
		std::error_code Ec;
		if (!std::filesystem::exists(CurrentPath, Ec))
		{
			std::filesystem::create_directories(CurrentPath, Ec);
		}

		for (const auto& [Key, Thumb] : FRenderResourceLibrary::Get().GetAllMaterialThumbnailMap())
		{
			FContentEntry Item;
			Item.DisplayName = Key;
			Item.Extension = ".material";
			Item.bIsDirectory = false;
			Item.Path = CurrentPath / (Key + ".material");
			Entries.push_back(std::move(Item));
		}
		std::sort(Entries.begin(), Entries.end(),
			[](const FContentEntry& A, const FContentEntry& B)
			{
				return A.DisplayName < B.DisplayName;
			});
		return;
	}

	std::error_code Ec;
	for (const auto& Entry : std::filesystem::directory_iterator(CurrentPath, Ec))
	{
		FContentEntry Item;
		Item.Path = Entry.path();
		Item.bIsDirectory = Entry.is_directory(Ec);
		Item.DisplayName = WideToUTF8(Entry.path().filename().wstring());

		if (!Item.bIsDirectory)
		{
			Item.Extension = WideToUTF8(Entry.path().extension().wstring());
			std::transform(Item.Extension.begin(), Item.Extension.end(), Item.Extension.begin(),
				[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		}

		Entries.push_back(std::move(Item));
	}

	// 폴더 먼저, 그 다음 파일. 각각 이름순.
	std::sort(Entries.begin(), Entries.end(),
		[](const FContentEntry& A, const FContentEntry& B)
		{
			if (A.bIsDirectory != B.bIsDirectory) { return A.bIsDirectory; }
			return A.DisplayName < B.DisplayName;
		});
}

TSharedPtr<FTexture> FImguiContentsDrawer::GetOrLoadThumbnail(const FContentEntry& Item)
{
	if (Item.bIsDirectory)
	{
		return nullptr;
	}
	if (Item.Extension != ".dds" && Item.Extension != ".png" && Item.Extension != ".jpg" && Item.Extension != ".jpeg")
	{
		return nullptr;
	}

	FString Key = Item.Path.stem().string();
	std::transform(Key.begin(), Key.end(), Key.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	FRenderResourceLibrary& Lib = FRenderResourceLibrary::Get();
	if (TSharedPtr<FTexture> Existing = Lib.GetTexture(Key))
	{
		return Existing;
	}

	if (LoadsThisFrame >= MaxLoadsPerFrame)
	{
		return nullptr;
	}

	FRenderer* Renderer = Lib.GetRenderer();
	if (!Renderer)
	{
		return nullptr;
	}

	++LoadsThisFrame;

	TSharedPtr<FTexture> Texture = Renderer->CreateTexture(Item.Path.wstring().c_str());
	Lib.RegisterTexture(Key, Texture);
	return Texture;
}

void FImguiContentsDrawer::RenderContentView()
{
	LoadsThisFrame = 0;

	if (bNeedsRefresh || CachedPath != CurrentPath)
	{
		RefreshEntries();
	}

	// ---------------- 상단 바 ----------------
	const std::filesystem::path Relative = std::filesystem::relative(CurrentPath, RootPath.parent_path());
	ImGui::TextUnformatted(WideToUTF8(Relative.wstring()).c_str());

	ImGui::SameLine();
	if (ImGui::SmallButton("Refresh")) { bNeedsRefresh = true; }

	if (CurrentPath != RootPath)
	{
		ImGui::SameLine();
		if (ImGui::SmallButton("Up")) { CurrentPath = CurrentPath.parent_path(); }
	}


	ImGui::SameLine();
	if (ImGui::SmallButton("Build / Reimport Meshes"))
	{
		FRenderResourceLibrary::Get().CreateObjMeshes();
		FRenderResourceLibrary::Get().CreateMeshThumbnails();
		FRenderResourceLibrary::Get().CreateMaterialThumbnails();
		bNeedsRefresh = true; // 완료 후 목록 갱신
	}


	ImGui::Separator();

	if (Entries.empty())
	{
		ImGui::TextDisabled("비어 있습니다.");
		return;
	}

	// ---------------- 타일 그리드 ----------------
	const ImGuiStyle& Style = ImGui::GetStyle();
	const float WindowVisibleX2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

	std::filesystem::path PendingNavigate;

	for (int Index = 0; Index < static_cast<int>(Entries.size()); ++Index)
	{
		const FContentEntry& Item = Entries[Index];

		ImGui::PushID(Index);
		ImGui::BeginGroup();

		const bool bSelected = (SelectedPath == Item.Path);

		ID3D11ShaderResourceView* DisplaySRV = nullptr;

		if (Item.bIsDirectory)
		{
			if (auto FolderTex = FRenderResourceLibrary::Get().GetEditTexture("foldericon"))
			{
				DisplaySRV = FolderTex->GetSRV();
			}
		}
		else if (Item.Extension == ".staticmesh")
		{
			const FName MeshKey(Item.DisplayName);
			if (auto MeshTex = FRenderResourceLibrary::Get().GetMeshThumbnail(MeshKey))
			{
				DisplaySRV = MeshTex->GetSRV();
			}
		}
		else if (Item.Extension == ".material")
		{
			if (auto MatTex = FRenderResourceLibrary::Get().GetMaterialThumbnail(Item.DisplayName))
			{
				DisplaySRV = MatTex->GetSRV();
			}
		}
		else
		{
			if (const TSharedPtr<FTexture> Thumbnail = GetOrLoadThumbnail(Item))
			{
				DisplaySRV = Thumbnail->GetSRV();
			}
		}

		if (DisplaySRV)
		{
			const ImGuiStyle& S = ImGui::GetStyle();
			ImGui::PushStyleColor(ImGuiCol_Button,
				bSelected ? S.Colors[ImGuiCol_ButtonActive] : ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

			const ImTextureID TexId =
				static_cast<ImTextureID>(reinterpret_cast<intptr_t>(DisplaySRV));

			if (ImGui::ImageButton("##thumb", TexId, ImVec2(ThumbnailSize, ThumbnailSize)))
			{
				SelectedPath = Item.Path;
			}

			ImU32 BarColor = IM_COL32(0, 0, 0, 0);

			switch (HashStr(Item.Extension.c_str()))
			{
			case HashStr(".material"):
				BarColor = IM_COL32(46, 204, 113, 255); // 머티리얼 (초록색)
				break;

			case HashStr(".staticmesh"):
			case HashStr(".mesh"):
			case HashStr(".obj"):
				BarColor = IM_COL32(52, 152, 219, 255); // 스태틱 메시 (파란색)
				break;

			default:
				break;
			}

			// [수정] Min, Max 변수 선언 및 패딩 오프셋 보정 적용
			const ImVec2 Min = ImGui::GetItemRectMin();
			const ImVec2 Max = ImGui::GetItemRectMax();

			if ((BarColor & IM_COL32_A_MASK) != 0)
			{
				const float LineMinX = Min.x + Style.FramePadding.x;
				const float LineMaxX = Max.x - Style.FramePadding.x;
				const float LineMaxY = Max.y - Style.FramePadding.y;
				constexpr float LineHeight = 3.0f;

				ImGui::GetWindowDrawList()->AddRectFilled(
					ImVec2(LineMinX, LineMaxY - LineHeight),
					ImVec2(LineMaxX, LineMaxY),
					BarColor
				);
			}

			ImGui::PopStyleColor();
		}
		else
		{
			const char* Caption = Item.bIsDirectory
				? "[DIR]"
				: (Item.Extension.empty() ? "FILE" : Item.Extension.c_str() + 1);

			if (ImGui::Selectable(Caption, bSelected, ImGuiSelectableFlags_AllowDoubleClick,
				ImVec2(ThumbnailSize, ThumbnailSize)))
			{
				SelectedPath = Item.Path;
			}
		}

		if (!Item.bIsDirectory && ImGui::BeginDragDropSource())
		{
			FContentDragPayload DragData;

			// [수정] .material 확장자 분기 추가
			if (Item.Extension == ".material")
			{
				DragData.Kind = FContentDragPayload::EKind::Material;
			}
			else if (Item.Extension == ".staticmesh")
			{
				DragData.Kind = FContentDragPayload::EKind::Mesh;
			}
			else
			{
				DragData.Kind = DisplaySRV ? FContentDragPayload::EKind::Texture : FContentDragPayload::EKind::Unknown;
			}

			const FString PathUtf8 = WideToUTF8(Item.Path.wstring());
			std::snprintf(DragData.Path, sizeof(DragData.Path), "%s", PathUtf8.c_str());

			FString Key = Item.DisplayName;
			std::snprintf(DragData.Key, sizeof(DragData.Key), "%s", Key.c_str());

			ImGui::SetDragDropPayload(ContentDragPayloadType, &DragData, sizeof(DragData));

			if (DisplaySRV)
			{
				const ImTextureID PreviewId =
					static_cast<ImTextureID>(reinterpret_cast<intptr_t>(DisplaySRV));
				ImGui::Image(PreviewId, ImVec2(48.0f, 48.0f));
				ImGui::SameLine();
			}
			ImGui::TextUnformatted(Item.DisplayName.c_str());

			ImGui::EndDragDropSource();
		}

		if (Item.bIsDirectory && ImGui::IsItemHovered() &&
			ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			PendingNavigate = Item.Path;
		}
		else if (!Item.bIsDirectory && ImGui::IsItemHovered() &&
			ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			if (Item.Extension == ".staticmesh")
			{
				const FName MeshId(Item.DisplayName);
				UStaticMesh* Mesh = FRenderResourceLibrary::Get().GetUStaticMesh(MeshId);
				if (Mesh)
				{
					FEditorApplication::Get().OpenPreviewWindow(Mesh, EPrevType::Mesh);
				}
			}
			else if (Item.Extension == ".material")
			{
				UStaticMesh* Mesh = FRenderResourceLibrary::Get().GetUStaticMesh("Sphere_Mat");
				Mesh->Materials[0] = Item.DisplayName;

				FEditorApplication::Get().OpenPreviewWindow(Mesh, EPrevType::Material);
			}
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", Item.DisplayName.c_str());
		}

		// 이름이 길면 썸네일 폭 안에서 줄바꿈
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ThumbnailSize);
		ImGui::TextUnformatted(Item.DisplayName.c_str());
		ImGui::PopTextWrapPos();

		ImGui::EndGroup();
		ImGui::PopID();

		// [수정] 우측 경계선을 넘지 않을 때만 SameLine() 호출하여 잘림 방지
		const float LastItemX2 = ImGui::GetItemRectMax().x;
		const float NextItemX2 = LastItemX2 + Style.ItemSpacing.x + ThumbnailSize;

		if (Index + 1 < static_cast<int>(Entries.size()))
		{
			if (NextItemX2 < WindowVisibleX2)
			{
				ImGui::SameLine();
			}
		}
	}

	if (!PendingNavigate.empty())
	{
		CurrentPath = PendingNavigate;
	}
}

void FImguiContentsDrawer::RenderFolderTree()
{
	ImGui::Text("Folders");
	ImGui::Separator();

	if (std::filesystem::exists(RootPath))
	{
		RenderFolderTreeNode(RootPath);
	}
}

void FImguiContentsDrawer::RenderFolderTreeNode(const std::filesystem::path& FolderPath) {
	// 루트 폴더일 때 필수 폴더들이 없으면 자동 생성
	if (FolderPath == RootPath)
	{
		std::error_code Ec;
		std::filesystem::create_directories(RootPath / "StaticMesh", Ec);
		std::filesystem::create_directories(RootPath / "Materials", Ec);
		std::filesystem::create_directories(RootPath / "Assets" / "Bins", Ec);
	}

	FString folderName = FolderPath == RootPath ? "All" : WideToUTF8(FolderPath.filename().wstring());

	std::error_code Ec;
	bool bHasSubFolder = false;
	for (const auto& Entry : std::filesystem::directory_iterator(FolderPath, Ec))
	{
		if (Entry.is_directory(Ec))
		{
			bHasSubFolder = true;
			break;
		}
	}

	ImGuiTreeNodeFlags Flags =
		ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_SpanAvailWidth;

	if (!bHasSubFolder)
	{
		Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}
	if (CurrentPath == FolderPath)
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}
	if (FolderPath == RootPath)
	{
		Flags |= ImGuiTreeNodeFlags_DefaultOpen;
	}

	ImGui::PushID(WideToUTF8(FolderPath.wstring()).c_str());

	const bool bOpened = ImGui::TreeNodeEx(folderName.c_str(), Flags);

	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
	{
		CurrentPath = FolderPath;
	}

	if (bOpened && bHasSubFolder)
	{
		for (const auto& Entry : std::filesystem::directory_iterator(FolderPath, Ec))
		{
			if (Entry.is_directory(Ec))
			{
				RenderFolderTreeNode(Entry.path());
			}
		}
		ImGui::TreePop();
	}

	ImGui::PopID();
}
