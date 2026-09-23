#include "USceneManager.h"
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include "ThirdParty/Json/nlohmann/json.hpp"
#include "Runtime/CoreUObject/FGarbageCollector.h"
#include "Runtime/Engine/FArchive.h"
#include "Runtime/CoreUObject/FUObjectArray.h"
#include "Runtime/Core/Log.h"

void USceneManager::SaveScene(const FString& path) const
{
	std::filesystem::path fsPath(path);
	std::filesystem::path directory = fsPath.parent_path();

	if (!directory.empty() && !std::filesystem::exists(directory))
		std::filesystem::create_directories(directory);

	FUObjectArray& ObjectArray = FUObjectArray::Get();
	int32 UUID = ObjectArray.GetNextUUID();

	FArchive Archive;
	Archive.SetInt32("Version", 2);
	Archive.SetInt32("NextUUID", UUID);

	FArchive SceneArchive;
	CurrentScene->Serialize(SceneArchive);
	Archive.SetArchive("Scene", SceneArchive);

	std::ofstream file(path);
	if (!file)
	{
		UE_LOG("[SaveScene] 현재 씬을 파일로 저장하는데 실패했습니다. 파일에 쓸 수 없습니다.");
		return;
	}

	file << Archive.GetJSON().dump(4);
}

void USceneManager::LoadScene(const FString& path)
{

	std::ifstream file(path);
	if (!file)
	{
		UE_LOG("[LoadScene] 씬을 파일에서 불러오는데 실패했습니다. 파일을 읽을 수 없습니다.");
		return;
	}

	std::stringstream buffer;
	buffer << file.rdbuf();

	UScene* Scene = nullptr;
	try
	{
		nlohmann::json JSON = nlohmann::json::parse(buffer.str());
		FArchive Archive{ JSON };

		int32 Version = Archive.GetInt32("Version");
		if (Version != 2)
		{
			UE_LOG("[LoadScene] 로드하려는 파일의 Scene Schema 버전이 다릅니다. 파일의 버전: %d, 지원하는 버전: %d", Version, 2);
			return;
		}

		if (Archive.IsNull("Scene"))
		{
			UE_LOG("[LoadScene] 로드하려는 파일에서 Scene 항목이 없습니다. 파일 형식이 올바르지 않습니다.");
			return;
		}

		// UUID는 런타임에 새로 발급하므로, 이미 살아 있는 객체와 겹치지 않도록 NextUUID는 올리기만 한다.
		const uint32 SavedNextUUID = static_cast<uint32>(Archive.GetInt32("NextUUID"));
		FUObjectArray& ObjectArray = FUObjectArray::Get();
		if (SavedNextUUID > ObjectArray.GetNextUUID())
		{
			ObjectArray.SetNextUUID(SavedNextUUID);
		}

		FArchive SceneArchive = Archive.GetArchive("Scene");

		Scene = NewObject<UScene>();
		Scene->Initialize();
		Scene->SetRenderResourceLibrary(&FRenderResourceLibrary::Get());
		Scene->Deserialize(SceneArchive);
	}
	catch (const nlohmann::json::exception& Exception)
	{
		UE_LOG("[LoadScene] 씬 파일 형식이 올바르지 않습니다: %s", Exception.what());
		if (Scene)
		{
			DestroyObject(Scene);
		}
		return;
	}

	SetScene(Scene);
}

void USceneManager::SetScene(UScene* scene)
{
	if (scene == nullptr) { return; }
	if (scene == CurrentScene) { return; }
	scene->Initialize();
	scene->SetRenderResourceLibrary(&FRenderResourceLibrary::Get());
	FGarbageCollector& GarbageCollector = FGarbageCollector::Get();

	if (CurrentScene)
	{
		GarbageCollector.RemoveRoot(CurrentScene);
		CurrentScene->EndPlay();
		CurrentScene->Deactivate();
		DestroyObject(CurrentScene);
	}
	CurrentScene = scene;
	GarbageCollector.AddRoot(CurrentScene);
	CurrentScene->Activate();
	CurrentScene->BeginPlay();
}

void USceneManager::Release()
{
	if (CurrentScene)
	{
		FGarbageCollector::Get().RemoveRoot(CurrentScene);
		DestroyObject(CurrentScene);
		CurrentScene = nullptr;
	}
}
