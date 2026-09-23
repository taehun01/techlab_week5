#include "FImguiToolBar.h"
#include "ThirdParty/Imgui/imgui.h"
#include "Runtime/Rendering/FRenderer.h"

// "표시명\0패턴\0" 이중 널 종료 필요
constexpr wchar_t SceneFilter[] = L"Scene Files (*.Scene)\0*.Scene\0All Files (*.*)\0*.*\0";

void FImguiToolbar::Process(FEditor& Editor, FImguiConsoleWindow& ConsoleWindow, FImguiControlPanelWindow& ControlPanelWindow, FImguiPropertyWindow& PropertyWindow)
{
    static FString CurrentScenePath;

	if (ImGui::BeginMainMenuBar()) 
    {
        //씬 저장,로드 기능
        ShowFileBar(CurrentScenePath, Editor);

        //Imgui Window들 소환
        ShowViewBar(Editor, ConsoleWindow);

        ImGui::EndMainMenuBar();
	}
}

// 취소하면 false
bool FImguiToolbar::PickSceneFile(FString& OutPath, bool bSave)
{
    wchar_t Buffer[MAX_PATH]{};
    const std::wstring InitialDir = GetScenesDirectory().wstring();

    OPENFILENAMEW Desc{};
    Desc.lStructSize = sizeof(Desc);
    Desc.hwndOwner = static_cast<HWND>(ImGui::GetMainViewport()->PlatformHandleRaw);
    Desc.lpstrFilter = SceneFilter;
    Desc.lpstrFile = Buffer;
    Desc.nMaxFile = MAX_PATH;
    Desc.lpstrDefExt = L"Scene";
    Desc.lpstrInitialDir = InitialDir.c_str();
    // OFN_NOCHANGEDIR 없으면 대화상자가 프로세스 현재 디렉터리를 바꿔서
    // 이후 상대 경로 로딩(셰이더/텍스처)이 조용히 깨진다
    Desc.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR
        | (bSave ? OFN_OVERWRITEPROMPT : (OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST));

    if (!(bSave ? GetSaveFileNameW(&Desc) : GetOpenFileNameW(&Desc)))
        return false;

   OutPath = std::filesystem::path(Buffer).string();
    return true;
}

void FImguiToolbar::ShowFileBar(FString CurrentScenePath, FEditor& Editor)
{


    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("New Scene"))
        {
            Editor.NewScene();
            CurrentScenePath.clear();
        }
        if (ImGui::MenuItem("Save Scene"))
        {
            // 경로가 있으면 그대로 덮어쓰고, 없으면 다른 이름으로 저장과 같게 동작
            if (CurrentScenePath.empty())
            {
                FString Path;
                if (PickSceneFile(Path, true))
                {
                    CurrentScenePath = Path;
                    Editor.SaveScene(Path);
                }
            }
            else
            {
                Editor.SaveScene(CurrentScenePath);
            }
        }

        if (ImGui::MenuItem("Save scene as..."))
        {
            FString Path;
            if (PickSceneFile(Path, true))
            {
                CurrentScenePath = Path;
                Editor.SaveScene(Path);
            }
        }

        if (ImGui::MenuItem("Load Scene"))
        {
            FString Path;
            if (PickSceneFile(Path, false))
            {
                CurrentScenePath = Path;
                Editor.LoadScene(Path);
            }
        }

        ImGui::EndMenu();
    }

}

void FImguiToolbar::ShowViewBar(FEditor& Editor, FImguiConsoleWindow& ConsoleWindow)
{
    if (ImGui::BeginMenu("View"))
    {
        ImGui::EndMenu();
    }

    auto& Gizmo = Editor.GetGizmo();

    static const char* GizmoModes[4] = { "None", "Translation", "Rotation", "Scale" };
    const int SelectedItem = static_cast<int>(Editor.GetGizmo().Mode);
    if (ImGui::Button(GizmoModes[SelectedItem], { 150.0f, 0.0f }))
    {
        Gizmo.Mode = static_cast<EGizmoMode>((SelectedItem + 1) % 4);
    }
}
