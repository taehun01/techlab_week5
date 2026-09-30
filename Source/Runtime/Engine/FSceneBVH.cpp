#include "FSceneBVH.h"

#include "Runtime/CoreUObject/UMeshComponent.h"
#include "Runtime/Engine/FStaticMeshBVH.h"
#include "Runtime/Rendering/FMesh.h"
#include "Runtime/Rendering/FRenderResourceLibrary.h"

#include <algorithm>

void FSceneBVH::Register(UMeshComponent* Component)
{
	if (!Component || ProxyIndexOf.contains(Component))
	{
		return;
	}

	if (Component->IsCameraFacing())
	{
		CameraFacingComponents.push_back(Component);
		return;
	}

	int32 Index;
	if (!FreeSlots.empty())
	{
		// 빈 슬롯 재사용: 트리 구조는 그대로 두고 Refit만 하면 된다
		Index = FreeSlots.back();
		FreeSlots.pop_back();
	}
	else
	{
		// 새 원소는 트리에 없으므로 다음 Flush에서 재빌드
		Index = static_cast<int32>(Proxies.size());
		Proxies.emplace_back();
		Bounds.emplace_back();
		bNeedsRebuild = true;
	}

	Proxies[Index].Component = Component;
	ProxyIndexOf[Component] = Index;
	DirtyComponents.insert(Component);
}

void FSceneBVH::Unregister(UMeshComponent* Component)
{
	if (!Component)
	{
		return;
	}

	if (Component->IsCameraFacing())
	{
		std::erase(CameraFacingComponents, Component);
		return;
	}

	auto It = ProxyIndexOf.find(Component);
	if (It == ProxyIndexOf.end())
	{
		return;
	}

	// 인덱스를 당기면 트리 전체의 인덱스가 틀어지므로, 빈 슬롯으로 남기고 바운드를 비운다
	const int32 Index = It->second;
	ProxyIndexOf.erase(It);
	DirtyComponents.erase(Component);

	Proxies[Index] = FPickProxy{};
	Bounds[Index] = FAxisAlignedBoundingBox{}; // 무효 박스: 광선과 절대 교차하지 않는다
	FreeSlots.push_back(Index);
	ChangedPrims.push_back(Index);
}

void FSceneBVH::MarkDirty(UMeshComponent* Component)
{
	if (Component && !Component->IsCameraFacing())
	{
		DirtyComponents.insert(Component);
	}
}

void FSceneBVH::Clear()
{
	Tree.Clear();
	Bounds.clear();
	Proxies.clear();
	ProxyIndexOf.clear();
	FreeSlots.clear();
	DirtyComponents.clear();
	ChangedPrims.clear();
	CameraFacingComponents.clear();
	bNeedsRebuild = false;
}

void FSceneBVH::UpdateProxy(int32 Index)
{
	FPickProxy& Proxy = Proxies[Index];
	UMeshComponent* Component = Proxy.Component;

	// 옥트리(CalcWorldBounds)와 같은 월드 행렬 / 바운드 규칙
	const FMatrix World = Component->GetGlobalTransform().ToMatrix();
	Proxy.Mesh = FRenderResourceLibrary::Get().GetMesh(Component->GetPureRenderData().MeshId);

	if (!Proxy.Mesh || !World.Inverse(Proxy.InvWorld))
	{
		// 검사할 메시가 없거나 스케일 0: 피킹 대상에서 빠진다
		Proxy.Mesh.reset();
		Bounds[Index] = FAxisAlignedBoundingBox{};
		return;
	}

	const FAxisAlignedBoundingBox Local = Component->CalcLocalBounds();
	Bounds[Index] = FAxisAlignedBoundingBox(Local, World);
}

void FSceneBVH::Flush()
{
	for (UMeshComponent* Component : DirtyComponents)
	{
		auto It = ProxyIndexOf.find(Component);
		if (It != ProxyIndexOf.end())
		{
			UpdateProxy(It->second);
			ChangedPrims.push_back(It->second);
		}
	}
	DirtyComponents.clear();

	if (bNeedsRebuild)
	{
		Tree.Build(Bounds);
		bNeedsRebuild = false;
	}
	else if (!ChangedPrims.empty())
	{
		Tree.Refit(Bounds, ChangedPrims);
	}
	ChangedPrims.clear();
}

bool FSceneBVH::RaycastClosest(const FRay& WorldRay, float& InOutMaxT, UMeshComponent*& OutComponent) const
{
	int32 BestIndex = -1;

	Tree.RaycastClosest(WorldRay, InOutMaxT, [&](int32 Index, float& MaxT)
	{
		const FPickProxy& Proxy = Proxies[Index];
		const FStaticMesh* Mesh = Proxy.Mesh.get();
		if (!Mesh)
		{
			return;
		}

		float T;
		if (Mesh->GetBVH().RaycastInverse(WorldRay, Proxy.InvWorld, MaxT, T) && T < MaxT)
		{
			MaxT = T;
			BestIndex = Index;
		}
	});

	if (BestIndex < 0)
	{
		return false;
	}
	OutComponent = Proxies[BestIndex].Component;
	return true;
}
