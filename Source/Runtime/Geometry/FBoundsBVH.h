#pragma once

#include "Runtime/Core/TArray.h"
#include "Runtime/Core/IntTypes.h"
#include "Runtime/Geometry/FAxisAlignedBoundingBox.h"
#include "Runtime/Geometry/FRay.h"

#include <cmath>
#include <limits>

// AABB 목록 위에 만드는 BVH (씬 전체용 Top-Level BVH).
// - 원소(Prim)는 0..N-1 인덱스로만 다룬다. 원소의 실제 데이터(컴포넌트, 메시)는 호출하는 쪽이 가진다.
// - Build: Binned SAH로 한 번 만든다. O(N log N)
// - Refit: 트리 구조는 그대로 두고 바뀐 원소의 바운드만 위로 전파한다. 움직이는 오브젝트는 이것으로 처리한다.
// - RaycastClosest: 가까운 자식부터 방문하고, 이미 찾은 충돌(MaxT)보다 먼 노드는 버린다.
class FBoundsBVH
{
public:
	// 노드 32바이트: 캐시라인 하나에 2개. 자식 둘이 연속으로 저장되므로 한 번에 같이 읽힌다.
	struct FNode
	{
		float Min[3];
		int32 LeftOrFirst = -1; // 내부 노드: 왼쪽 자식 인덱스(오른쪽은 +1) / 리프: PrimIndices 시작 위치
		float Max[3];
		int32 Count = 0;        // 0이면 내부 노드, >0이면 리프의 원소 수

		bool IsLeaf() const { return Count > 0; }
	};

	// 광선 순회용 전처리. 나눗셈과 방향 부호 분기를 순회 밖으로 뺀다.
	struct FRayData
	{
		float Origin[3];
		float InvDir[3];
		int32 Neg[3]; // 방향이 음수인 축이면 1 (가까운 면이 Max)

		explicit FRayData(const FRay& Ray)
		{
			for (int32 i = 0; i < 3; ++i)
			{
				const float D = Ray.Direction[i];
				Origin[i] = Ray.Origin[i];
				// 0 방향이면 매우 큰 값으로 대체한다. 1/0 = inf는 0 * inf = NaN을 만들 수 있다.
				InvDir[i] = (std::abs(D) > 1e-20f) ? 1.0f / D : std::copysign(1e30f, D);
				Neg[i] = InvDir[i] < 0.0f ? 1 : 0;
			}
		}
	};

	void Build(const TArray<FAxisAlignedBoundingBox>& PrimBounds);
	// ChangedPrims의 바운드가 바뀌었을 때: 해당 리프에서 루트까지 바운드를 다시 계산한다.
	void Refit(const TArray<FAxisAlignedBoundingBox>& PrimBounds, const TArray<int32>& ChangedPrims);
	// 모든 노드를 다시 계산한다. 바뀐 원소가 많을 때는 이쪽이 더 빠르다.
	void RefitAll(const TArray<FAxisAlignedBoundingBox>& PrimBounds);
	void Clear();

	bool IsEmpty() const { return Nodes.empty(); }
	int32 GetNodeCount() const { return static_cast<int32>(Nodes.size()); }
	int32 GetPrimCount() const { return static_cast<int32>(PrimIndices.size()); }

	// 광선 순회. 박스에 닿은 원소마다 Visit(PrimIndex, InOutMaxT)를 호출한다.
	// Visit가 더 가까운 충돌을 찾으면 InOutMaxT를 줄이고, 그보다 먼 노드는 이후 방문하지 않는다.
	template <typename FVisit>
	void RaycastClosest(const FRay& Ray, float& InOutMaxT, FVisit&& Visit) const;

	// 광선이 박스에 들어가는 거리. 원점이 박스 안이면 0. MaxT보다 멀거나 빗나가면 false.
	static bool IntersectNode(const FRayData& Ray, const float BMin[3], const float BMax[3], float MaxT, float& OutTNear)
	{
		const float* B[2] = { BMin, BMax };
		float TMin = (B[Ray.Neg[0]][0] - Ray.Origin[0]) * Ray.InvDir[0];
		float TMax = (B[1 - Ray.Neg[0]][0] - Ray.Origin[0]) * Ray.InvDir[0];
		const float TyMin = (B[Ray.Neg[1]][1] - Ray.Origin[1]) * Ray.InvDir[1];
		const float TyMax = (B[1 - Ray.Neg[1]][1] - Ray.Origin[1]) * Ray.InvDir[1];
		const float TzMin = (B[Ray.Neg[2]][2] - Ray.Origin[2]) * Ray.InvDir[2];
		const float TzMax = (B[1 - Ray.Neg[2]][2] - Ray.Origin[2]) * Ray.InvDir[2];

		TMin = (std::max)((std::max)(TMin, TyMin), (std::max)(TzMin, 0.0f));
		TMax = (std::min)((std::min)(TMax, TyMax), (std::min)(TzMax, MaxT));
		OutTNear = TMin;
		return TMin <= TMax;
	}

private:
	static constexpr int32 MaxLeafSize = 2;
	static constexpr int32 NumBins = 16;
	static constexpr int32 MaxDepth = 60;
	static constexpr int32 MaxStack = 64;

	TArray<FNode> Nodes;        // Nodes[0] = 루트. 자식은 항상 부모보다 뒤에 있다 (역순 순회로 Refit 가능).
	TArray<int32> Parents;      // 부분 Refit용. 순회에는 쓰지 않으므로 노드와 분리해 둔다.
	TArray<int32> PrimIndices;  // 리프 순서로 재배치된 원소 인덱스
	TArray<int32> LeafOfPrim;   // 원소 -> 그 원소를 가진 리프 노드
	TArray<uint32> RefitStamp;  // 부분 Refit 중복 방지
	uint32 RefitEpoch = 0;

	void ComputeLeafBounds(FNode& Node, const TArray<FAxisAlignedBoundingBox>& PrimBounds) const;
	void ComputeInnerBounds(int32 NodeIndex);
};

template <typename FVisit>
void FBoundsBVH::RaycastClosest(const FRay& Ray, float& InOutMaxT, FVisit&& Visit) const
{
	if (Nodes.empty())
	{
		return;
	}

	const FRayData R(Ray);

	struct FEntry
	{
		int32 Node;
		float TNear;
	};
	FEntry Stack[MaxStack];
	int32 StackSize = 0;

	float T = 0.0f;
	if (!IntersectNode(R, Nodes[0].Min, Nodes[0].Max, InOutMaxT, T))
	{
		return;
	}
	Stack[StackSize++] = { 0, T };

	while (StackSize > 0)
	{
		const FEntry E = Stack[--StackSize];
		if (E.TNear > InOutMaxT)
		{
			continue; // 스택에 넣은 뒤 더 가까운 충돌이 발견된 경우
		}

		const FNode& Node = Nodes[E.Node];
		if (Node.IsLeaf())
		{
			for (int32 i = 0; i < Node.Count; ++i)
			{
				Visit(PrimIndices[Node.LeftOrFirst + i], InOutMaxT);
			}
			continue;
		}

		const int32 L = Node.LeftOrFirst;
		const int32 Rn = L + 1;
		float TL = 0.0f;
		float TR = 0.0f;
		const bool bL = IntersectNode(R, Nodes[L].Min, Nodes[L].Max, InOutMaxT, TL);
		const bool bR = IntersectNode(R, Nodes[Rn].Min, Nodes[Rn].Max, InOutMaxT, TR);

		// 가까운 쪽을 나중에 넣어 먼저 꺼낸다
		if (bL && bR)
		{
			if (TL <= TR)
			{
				Stack[StackSize++] = { Rn, TR };
				Stack[StackSize++] = { L, TL };
			}
			else
			{
				Stack[StackSize++] = { L, TL };
				Stack[StackSize++] = { Rn, TR };
			}
		}
		else if (bL)
		{
			Stack[StackSize++] = { L, TL };
		}
		else if (bR)
		{
			Stack[StackSize++] = { Rn, TR };
		}
	}
}
