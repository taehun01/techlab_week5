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
	// 나눗셈과 방향 부호 판단을 순회 밖으로 빼서, 박스 검사에서는 분기 없이 곱셈과 min/max만 쓴다.
	struct FRayCache
	{
		float Origin[3];
		float InvDirection[3];
		int32 Neg[3]; // 방향이 음수인 축이면 1 (광선이 먼저 만나는 면이 Max)

		explicit FRayCache(const FRay& Ray)
		{
			for (int32 I = 0; I < 3; ++I)
			{
				const float D = Ray.Direction[I];
				Origin[I] = Ray.Origin[I];
				// 0 방향이면 매우 큰 값으로 대체한다. 1/0 = inf는 0 * inf = NaN을 만들 수 있다.
				// 축과 평행한 광선은 시작점이 슬랩 밖이면 (거리 * 1e30)이 범위를 벗어나 자연스럽게 탈락한다.
				InvDirection[I] = (std::abs(D) > 1e-20f) ? 1.0f / D : std::copysign(1e30f, D);
				Neg[I] = InvDirection[I] < 0.0f ? 1 : 0;
			}
		}
	};

	// 광선이 박스에 들어가는 거리. 원점이 박스 안이면 0. MaxT보다 멀거나 빗나가면 false.
	bool IntersectRayBounds(const FRayCache& Ray, const FAxisAlignedBoundingBox& Box, float MaxT, float& OutTNear)
	{
		const FVector* B[2] = { &Box.Min, &Box.Max };
		const float TxMin = ((*B[Ray.Neg[0]]).X - Ray.Origin[0]) * Ray.InvDirection[0];
		const float TxMax = ((*B[1 - Ray.Neg[0]]).X - Ray.Origin[0]) * Ray.InvDirection[0];
		const float TyMin = ((*B[Ray.Neg[1]]).Y - Ray.Origin[1]) * Ray.InvDirection[1];
		const float TyMax = ((*B[1 - Ray.Neg[1]]).Y - Ray.Origin[1]) * Ray.InvDirection[1];
		const float TzMin = ((*B[Ray.Neg[2]]).Z - Ray.Origin[2]) * Ray.InvDirection[2];
		const float TzMax = ((*B[1 - Ray.Neg[2]]).Z - Ray.Origin[2]) * Ray.InvDirection[2];

		const float TNear = (std::max)((std::max)(TxMin, TyMin), (std::max)(TzMin, 0.0f));
		const float TFar = (std::min)((std::min)(TxMax, TyMax), (std::min)(TzMax, MaxT));
		if (TNear > TFar)
		{
			return false;
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

bool FMeshBVH::QueryRayClosest(const FRay& Ray, FMeshBVHRayHit& OutHit, float MaxT) const
{
	if (Nodes.empty())
	{
		return false;
	}

	const FRayCache RayCache(Ray);
	float ClosestT = MaxT;
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

	FAxisAlignedBoundingBox CentroidBounds;
	for (int32 I = First; I < First + Count; ++I)
	{
		Expand(CentroidBounds, Centroid(Triangles[I]));
	}

	// 삼각형 중심점이 가장 넓게 퍼진 축 (Median split과 SAH 실패 시 대체용)
	const FVector Extent = CentroidBounds.Max - CentroidBounds.Min;
	int32 Axis = 0;
	if (Extent.Y > Extent[Axis]) Axis = 1;
	if (Extent.Z > Extent[Axis]) Axis = 2;

	// 중심점이 모두 같은 위치면 더 나눌 수 없다.
	if (Extent[Axis] <= 0.0f)
	{
		return;
	}

	int32 Mid = Settings.bUseSAH ? SplitSAH(First, Count, CentroidBounds) : -1;
	// SAH가 분할점을 못 찾았거나 한쪽으로 쏠리면 Median으로 자른다.
	if (Mid <= First || Mid >= First + Count)
	{
		Mid = SplitMedian(First, Count, Axis);
	}

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

int32 FMeshBVH::SplitSAH(int32 First, int32 Count, const FAxisAlignedBoundingBox& CentroidBounds)
{
	// Binned SAH: 중심점을 축마다 NumBins개 구간에 나눠 담고, 구간 경계에서만 비용을 계산한다.
	// 비용 = 왼쪽 AABB 표면적 * 개수 + 오른쪽 AABB 표면적 * 개수. 세 축 중 가장 작은 곳을 고른다.
	// 정렬 없이 O(N)이라 삼각형 수가 많아도 빌드가 빠르고, 트리 품질은 전체 정렬 SAH와 거의 같다.
	constexpr int32 NumBins = 16;

	struct FBin
	{
		FAxisAlignedBoundingBox Bounds;
		int32 Count = 0;
	};

	float BestCost = (std::numeric_limits<float>::max)();
	int32 BestAxis = -1;
	int32 BestSplit = -1; // 이 구간까지가 왼쪽

	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const float Lo = CentroidBounds.Min[Axis];
		const float Hi = CentroidBounds.Max[Axis];
		if (Hi - Lo <= 0.0f)
		{
			continue;
		}

		FBin Bins[NumBins];
		const float Scale = NumBins / (Hi - Lo);
		for (int32 I = First; I < First + Count; ++I)
		{
			const FTriangle& T = Triangles[I];
			const int32 BinIndex = (std::min)(NumBins - 1, static_cast<int32>((Centroid(T)[Axis] - Lo) * Scale));
			FBin& Bin = Bins[BinIndex];
			++Bin.Count;
			Expand(Bin.Bounds, T.A); Expand(Bin.Bounds, T.B); Expand(Bin.Bounds, T.C);
		}

		// 오른쪽에서 왼쪽으로 누적: Right*[I] = 구간 (I, NumBins) 합계
		float RightArea[NumBins - 1];
		int32 RightCount[NumBins - 1];
		FAxisAlignedBoundingBox RightBox;
		int32 RightSum = 0;
		for (int32 I = NumBins - 1; I > 0; --I)
		{
			RightSum += Bins[I].Count;
			if (Bins[I].Count > 0)
			{
				Expand(RightBox, Bins[I].Bounds.Min); Expand(RightBox, Bins[I].Bounds.Max);
			}
			RightCount[I - 1] = RightSum;
			RightArea[I - 1] = RightSum > 0 ? SurfaceArea(RightBox) : 0.0f;
		}

		// 왼쪽에서 오른쪽으로 누적하면서 비용 비교
		FAxisAlignedBoundingBox LeftBox;
		int32 LeftSum = 0;
		for (int32 I = 0; I < NumBins - 1; ++I)
		{
			LeftSum += Bins[I].Count;
			if (Bins[I].Count > 0)
			{
				Expand(LeftBox, Bins[I].Bounds.Min); Expand(LeftBox, Bins[I].Bounds.Max);
			}
			if (LeftSum == 0 || RightCount[I] == 0)
			{
				continue;
			}

			const float Cost = SurfaceArea(LeftBox) * LeftSum + RightArea[I] * RightCount[I];
			if (Cost < BestCost)
			{
				BestCost = Cost;
				BestAxis = Axis;
				BestSplit = I;
			}
		}
	}

	if (BestAxis < 0)
	{
		return -1;
	}

	// 고른 축/구간 기준으로 삼각형을 왼쪽, 오른쪽으로 재배치
	const float Lo = CentroidBounds.Min[BestAxis];
	const float Scale = NumBins / (CentroidBounds.Max[BestAxis] - Lo);
	const auto It = std::partition(Triangles.begin() + First, Triangles.begin() + First + Count,
		[&](const FTriangle& T)
		{
			const int32 BinIndex = (std::min)(NumBins - 1, static_cast<int32>((Centroid(T)[BestAxis] - Lo) * Scale));
			return BinIndex <= BestSplit;
		});
	return static_cast<int32>(It - Triangles.begin());
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