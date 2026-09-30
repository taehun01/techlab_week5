#include "FBoundsBVH.h"

#include <algorithm>
#include <functional>

namespace
{
	constexpr float BigFloat = (std::numeric_limits<float>::max)();

	struct FBox
	{
		float Min[3] = { BigFloat, BigFloat, BigFloat };
		float Max[3] = { -BigFloat, -BigFloat, -BigFloat };

		void Grow(const float P[3])
		{
			for (int32 i = 0; i < 3; ++i)
			{
				Min[i] = (std::min)(Min[i], P[i]);
				Max[i] = (std::max)(Max[i], P[i]);
			}
		}
		void Grow(const FBox& B)
		{
			for (int32 i = 0; i < 3; ++i)
			{
				Min[i] = (std::min)(Min[i], B.Min[i]);
				Max[i] = (std::max)(Max[i], B.Max[i]);
			}
		}
		void Grow(const FAxisAlignedBoundingBox& B)
		{
			for (int32 i = 0; i < 3; ++i)
			{
				Min[i] = (std::min)(Min[i], B.Min[i]);
				Max[i] = (std::max)(Max[i], B.Max[i]);
			}
		}
		bool IsValid() const { return Min[0] <= Max[0] && Min[1] <= Max[1] && Min[2] <= Max[2]; }
		float HalfArea() const
		{
			if (!IsValid())
			{
				return 0.0f;
			}
			const float X = Max[0] - Min[0];
			const float Y = Max[1] - Min[1];
			const float Z = Max[2] - Min[2];
			return X * Y + Y * Z + Z * X;
		}
	};

	struct FBuildTask
	{
		int32 Node;
		int32 Depth;
	};
}

void FBoundsBVH::Clear()
{
	Nodes.clear();
	Parents.clear();
	PrimIndices.clear();
	LeafOfPrim.clear();
	RefitStamp.clear();
}

void FBoundsBVH::ComputeLeafBounds(FNode& Node, const TArray<FAxisAlignedBoundingBox>& PrimBounds) const
{
	FBox B;
	for (int32 i = 0; i < Node.Count; ++i)
	{
		B.Grow(PrimBounds[PrimIndices[Node.LeftOrFirst + i]]);
	}
	std::copy(B.Min, B.Min + 3, Node.Min);
	std::copy(B.Max, B.Max + 3, Node.Max);
}

void FBoundsBVH::ComputeInnerBounds(int32 NodeIndex)
{
	FNode& Node = Nodes[NodeIndex];
	const FNode& L = Nodes[Node.LeftOrFirst];
	const FNode& R = Nodes[Node.LeftOrFirst + 1];
	for (int32 i = 0; i < 3; ++i)
	{
		Node.Min[i] = (std::min)(L.Min[i], R.Min[i]);
		Node.Max[i] = (std::max)(L.Max[i], R.Max[i]);
	}
}

void FBoundsBVH::Build(const TArray<FAxisAlignedBoundingBox>& PrimBounds)
{
	Clear();

	const int32 NumPrims = static_cast<int32>(PrimBounds.size());
	if (NumPrims == 0)
	{
		return;
	}

	// 분할 기준은 원소 박스의 중심점
	TArray<float> Centroids(static_cast<size_t>(NumPrims) * 3);
	PrimIndices.resize(NumPrims);
	for (int32 i = 0; i < NumPrims; ++i)
	{
		PrimIndices[i] = i;
		const FAxisAlignedBoundingBox& B = PrimBounds[i];
		const bool bValid = B.Min.X <= B.Max.X && B.Min.Y <= B.Max.Y && B.Min.Z <= B.Max.Z;
		for (int32 a = 0; a < 3; ++a)
		{
			Centroids[i * 3 + a] = bValid ? (B.Min[a] + B.Max[a]) * 0.5f : 0.0f;
		}
	}

	Nodes.reserve(static_cast<size_t>(NumPrims) * 2);
	Parents.reserve(static_cast<size_t>(NumPrims) * 2);

	FNode Root;
	Root.LeftOrFirst = 0;
	Root.Count = NumPrims;
	Nodes.push_back(Root);
	Parents.push_back(-1);

	// 재귀 대신 명시적 스택 (깊이가 깊어도 콜스택을 쓰지 않는다)
	TArray<FBuildTask> Tasks;
	Tasks.push_back({ 0, 0 });

	struct FBin
	{
		FBox Box;
		int32 Count = 0;
	};

	while (!Tasks.empty())
	{
		const FBuildTask Task = Tasks.back();
		Tasks.pop_back();

		ComputeLeafBounds(Nodes[Task.Node], PrimBounds);

		const int32 First = Nodes[Task.Node].LeftOrFirst;
		const int32 Count = Nodes[Task.Node].Count;
		if (Count <= MaxLeafSize || Task.Depth >= MaxDepth)
		{
			continue;
		}

		// 중심점 범위
		FBox CBox;
		for (int32 i = First; i < First + Count; ++i)
		{
			CBox.Grow(&Centroids[PrimIndices[i] * 3]);
		}

		// 세 축 모두에 대해 Binned SAH 비용 계산
		float BestCost = (std::numeric_limits<float>::max)();
		int32 BestAxis = -1;
		int32 BestSplit = -1;

		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const float Lo = CBox.Min[Axis];
			const float Hi = CBox.Max[Axis];
			if (Hi - Lo <= 1e-12f)
			{
				continue;
			}

			FBin Bins[NumBins];
			const float Scale = NumBins / (Hi - Lo);
			for (int32 i = First; i < First + Count; ++i)
			{
				const int32 P = PrimIndices[i];
				const int32 BinIdx = (std::min)(NumBins - 1, static_cast<int32>((Centroids[P * 3 + Axis] - Lo) * Scale));
				Bins[BinIdx].Count++;
				Bins[BinIdx].Box.Grow(PrimBounds[P]);
			}

			// 오른쪽 누적
			float RightArea[NumBins - 1];
			int32 RightCount[NumBins - 1];
			FBox RBox;
			int32 RSum = 0;
			for (int32 i = NumBins - 1; i > 0; --i)
			{
				RSum += Bins[i].Count;
				RBox.Grow(Bins[i].Box);
				RightCount[i - 1] = RSum;
				RightArea[i - 1] = RBox.HalfArea();
			}

			// 왼쪽 누적하며 비용 비교
			FBox LBox;
			int32 LSum = 0;
			for (int32 i = 0; i < NumBins - 1; ++i)
			{
				LSum += Bins[i].Count;
				LBox.Grow(Bins[i].Box);
				if (LSum == 0 || RightCount[i] == 0)
				{
					continue;
				}
				const float Cost = LSum * LBox.HalfArea() + RightCount[i] * RightArea[i];
				if (Cost < BestCost)
				{
					BestCost = Cost;
					BestAxis = Axis;
					BestSplit = i;
				}
			}
		}

		int32 Mid = -1;
		if (BestAxis >= 0)
		{
			const float Lo = CBox.Min[BestAxis];
			const float Scale = NumBins / (CBox.Max[BestAxis] - Lo);
			auto It = std::partition(PrimIndices.begin() + First, PrimIndices.begin() + First + Count,
				[&](int32 P)
				{
					const int32 BinIdx = (std::min)(NumBins - 1, static_cast<int32>((Centroids[P * 3 + BestAxis] - Lo) * Scale));
					return BinIdx <= BestSplit;
				});
			Mid = static_cast<int32>(It - PrimIndices.begin());
		}

		// 중심점이 모두 같거나 분할이 한쪽으로 쏠리면 절반으로 자른다 (리프가 무한히 커지지 않도록)
		if (Mid <= First || Mid >= First + Count)
		{
			Mid = First + Count / 2;
		}

		const int32 LeftIndex = static_cast<int32>(Nodes.size());

		FNode Left;
		Left.LeftOrFirst = First;
		Left.Count = Mid - First;
		FNode Right;
		Right.LeftOrFirst = Mid;
		Right.Count = First + Count - Mid;

		Nodes.push_back(Left);
		Nodes.push_back(Right);
		Parents.push_back(Task.Node);
		Parents.push_back(Task.Node);

		Nodes[Task.Node].LeftOrFirst = LeftIndex;
		Nodes[Task.Node].Count = 0;

		Tasks.push_back({ LeftIndex + 1, Task.Depth + 1 });
		Tasks.push_back({ LeftIndex, Task.Depth + 1 });
	}

	// 내부 노드 바운드는 자식으로부터 (자식 인덱스 > 부모 인덱스이므로 역순이면 된다)
	LeafOfPrim.assign(NumPrims, -1);
	for (int32 n = static_cast<int32>(Nodes.size()) - 1; n >= 0; --n)
	{
		if (Nodes[n].IsLeaf())
		{
			for (int32 i = 0; i < Nodes[n].Count; ++i)
			{
				LeafOfPrim[PrimIndices[Nodes[n].LeftOrFirst + i]] = n;
			}
		}
		else
		{
			ComputeInnerBounds(n);
		}
	}

	RefitStamp.assign(Nodes.size(), 0);
	RefitEpoch = 0;
}

void FBoundsBVH::RefitAll(const TArray<FAxisAlignedBoundingBox>& PrimBounds)
{
	for (int32 n = static_cast<int32>(Nodes.size()) - 1; n >= 0; --n)
	{
		if (Nodes[n].IsLeaf())
		{
			ComputeLeafBounds(Nodes[n], PrimBounds);
		}
		else
		{
			ComputeInnerBounds(n);
		}
	}
}

void FBoundsBVH::Refit(const TArray<FAxisAlignedBoundingBox>& PrimBounds, const TArray<int32>& ChangedPrims)
{
	if (Nodes.empty() || ChangedPrims.empty())
	{
		return;
	}

	// 많이 바뀌었으면 전체를 한 번 도는 편이 경로를 여러 번 오르는 것보다 싸다
	if (ChangedPrims.size() * 8 > Nodes.size())
	{
		RefitAll(PrimBounds);
		return;
	}

	// 같은 노드를 여러 번 계산하지 않도록 이번 Refit에서 방문한 노드에 표시한다
	++RefitEpoch;
	if (RefitEpoch == 0)
	{
		std::fill(RefitStamp.begin(), RefitStamp.end(), 0u);
		RefitEpoch = 1;
	}

	// 1) 바뀐 원소가 속한 리프 다시 계산
	TArray<int32> Dirty;
	Dirty.reserve(ChangedPrims.size());
	for (const int32 P : ChangedPrims)
	{
		if (P < 0 || P >= static_cast<int32>(LeafOfPrim.size()))
		{
			continue; // 아직 트리에 없는 원소 (다음 Build에서 들어간다)
		}
		const int32 Leaf = LeafOfPrim[P];
		if (Leaf < 0 || RefitStamp[Leaf] == RefitEpoch)
		{
			continue;
		}
		RefitStamp[Leaf] = RefitEpoch;
		ComputeLeafBounds(Nodes[Leaf], PrimBounds);
		Dirty.push_back(Leaf);
	}

	// 2) 부모를 인덱스 역순(깊은 노드 먼저)으로 계산해 각 노드를 한 번씩만 처리한다
	TArray<int32> Inner;
	for (int32 Leaf : Dirty)
	{
		for (int32 P = Parents[Leaf]; P >= 0 && RefitStamp[P] != RefitEpoch; P = Parents[P])
		{
			RefitStamp[P] = RefitEpoch;
			Inner.push_back(P);
		}
	}
	std::sort(Inner.begin(), Inner.end(), std::greater<int32>());
	for (const int32 N : Inner)
	{
		ComputeInnerBounds(N);
	}
}
