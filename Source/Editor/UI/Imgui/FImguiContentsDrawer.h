#pragma once
#include "Editor/Core/FEditor.h"
#include "Runtime/Rendering/FPreviewRenderTarget.h"

class FImguiContentsDrawer final
{

public:
	FImguiContentsDrawer();
	~FImguiContentsDrawer() = default;

	//복사 생성 금지
	FImguiContentsDrawer(const FImguiContentsDrawer&) = delete;
	//복사 대입 금지
	FImguiContentsDrawer& operator=(const FImguiContentsDrawer&) = delete;

	void Process(FEditor& Editor);
	std::filesystem::path RootPath;
	std::filesystem::path CurrentPath;
	float LeftPanelWidth;


private:
	//폴더 트리 렌더
	void RenderFolderTree();
	void RenderFolderTreeNode(const std::filesystem::path& FolderPath);
	// 우측 파일 목록
	void RenderContentView();

	// 폴더 안의 항목 하나.
	// 이름은 표시용으로 미리 UTF-8로 변환해 둔다. ImGui는 UTF-8만 받는다.
	struct FContentEntry
	{
		std::filesystem::path Path;
		FString DisplayName;
		FString Extension;   // 소문자. 썸네일 조회와 필터에 쓴다
		bool bIsDirectory = false;
	};

	// CurrentPath의 내용을 다시 읽어 Entries를 채운다.
	void RefreshEntries();

	// 항목의 썸네일용 텍스처를 얻는다. 없으면 디스크에서 읽어 라이브러리에 등록한다.
	// 이미지가 아니거나 로드에 실패하면 nullptr.
	TSharedPtr<class FTexture> GetOrLoadThumbnail(const FContentEntry& Item);

	// directory_iterator는 실제 파일 시스템 호출이라 매 프레임 돌리면 느려진다.
	// 폴더가 바뀔 때와 새로고침할 때만 갱신한다.
	TArray<FContentEntry> Entries;
	std::filesystem::path CachedPath;
	bool bNeedsRefresh = true;

	std::filesystem::path SelectedPath;
	float ThumbnailSize = 80.0f;

	// 한 프레임에 새로 디코딩할 이미지 수.
	// 폴더를 처음 열 때 수십 장을 한꺼번에 읽으면 눈에 띄게 멈춘다.
	static constexpr int MaxLoadsPerFrame = 2;
	int LoadsThisFrame = 0;
};
