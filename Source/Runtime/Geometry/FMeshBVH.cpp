#include "FMeshBVH.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
	constexpr int32 MaxStackSize = 64;
	//AABB 박스가 점 하나를 포함하도록 박스를 넓히는 함수
	void Expand(FAxisAlignedBoundingBox& Box, const FVector& Point)
	{
		Box.Min = FVector((std::min)(Box.Min.X, Point.X), (std::min)(Box.Min.Y, Point.Y), (std::min)(Box.Min.Z, Point.Z));
		Box.Max = FVector((std::max)(Box.Max.X, Point.X), (std::max)(Box.Max.Y, Point.Y), (std::max)(Box.Max.Z, Point.Z));
	}

	bool IsIntersect(const FAxisAlignedBoundingBox& A, const FAxisAlignedBoundingBox& B)
	{
		return !(A.Min.X > B.Max.X
			|| A.Min.Y > B.Max.Y
			|| A.Min.Z > B.Max.Z
			|| A.Max.X < B.Min.X
			|| A.Max.Y < B.Min.Y
			|| A.Max.Z < B.Min.Z);
	}

	// 순회 중 반복 사용하는 광선 정보를 미리 계산해 둔다.
	struct FRayCache
	{
		FVector Origin;
		FVector InvDirection;
		bool bParallel[3] = {};

		explicit FRayCache(const FRay& Ray)
			: Origin(Ray.Origin)
		{
			constexpr float Epsilon = 0.000001f;
			for (int32 I = 0; I < 3; ++I)
			{
				bParallel[I] = std::abs(Ray.Direction[I]) < Epsilon;
				InvDirection[I] = bParallel[I] ? 0.0f : 1.0f / Ray.Direction[I];
			}
		}
	};

	bool IntersectRayBounds(const FRayCache& Ray, const FAxisAlignedBoundingBox& Box, float MaxT, float& OutTNear)
	{
		float TNear = 0.0f;
		float TFar = MaxT;

		for (int32 I = 0; I < 3; ++I)
		{
			if (Ray.bParallel[I])
			{
				if (Ray.Origin[I] < Box.Min[I] || Ray.Origin[I] > Box.Max[I])
				{
					return false;
				}
				continue;
			}

			float T0 = (Box.Min[I] - Ray.Origin[I]) * Ray.InvDirection[I];
			float T1 = (Box.Max[I] - Ray.Origin[I]) * Ray.InvDirection[I];
			if (T0 > T1)
			{
				std::swap(T0, T1);
			}

			TNear = (std::max)(TNear, T0);
			TFar = (std::min)(TFar, T1);
			if (TNear > TFar)
			{
				return false;
			}
		}

		OutTNear = TNear;
		return true;
	}
}

void FMeshBVH::Build(const TArray<FVector>& Positions, const TArray<uint32>& Indices, const FMeshBVHSettings& InSettings)
{
	Clear();

	Settings = InSettings;
	Settings.MaxTrianglesPerLeaf = (std::max)(Settings.MaxTrianglesPerLeaf, 1);
	Settings.MaxDepth = (std::clamp)(Settings.MaxDepth, 0, MaxStackSize - 2);

	const bool bHasIndices = !Indices.empty();
	const uint32 ElementCount = bHasIndices
		? static_cast<uint32>(Indices.size())
		: static_cast<uint32>(Positions.size());
	const uint32 VertexCount = static_cast<uint32>(Positions.size());

	Triangles.reserve(ElementCount / 3);
	for (uint32 I = 0; I + 2 < ElementCount; I += 3)
	{
		const uint32 I0 = bHasIndices ? Indices[I] : I;
		const uint32 I1 = bHasIndices ? Indices[I + 1] : I + 1;
		const uint32 I2 = bHasIndices ? Indices[I + 2] : I + 2;

		// 잘못된 인덱스 방어
		if (I0 >= VertexCount || I1 >= VertexCount || I2 >= VertexCount)
		{
			continue;
		}

		Triangles.push_back({ Positions[I0], Positions[I1], Positions[I2], static_cast<int32>(I / 3) });
	}

	if (Triangles.empty())
	{
		return;
	}

	// 이진 트리의 노드 수는 최대 2N - 1
	Nodes.reserve(Triangles.size() * 2 - 1);

	FNode Root;
	Root.LeftOrFirst = 0;
	Root.TriangleCount = static_cast<int32>(Triangles.size());
	Root.Bounds = ComputeBounds(0, Root.TriangleCount);
	Nodes.push_back(Root);

	Subdivide(0, 0);
}

void FMeshBVH::Clear()
{
	Nodes.clear();
	Triangles.clear();
}

bool FMeshBVH::QueryRayClosest(const FRay& Ray, FMeshBVHRayHit& OutHit) const
{
	if (Nodes.empty())
	{
		return false;
	}

	const FRayCache RayCache(Ray);
	float ClosestT = (std::numeric_limits<float>::max)();
	int32 ClosestTriangle = -1;

	struct FStackEntry
	{
		int32 NodeIndex;
		float TNear;
	};

	FStackEntry Stack[MaxStackSize];
	int32 StackSize = 0;

	float RootTNear = 0.0f;
	if (!IntersectRayBounds(RayCache, Nodes[0].Bounds, ClosestT, RootTNear))
	{
		return false;
	}
	Stack[StackSize++] = { 0, RootTNear };

	while (StackSize > 0)
	{
		const FStackEntry Entry = Stack[--StackSize];

		if (Entry.TNear > ClosestT)
		{
			continue;
		}

		const FNode& Node = Nodes[Entry.NodeIndex];

		if (Node.IsLeaf())
		{
			for (int32 I = Node.LeftOrFirst; I < Node.LeftOrFirst + Node.TriangleCount; ++I)
			{
				const FTriangle& Triangle = Triangles[I];
				float HitT = 0.0f;
				if (IntersectRayTriangle(Ray, Triangle.A, Triangle.B, Triangle.C, HitT) && HitT < ClosestT)
				{
					ClosestT = HitT;
					ClosestTriangle = Triangle.TriangleIndex;
				}
			}
			continue;
		}

		const int32 Left = Node.LeftOrFirst;
		const int32 Right = Node.LeftOrFirst + 1;

		float LeftTNear = 0.0f;
		float RightTNear = 0.0f;
		const bool bHitLeft = IntersectRayBounds(RayCache, Nodes[Left].Bounds, ClosestT, LeftTNear);
		const bool bHitRight = IntersectRayBounds(RayCache, Nodes[Right].Bounds, ClosestT, RightTNear);

		// 가까운 자식을 나중에 넣어 먼저 방문한다.
		if (bHitLeft && bHitRight)
		{
			if (LeftTNear <= RightTNear)
			{
				Stack[StackSize++] = { Right, RightTNear };
				Stack[StackSize++] = { Left, LeftTNear };
			}
			else
			{
				Stack[StackSize++] = { Left, LeftTNear };
				Stack[StackSize++] = { Right, RightTNear };
			}
		}
		else if (bHitLeft)
		{
			Stack[StackSize++] = { Left, LeftTNear };
		}
		else if (bHitRight)
		{
			Stack[StackSize++] = { Right, RightTNear };
		}
	}

	if (ClosestTriangle == -1)
	{
		return false;
	}

	OutHit.TriangleIndex = ClosestTriangle;
	OutHit.T = ClosestT;
	return true;
}

void FMeshBVH::QueryAABB(const FAxisAlignedBoundingBox& Box, TArray<int32>& OutTriangles) const
{
	if (Nodes.empty())
	{
		return;
	}

	int32 Stack[MaxStackSize];
	int32 StackSize = 0;
	Stack[StackSize++] = 0;

	while (StackSize > 0)
	{
		const FNode& Node = Nodes[Stack[--StackSize]];

		if (!IsIntersect(Node.Bounds, Box))
		{
			continue;
		}

		if (Node.IsLeaf())
		{
			for (int32 I = Node.LeftOrFirst; I < Node.LeftOrFirst + Node.TriangleCount; ++I)
			{
				const FTriangle& Triangle = Triangles[I];

				FAxisAlignedBoundingBox TriangleBounds;
				Expand(TriangleBounds, Triangle.A);
				Expand(TriangleBounds, Triangle.B);
				Expand(TriangleBounds, Triangle.C);

				if (IsIntersect(TriangleBounds, Box))
				{
					OutTriangles.push_back(Triangle.TriangleIndex);
				}
			}
			continue;
		}

		Stack[StackSize++] = Node.LeftOrFirst;
		Stack[StackSize++] = Node.LeftOrFirst + 1;
	}
}

void FMeshBVH::Subdivide(int32 NodeIndex, int32 Depth)
{
	const int32 First = Nodes[NodeIndex].LeftOrFirst;
	const int32 Count = Nodes[NodeIndex].TriangleCount;

	if (Count <= Settings.MaxTrianglesPerLeaf || Depth >= Settings.MaxDepth)
	{
		return;
	}

	// 삼각형 중심점이 가장 넓게 퍼진 축을 분할 축으로 고른다.
	FAxisAlignedBoundingBox CentroidBounds;
	for (int32 I = First; I < First + Count; ++I)
	{
		const FTriangle& Triangle = Triangles[I];
		Expand(CentroidBounds, (Triangle.A + Triangle.B + Triangle.C) * (1.0f / 3.0f));
	}

	const FVector Extent = CentroidBounds.Max - CentroidBounds.Min;
	int32 Axis = 0;
	if (Extent.Y > Extent[Axis]) Axis = 1;
	if (Extent.Z > Extent[Axis]) Axis = 2;

	// 중심점이 모두 같은 위치면 더 나눌 수 없다.
	if (Extent[Axis] <= 0.0f)
	{
		return;
	}

	const int32 Mid = Settings.bUseSAH
		? SplitSAH(First, Count, Axis)
		: SplitMedian(First, Count, Axis);

	const int32 LeftIndex = static_cast<int32>(Nodes.size());

	FNode Left;
	Left.LeftOrFirst = First;
	Left.TriangleCount = Mid - First;
	Left.Bounds = ComputeBounds(Left.LeftOrFirst, Left.TriangleCount);

	FNode Right;
	Right.LeftOrFirst = Mid;
	Right.TriangleCount = First + Count - Mid;
	Right.Bounds = ComputeBounds(Right.LeftOrFirst, Right.TriangleCount);

	// push_back 이후 Nodes 참조가 무효화될 수 있으므로 인덱스로 접근한다.
	Nodes.push_back(Left);
	Nodes.push_back(Right);

	Nodes[NodeIndex].LeftOrFirst = LeftIndex;
	Nodes[NodeIndex].TriangleCount = 0;

	Subdivide(LeftIndex, Depth + 1);
	Subdivide(LeftIndex + 1, Depth + 1);
}

int32 FMeshBVH::SplitMedian(int32 First, int32 Count, int32 Axis)
{
	// Median split: 중심점 기준 중앙값으로 절반씩 나눈다.
	const int32 Mid = First + Count / 2;
	std::nth_element(
		Triangles.begin() + First,
		Triangles.begin() + Mid,
		Triangles.begin() + First + Count,
		[Axis](const FTriangle& L, const FTriangle& R)
		{
			return (L.A[Axis] + L.B[Axis] + L.C[Axis]) < (R.A[Axis] + R.B[Axis] + R.C[Axis]);
		});
	return Mid;
}

int32 FMeshBVH::SplitSAH(int32 First, int32 Count, int32 Axis)
{
	// SAH: 왼쪽 AABB 표면적 * 개수 + 오른쪽 AABB 표면적 * 개수가 가장 작은 분할점을 고른다.
	// 
	// 고른 Axis 기준으로 중심점 순 정렬
	std::sort(Triangles.begin() + First, Triangles.begin() + First + Count,
		[Axis](const FTriangle& L, const FTriangle& R)
		{
			return (L.A[Axis] + L.B[Axis] + L.C[Axis]) < (R.A[Axis] + R.B[Axis] + R.C[Axis]);
		});

	// 오른쪽에서 왼쪽으로 스윕: RightArea[I] = [First + I, End) 박스 표면적
	TArray<float> RightArea(Count);
	FAxisAlignedBoundingBox RightBox;
	for (int32 I = Count - 1; I > 0; --I)
	{
		const FTriangle& T = Triangles[First + I];
		Expand(RightBox, T.A); Expand(RightBox, T.B); Expand(RightBox, T.C);
		RightArea[I] = SurfaceArea(RightBox);
	}

	// 왼쪽에서 오른쪽으로 스윕하면서 비용 비교
	float BestCost = std::numeric_limits<float>::max();
	int32 BestSplit = First + Count / 2;
	FAxisAlignedBoundingBox LeftBox;
	for (int32 I = 1; I < Count; ++I) // I = 왼쪽 개수, 양쪽 최소 1개
	{
		const FTriangle& T = Triangles[First + I - 1];
		Expand(LeftBox, T.A); Expand(LeftBox, T.B); Expand(LeftBox, T.C);

		const float Cost = SurfaceArea(LeftBox) * I + RightArea[I] * (Count - I);
		if (Cost < BestCost)
		{
			BestCost = Cost;
			BestSplit = First + I;
		}
	}
	return BestSplit;
}

//삼각형 A, B, C를 전부 Expand해서 범위 전체를 감싸는 박스를 만듭니다.
FAxisAlignedBoundingBox FMeshBVH::ComputeBounds(int32 First, int32 Count) const
{
	FAxisAlignedBoundingBox Bounds;
	for (int32 I = First; I < First + Count; ++I)
	{
		Expand(Bounds, Triangles[I].A);
		Expand(Bounds, Triangles[I].B);
		Expand(Bounds, Triangles[I].C);
	}
	return Bounds;
}

float FMeshBVH::SurfaceArea(const FAxisAlignedBoundingBox& Box)
{
	const FVector E = Box.Max - Box.Min;
	return 2.0f * (E.X * E.Y + E.Y * E.Z + E.Z * E.X);
}