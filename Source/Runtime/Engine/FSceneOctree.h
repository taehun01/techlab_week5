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

	const TArray<UMeshComponent*>& GetCameraFacingComponents() const { return CameraFacingComponents; }

private:
	FOctree Tree;
	TSet<UMeshComponent*> DirtyComponents;
	TArray<UMeshComponent*> CameraFacingComponents;

	UMeshComponent* FindComponent(int32 ObjectIndex) const;
};
