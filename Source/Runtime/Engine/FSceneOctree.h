#pragma once

#include "Runtime/Core/TArray.h"
#include "Runtime/Core/TSet.h"
#include "Runtime/Geometry/FOctree.h"

class UMeshComponent;
struct FFrustum;

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
	// 프러스텀과 겹칠 수 있는 컴포넌트 후보를 모은다 (보수적: 실제로 안 보이는 것이 섞일 수는 있어도 보이는 것이 빠지지는 않는다).
	// 카메라를 향하는 컴포넌트(빌보드/텍스트)는 트리에 없으므로 항상 후보에 포함한다.
	void QueryFrustumCandidates(const FFrustum& Frustum, TArray<UMeshComponent*>& OutComponents) const;

	const TArray<UMeshComponent*>& GetCameraFacingComponents() const { return CameraFacingComponents; }

private:
	FOctree Tree;
	TSet<UMeshComponent*> DirtyComponents;
	TArray<UMeshComponent*> CameraFacingComponents;
	// QueryFrustumCandidates의 결과 인덱스 작업 배열. 매 프레임 할당하지 않도록 용량을 유지한다.
	mutable TArray<int32> FrustumQueryHits;

	UMeshComponent* FindComponent(int32 ObjectIndex) const;
};
