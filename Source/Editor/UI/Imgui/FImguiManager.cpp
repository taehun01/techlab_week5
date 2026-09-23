#include "FImguiManager.h"
#include "ThirdParty/Imgui/imgui.h"
#include "ThirdParty/Imgui/imgui_internal.h"
#include "ThirdParty/Imgui/imgui_impl_dx11.h"
#include "ThirdParty/Imgui/imgui_impl_win32.h"


bool FImguiManager::Initialize_ImplWin32DX11(HWND& Window, ID3D11Device* Device, ID3D11DeviceContext* Context)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	auto& IO = ImGui::GetIO();
	IO.Fonts->AddFontFromFileTTF(
		"C:/Windows/Fonts/malgun.ttf",
		18.0f,
		nullptr,
		IO.Fonts->GetGlyphRangesKorean()
	);

	IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	// IO.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	ImFontConfig Config;
	Config.SizePixels = 16.0f;
	IO.Fonts->AddFontDefault(&Config);

	bool success = true;
	success &= ImGui_ImplWin32_Init((void*)Window);
	success &= ImGui_ImplDX11_Init(Device, Context);
	if (success) ImplType = EImplType::Win32DX11;
	return success;
}

void FImguiManager::NewFrame()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGuiViewport* Viewport = ImGui::GetMainViewport();

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGuiID DockSpaceID = ImGui::DockSpaceOverViewport(0, Viewport, ImGuiDockNodeFlags_PassthruCentralNode);
	ImGui::PopStyleColor();

	static bool Initialized = false;
	if (!Initialized)
	{
		ImGui::DockBuilderRemoveNode(DockSpaceID);
		ImGui::DockBuilderAddNode(DockSpaceID, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
		ImGui::DockBuilderSetNodeSize(DockSpaceID, Viewport->WorkSize);

		ImGuiID MainID = DockSpaceID;

		ImGuiID RightID = ImGui::DockBuilderSplitNode(MainID, ImGuiDir_Right, 0.33f, nullptr, &MainID);
		ImGuiID RightRemaining = RightID;
		ImGuiID PropertyWindowID = ImGui::DockBuilderSplitNode(RightRemaining, ImGuiDir_Up, 0.4f, nullptr, &RightRemaining);
		ImGuiID ControlPanelID = RightRemaining;

		ImGuiID ConsoleID = ImGui::DockBuilderSplitNode(MainID, ImGuiDir_Down, 0.25f, nullptr, &MainID);

		ImGui::DockBuilderDockWindow("Viewport", MainID);
		ImGui::DockBuilderDockWindow("World Outliner", PropertyWindowID);
		ImGui::DockBuilderDockWindow("Jungle Property Window", PropertyWindowID);
		ImGui::DockBuilderDockWindow("Jungle Control Panel", ControlPanelID);
		ImGui::DockBuilderDockWindow("Content Drawer", ConsoleID);
		ImGui::DockBuilderDockWindow("Console Window", ConsoleID);

		ImGui::DockBuilderFinish(DockSpaceID);

		EditorViewportID = MainID;
		if (ImGuiDockNode* Node =
			ImGui::DockBuilderGetNode(EditorViewportID))
		{
			//Node->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
		}

		Initialized = true;
	}
}

void FImguiManager::RenderUI()
{
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	// 독립 플랫폼 윈도우 렌더링
	if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
}

ImGuiID FImguiManager::GetEditorViewportID() const
{
	// 뷰포트 창이 실제로 도킹된 노드 ID 우선 반환
	if (ImGuiWindow* ViewportWindow = ImGui::FindWindowByName("Viewport"))
	{
		if (ViewportWindow->DockId != 0)
		{
			return ViewportWindow->DockId;
		}
	}
	return EditorViewportID;
}
