#pragma once

#include "Runtime/Core/TArray.h"
#include "Runtime/Core/TSet.h"
#include "Runtime/Geometry/FOctree.h"

class UMeshComponent;

struct FSceneRayCandidate
{
	UMeshComponent* Component = nullptr;
	float TNear = 0.0f;
};

class FSceneOctree
{
public:
	FSceneOctree(const FVector& RootCenter, float RootHalfSize, const FOctreeSettings& InSettings = {});

	void Register(UMeshComponent* Component); // UScene::AddRenderComponent
	void Unregister(UMeshComponent* Component);	// UScene::RemoveRenderComponent

	void MarkDirty(UMeshComponent* Component);
	void Flush();
	void Clear();

	void RaycastCandidates(const FRay& Ray, TArray<FSceneRayCandidate>& OutCandidates) const;
	void QueryAABB(const FAxisAlignedBoundingBox& Box, TArray<UMeshComponent*>& OutComponents) const;

	// 카메라를 향하는 컴포넌트(빌보드, 텍스트)는 옥트리에 넣지 않고 여기에 모은다.
	// 조회 결과에 포함되지 않으므로 필요하면 호출하는 쪽에서 직접 검사한다.
	[[nodiscard]] const TArray<UMeshComponent*>& GetCameraFacingComponents() const { return CameraFacingComponents; }

private:
	FOctree Tree;
	TSet<UMeshComponent*> DirtyComponents;
	TArray<UMeshComponent*> CameraFacingComponents;

	UMeshComponent* FindComponent(int32 ObjectIndex) const;
};
