#pragma once
#include "UScene.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include <optional>

// 씬 파일에 함께 저장된 카메라 정보 (예전 씬 형식의 PerspectiveCamera)
// 카메라는 에디터 소유이므로 런타임은 읽어서 넘겨주기만 한다.
struct FSceneCameraInfo
{
	FVector Location;
	FVector RotationRad; // (Roll, Pitch, Yaw), 라디안
	float FOV = 60.0f;
	float NearClip = 0.1f;
	float FarClip = 1000.0f;
};

class USceneManager final
{

public:
	void SaveScene(const FString& path) const;
	// 파일에 카메라 정보가 있으면 반환한다.
	std::optional<FSceneCameraInfo> LoadScene(const FString& path);
	void SetScene(UScene* scene);
	void Release();

	UScene* CurrentScene = nullptr;
};
