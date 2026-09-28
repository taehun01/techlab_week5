#include "FSceneOctree.h"

#include "Runtime/CoreUObject/UMeshComponent.h"
#include "Runtime/CoreUObject/FUObjectArray.h"

#include <algorithm>

namespace
{
	FAxisAlignedBoundingBox CalcWorldBounds(UMeshComponent& Component)
	{
		const FAxisAlignedBoundingBox Local = Component.CalcLocalBounds();
		const FMatrix World = Component.GetGlobalTransform().ToMatrix();

		if (Local.Min.X > Local.Max.X)
		{
			const FVector Location = World.TransformPointRow(FVector(0.0f, 0.0f, 0.0f));
			FAxisAlignedBoundingBox Point;
			Point.Min = Location;
			Point.Max = Location;
			return Point;
		}

		FAxisAlignedBoundingBox Out;
		for (int32 i = 0; i < 8; ++i)
		{
			const FVector Corner(
				(i & 4) ? Local.Max.X : Local.Min.X,
				(i & 2) ? Local.Max.Y : Local.Min.Y,
				(i & 1) ? Local.Max.Z : Local.Min.Z);
			const FVector P = World.TransformPointRow(Corner);
			for (int32 a = 0; a < 3; ++a)
			{
				Out.Min[a] = (std::min)(Out.Min[a], P[a]);
				Out.Max[a] = (std::max)(Out.Max[a], P[a]);
			}
		}
		return Out;
	}
}

FSceneOctree::FSceneOctree(const FVector& RootCenter, float RootHalfSize, const FOctreeSettings& InSettings)
	: Tree(RootCenter, RootHalfSize, InSettings)
{

}

void FSceneOctree::Register(UMeshComponent* Component)
{
	if (Component->IsCameraFacing())
	{
		CameraFacingComponents.push_back(Component);
		return;
	}

	const auto& Index = Component->GetInternalIndex();
	const auto& AABB = CalcWorldBounds(*Component);
	Tree.Insert(FOctreeElement(Index, AABB));
}

void FSceneOctree::Unregister(UMeshComponent* Component)
{
	if (Component->IsCameraFacing())
	{
		std::erase(CameraFacingComponents, Component);
		return;
	}

	Tree.Remove(Component->GetInternalIndex());
	DirtyComponents.erase(Component);
}

void FSceneOctree::MarkDirty(UMeshComponent* Component)
{
	if (Component->IsCameraFacing())
	{
		return;
	}
	DirtyComponents.insert(Component);
}

void FSceneOctree::Flush()
{
	for (const auto& Component : DirtyComponents)
	{
		const auto& Index = Component->GetInternalIndex();
		const auto& AABB = CalcWorldBounds(*Component);
		Tree.Update(Index, AABB);
	}
	DirtyComponents.clear();
}

void FSceneOctree::Clear()
{
	Tree.Clear();
	DirtyComponents.clear();
	CameraFacingComponents.clear();
}

void FSceneOctree::RaycastCandidates(const FRay& Ray, TArray<FSceneRayCandidate>& OutCandidates) const
{
	TArray<FOctreeRayHit> Hits;
	Tree.QueryRay(Ray, Hits);

	for (const auto& Hit : Hits)
	{
		UMeshComponent* Component = FindComponent(Hit.ObjectIndex);
		if (Component)
		{
			OutCandidates.push_back({ Component, Hit.TNear });
		}
	}

	std::sort(OutCandidates.begin(), OutCandidates.end(),
		[](const FSceneRayCandidate& A, const FSceneRayCandidate& B) { return A.TNear < B.TNear; });
}

void FSceneOctree::QueryAABB(const FAxisAlignedBoundingBox& Box, TArray<UMeshComponent*>& OutComponents) const
{
	TArray<int32> Hits;
	Tree.QueryAABB(Box, Hits);

	for (const int32 Hit : Hits)
	{
		UMeshComponent* Component = FindComponent(Hit);
		if (Component)
		{
			OutComponents.push_back(Component);
		}
	}
}

UMeshComponent* FSceneOctree::FindComponent(int32 ObjectIndex) const
{
	UObject* Object = FUObjectArray::Get().GetObjectByIndex(static_cast<uint32>(ObjectIndex));
	return Object ? Object->Cast<UMeshComponent>() : nullptr;
}
