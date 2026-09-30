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
#include "Runtime/Actors/AStaticMeshActor.h"
#include <numbers>

namespace
{
	FVector ReadVector(const nlohmann::json& Array)
	{
		return FVector{ Array.at(0).get<float>(), Array.at(1).get<float>(), Array.at(2).get<float>() };
	}

	// Version 필드가 없는 예전 씬 형식({ NextUUID, PerspectiveCamera, Primitives })을 불러온다.
	// Primitives의 각 항목은 StaticMeshComp 하나이므로 AStaticMeshActor로 감싸서 생성한다.
	void LoadLegacyPrimitives(UScene* Scene, const nlohmann::json& Primitives)
	{
		constexpr float RadToDeg = 180.0f / std::numbers::pi_v<float>;

		for (const auto& [Key, Item] : Primitives.items())
		{
			const FString Type = Item.value("Type", FString());
			if (Type != "StaticMeshComp")
			{
				UE_LOG_WARN("[LoadScene] 지원하지 않는 Primitive 타입 %s (UUID %s)", Type.c_str(), Key.c_str());
				continue;
			}

			FTransform Transform{};
			Transform.Location = ReadVector(Item.at("Location"));
			FVector Rotation = ReadVector(Item.at("Rotation"));
			for (int i = 0; i < 3; ++i)
			{
				Rotation[i] *= RadToDeg;
			}
			Transform.Rotation = FQuaternion::FromEulerXYZDeg(Rotation);
			Transform.Scale3D = ReadVector(Item.at("Scale"));

			AStaticMeshActor* Actor = Scene->SpawnActor<AStaticMeshActor>(Transform.Location, Transform.Scale3D);
			Actor->SetTransform(Transform);

			// "Data/apple_mid.obj" -> "apple_mid" (FRenderResourceLibrary는 OBJ 파일 stem을 MeshID로 쓴다)
			const FString AssetPath = Item.value("ObjStaticMeshAsset", FString());
			const FString MeshId = std::filesystem::path(AssetPath).stem().string();
			if (UStaticMesh* Mesh = FRenderResourceLibrary::Get().GetUStaticMesh(FName(MeshId)))
			{
				Actor->SetStaticMesh(Mesh);
			}
			else
			{
				UE_LOG_WARN("[LoadScene] 메시 %s 를 찾을 수 없습니다.", AssetPath.c_str());
			}
		}
	}
}

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

std::optional<FSceneCameraInfo> USceneManager::LoadScene(const FString& path)
{

	std::ifstream file(path);
	if (!file)
	{
		UE_LOG("[LoadScene] 씬을 파일에서 불러오는데 실패했습니다. 파일을 읽을 수 없습니다.");
		return std::nullopt;
	}

	std::stringstream buffer;
	buffer << file.rdbuf();

	UScene* Scene = nullptr;
	try
	{
		nlohmann::json JSON = nlohmann::json::parse(buffer.str());
		FArchive Archive{ JSON };

		if (!JSON.contains("Version") && JSON.contains("Primitives"))
		{
			const uint32 SavedNextUUID = JSON.value("NextUUID", 0u);
			FUObjectArray& ObjectArray = FUObjectArray::Get();
			if (SavedNextUUID > ObjectArray.GetNextUUID())
			{
				ObjectArray.SetNextUUID(SavedNextUUID);
			}

			Scene = NewObject<UScene>();
			Scene->Initialize();
			Scene->SetRenderResourceLibrary(&FRenderResourceLibrary::Get());
			LoadLegacyPrimitives(Scene, JSON.at("Primitives"));

			std::optional<FSceneCameraInfo> Camera;
			if (JSON.contains("PerspectiveCamera"))
			{
				// 예전 형식은 스칼라 값도 [60.0]처럼 원소 하나짜리 배열로 저장한다.
				const nlohmann::json& CameraJSON = JSON.at("PerspectiveCamera");
				Camera = FSceneCameraInfo{
					.Location = ReadVector(CameraJSON.at("Location")),
					.RotationRad = ReadVector(CameraJSON.at("Rotation")),
					.FOV = CameraJSON.at("FOV").at(0).get<float>(),
					.NearClip = CameraJSON.at("NearClip").at(0).get<float>(),
					.FarClip = CameraJSON.at("FarClip").at(0).get<float>(),
				};
			}

			SetScene(Scene);
			return Camera;
		}

		int32 Version = Archive.GetInt32("Version");
		if (Version != 2)
		{
			UE_LOG("[LoadScene] 로드하려는 파일의 Scene Schema 버전이 다릅니다. 파일의 버전: %d, 지원하는 버전: %d", Version, 2);
			return std::nullopt;
		}

		if (Archive.IsNull("Scene"))
		{
			UE_LOG("[LoadScene] 로드하려는 파일에서 Scene 항목이 없습니다. 파일 형식이 올바르지 않습니다.");
			return std::nullopt;
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
		return std::nullopt;
	}

	SetScene(Scene);
	return std::nullopt;
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
