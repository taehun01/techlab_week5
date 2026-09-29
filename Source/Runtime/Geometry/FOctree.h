#pragma once

#include "Runtime/Core/TArray.h"
#include "Runtime/Core/TMap.h"
#include "Runtime/Core/IntTypes.h"
#include "Runtime/Geometry/FAxisAlignedBoundingBox.h"
#include "Runtime/Geometry/FRay.h"
#include "Runtime/Math/FVector.h"

struct FOctreeElement
{
	int32 ObjectIndex = -1; // InternalIndex
	FAxisAlignedBoundingBox Bounds;
};

struct FOctreeSettings
{
	int32 MaxElementsPerNode = 16;
	int32 MaxDepth = 8;
	float LooseFactor = 2.0f;
};

struct FOctreeRayHit
{
	int32 ObjectIndex = -1;
	float TNear = -1.0f;
};

class FOctree
{
public:
	FOctree(const FVector& RootCenter, float RootHalfSize, const FOctreeSettings& InSettings = {});

	void Insert(const FOctreeElement& Element);
	void Remove(int32 ObjectIndex);
	void Update(int32 ObjectIndex, const FAxisAlignedBoundingBox& NewBounds);
	void Clear();

	void QueryAABB(const FAxisAlignedBoundingBox& Box, TArray<int32>& OutObjects) const;
	void QueryRay(const FRay& Ray, TArray<FOctreeRayHit>& OutHits) const;

private:
	struct FNode
	{
		FVector Center;
		float HalfSize = 0.0f;
		int32 Parent = -1;
		int32 FirstChild = -1;
		int32 Depth = 0;
	};

	struct FElementId
	{
		int32 NodeIndex = -1;
		int32 SlotIndex = -1;
	};

	FOctreeSettings Settings;
	TArray<FNode> Nodes;

	TArray<int32> FreeNodeBlocks;
	TArray<TArray<FOctreeElement>> Elements;
	TMap<int32, FElementId> ElementIdByObjectIndex;

	void AddToNode(int32 NodeIndex, const FOctreeElement& Element);
	void Split(int32 NodeIndex);
	void QueryAABB(const FAxisAlignedBoundingBox& Box, TArray<int32>& OutObjects, int32 NodeIndex) const;
	void QueryRay(const FRay& Ray, TArray<FOctreeRayHit>& OutHits, int32 NodeIndex) const;
	void TryMerge(int32 NodeIndex);
};
