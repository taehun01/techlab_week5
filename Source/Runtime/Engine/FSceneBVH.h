#pragma once

#include "Runtime/Core/TArray.h"
#include "Runtime/Core/TMap.h"
#include "Runtime/Core/TSet.h"
#include "Runtime/Core/PointerTypes.h"
#include "Runtime/Geometry/FBoundsBVH.h"
#include "Runtime/Math/FMatrix.h"

class UMeshComponent;
class FStaticMesh;

// 씬 전체를 BVH 하나로 관리하는 피킹 가속 구조 (TLAS).
// 메시 내부 삼각형은 각 FStaticMesh의 BVH(BLAS)가 담당하고, 여기서는 "어느 인스턴스를 검사할지"만 고른다.
//
// 원소 하나(Proxy)에 피킹에 필요한 값을 미리 담아 둔다:
//   컴포넌트 포인터, 메시 포인터, 월드 역행렬, 월드 AABB
// → 피킹 중에는 FName 문자열 조회, 부모 트랜스폼 재귀, 역행렬 계산, UObject 조회가 일어나지 않는다.
// 이 값들은 트랜스폼이 바뀐 컴포넌트(MarkDirty)만 Flush에서 다시 계산한다. (렌더 결과 캐시가 아니라 파생 데이터)
class FSceneBVH
{
public:
	void Register(UMeshComponent* Component);
	void Unregister(UMeshComponent* Component);
	void MarkDirty(UMeshComponent* Component);

	// 예약된 변경을 반영한다. 원소가 새로 늘었으면 재빌드, 트랜스폼만 바뀌었으면 Refit.
	void Flush();
	void Clear();
	// 오브젝트가 많이 움직여 트리 품질이 떨어졌을 때 다음 Flush에서 새로 빌드하도록 요청한다.
	void RequestRebuild() { bNeedsRebuild = true; }

	// 가장 가까운 메시 삼각형을 찾는다. InOutMaxT보다 먼 충돌은 무시하며, 찾으면 InOutMaxT를 줄인다.
	// (카메라 지향 컴포넌트를 먼저 검사한 결과를 이어받을 수 있다)
	bool RaycastClosest(const FRay& WorldRay, float& InOutMaxT, UMeshComponent*& OutComponent) const;

	const TArray<UMeshComponent*>& GetCameraFacingComponents() const { return CameraFacingComponents; }
	int32 GetNodeCount() const { return Tree.GetNodeCount(); }

private:
	struct FPickProxy
	{
		UMeshComponent* Component = nullptr;   // nullptr이면 빈 슬롯 (삭제된 원소)
		TSharedPtr<FStaticMesh> Mesh;          // 수명 보장용. 피킹 중에는 get()으로만 접근 (참조 카운트 변화 없음)
		FMatrix InvWorld;
	};

	FBoundsBVH Tree;
	TArray<FAxisAlignedBoundingBox> Bounds; // Tree의 원소 인덱스와 같은 순서
	TArray<FPickProxy> Proxies;
	TMap<UMeshComponent*, int32> ProxyIndexOf;
	TArray<int32> FreeSlots;

	TSet<UMeshComponent*> DirtyComponents;
	TArray<int32> ChangedPrims; // Flush 작업 배열 (용량 유지)
	bool bNeedsRebuild = false;

	TArray<UMeshComponent*> CameraFacingComponents;

	void UpdateProxy(int32 Index);
};
