#pragma once

#include "Runtime/Core/TArray.h"
#include "Runtime/Core/IntTypes.h"
#include "Runtime/Geometry/FAxisAlignedBoundingBox.h"
#include "Runtime/Geometry/FRay.h"
#include "Runtime/Math/FVector.h"

struct FMeshBVHSettings
{
	int32 MaxTrianglesPerLeaf = 4;
	int32 MaxDepth = 32;
};

struct FMeshBVHRayHit
{
	int32 TriangleIndex = -1; // 원본 삼각형 번호 (Indices[3 * TriangleIndex ...])
	float T = -1.0f;
};

class FMeshBVH
{
public:
	void Build(const TArray<FVector>& Positions, const TArray<uint32>& Indices, const FMeshBVHSettings& InSettings = {});
	void Clear();

	bool IsBuilt() const { return !Nodes.empty(); }
	int32 GetNodeCount() const { return static_cast<int32>(Nodes.size()); }
	int32 GetTriangleCount() const { return static_cast<int32>(Triangles.size()); }

	bool QueryRayClosest(const FRay& Ray, FMeshBVHRayHit& OutHit) const;
	void QueryAABB(const FAxisAlignedBoundingBox& Box, TArray<int32>& OutTriangles) const;

private:
	struct FNode
	{
		FAxisAlignedBoundingBox Bounds;
		int32 LeftOrFirst = -1;  // 내부 노드: 왼쪽 자식(오른쪽은 +1) / 리프: Triangles 시작 위치
		int32 TriangleCount = 0; // 0이면 내부 노드

		bool IsLeaf() const { return TriangleCount > 0; }
	};

	struct FTriangle
	{
		FVector A;
		FVector B;
		FVector C;
		int32 TriangleIndex = -1;
	};

	FMeshBVHSettings Settings;
	TArray<FNode> Nodes;         // Nodes[0] = 루트
	TArray<FTriangle> Triangles; // 리프 순서대로 재배치됨

	void Subdivide(int32 NodeIndex, int32 Depth);
	FAxisAlignedBoundingBox ComputeBounds(int32 First, int32 Count) const;
};
