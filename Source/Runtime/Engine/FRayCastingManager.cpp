#include "FRayCastingManager.h"
#include "Runtime/Input/FInputManager.h"
#include "Runtime/Math/FMatrix.h"
#include "Runtime/Rendering/FMesh.h"
#include "Runtime/Rendering/FRenderResourceLibrary.h"
#include <limits>
#include "Runtime/CoreUObject/UPrimitiveComponent.h"
#include "Runtime/CoreUObject/UMeshComponent.h"
#include "Runtime/Engine/FSceneOctree.h"
#include "Runtime/Engine/FStaticMeshBVH.h"
#include "Runtime/Core/Log.h"
#include <Runtime\Core\TArray.h>
#include "../Geometry/FAxisAlignedBoundingBox.h"

constexpr float Epsilon = 0.000001f;

FRay FRayCastingManager::CreateRayFromScreenPosition(const FCamera& Camera, const FVector2& MousePosition, const FVector2& ViewportSize)
{
	float ViewportWidth = ViewportSize.X;
	float ViewportHeight = ViewportSize.Y;

	FMatrix InvVP;
	Camera.CreateViewProjectionMatrix().Inverse(InvVP);

	const float screenNdcX = (MousePosition.X / ViewportWidth) * 2.0f - 1.0f;
	const float screenNdcY = 1.0f - (MousePosition.Y / ViewportHeight) * 2.0f;

	// 이 엔진의 투영 행렬:
	// X = depth, Y = screen horizontal, Z = screen vertical
	FVector NearClip{ 0.0f, screenNdcX, screenNdcY };
	FVector FarClip{ 1.0f, screenNdcX, screenNdcY };

	const FVector NearWorld = InvVP.TransformPointRow(NearClip);
	const FVector FarWorld = InvVP.TransformPointRow(FarClip);

	FRay Ray;
	Ray.Origin = NearWorld;
	FVector dir = FarWorld - NearWorld;
	Ray.Direction = dir / dir.Size();
	return Ray;
}

namespace
{
	struct FClosestHit
	{
		float Distance = (std::numeric_limits<float>::max)();
		UMeshComponent* Component = nullptr;
		FVector ImpactPoint;
	};

	// 컴포넌트 하나를 삼각형 단위로 검사해 더 가까우면 갱신한다.
	// CullingFrustum이 주어지면 월드 바운드가 프러스텀 밖인 컴포넌트는 건너뛴다.
	void TestComponent(const FRay& Ray, const FCamera& Camera, UMeshComponent* Component, FClosestHit& Closest,
		const FFrustum* CullingFrustum = nullptr)
	{
		if (!Component)
		{
			return;
		}

		auto Mesh = FRenderResourceLibrary::Get().GetMesh(Component->GetPureRenderData().MeshId);
		if (!Mesh)
		{
			return;
		}

		const FMatrix World = Component->GetRenderMatrix(Camera);

		if (CullingFrustum)
		{
			const FAxisAlignedBoundingBox WorldBounds = { Component->GetLocalBounds(), World };
			if (!CullingFrustum->Intersects(WorldBounds))
			{
				return;
			}
		}

		float HitDistance;
		FVector ImpactPoint;
		if (FRayCastingManager::RayIntersectsMesh(Ray, *Mesh, World, HitDistance, ImpactPoint) &&
			HitDistance < Closest.Distance)
		{
			Closest.Distance = HitDistance;
			Closest.Component = Component;
			Closest.ImpactPoint = ImpactPoint;
		}
	}
}

bool FRayCastingManager::RayIntersectsMeshes(const FRay& Ray, const FCamera& Camera, const TArray<UMeshComponent*>& Components, UMeshComponent*& HitComponent, FVector& OutImpactPoint)
{
	// 전체 순회: 화면 밖 컴포넌트는 프러스텀으로 먼저 걸러낸다.
	const FFrustum CullingFrustum = Camera.CreateFrustum();

	FClosestHit Closest;
	for (UMeshComponent* Component : Components)
	{
		TestComponent(Ray, Camera, Component, Closest, &CullingFrustum);
	}

	HitComponent = Closest.Component;
	OutImpactPoint = Closest.ImpactPoint;
	return Closest.Component != nullptr;
}

bool FRayCastingManager::RaycastScene(const FRay& Ray, const FCamera& Camera, const FSceneOctree& Octree, UMeshComponent*& HitComponent, FVector& OutImpactPoint)
{
	FClosestHit Closest;

	// 1) 옥트리 밖의 카메라 지향 컴포넌트 (개수가 적어 전부 검사)
	for (UMeshComponent* Component : Octree.GetCameraFacingComponents())
	{
		TestComponent(Ray, Camera, Component, Closest);
	}

	// 2) 옥트리 후보를 AABB 진입 거리 오름차순으로 검사
	TArray<FSceneRayCandidate> Candidates;
	Octree.RaycastCandidates(Ray, Candidates);

	for (const FSceneRayCandidate& Candidate : Candidates)
	{
		// 메시의 삼각형은 모두 자기 AABB 안에 있으므로 충돌 거리 >= TNear.
		// 이후 후보는 TNear가 더 크므로 지금 찾은 충돌보다 가까울 수 없다.
		if (Candidate.TNear > Closest.Distance)
		{
			break;
		}
		TestComponent(Ray, Camera, Candidate.Component, Closest);
	}

	HitComponent = Closest.Component;
	OutImpactPoint = Closest.ImpactPoint;
	return Closest.Component != nullptr;
}



bool FRayCastingManager::RayIntersectsAABB(const FRay& Ray, const FAxisAlignedBoundingBox& AABB)
{
	float TNear = 0.0f;
	return IntersectRayAABB(Ray, AABB, TNear);
}

bool FRayCastingManager::RayIntersectsMesh(const FRay& Ray, const FStaticMesh& Mesh, const FMatrix& ModelMatrix, float& OutDistance, FVector& OutImpactPoint)
{
	if (Mesh.GetPositions().size() < 3)
	{
		return false;
	}

	// BVH 루트 바운드가 메시 로컬 AABB와 같으므로 별도의 AABB 사전 검사는 필요 없다.
	return Mesh.GetBVH().Raycast(Ray, ModelMatrix, OutDistance, OutImpactPoint);
}

bool FRayCastingManager::RayIntersectsTriangle(
	const FRay& Ray,
	const FVector& A,
	const FVector& B,
	const FVector& C,
	float& OutT)
{

	const FVector edge1 = B - A;
	const FVector edge2 = C - A;

	// Ray direction × triangle edge
	const FVector pVector = Ray.Direction.Cross(edge2);
	const float determinant = edge1.Dot(pVector);

	// 레이와 삼각형 평면이 평행함
	if (std::fabs(determinant) < Epsilon)
	{
		return false;
	}

	const float inverseDeterminant = 1.0f / determinant;

	// Barycentric u 계산
	const FVector tVector = Ray.Origin - A;
	const float u = tVector.Dot(pVector) * inverseDeterminant;

	if (u < 0.0f || u > 1.0f)
	{
		return false;
	}

	// Barycentric v 계산
	const FVector qVector = tVector.Cross(edge1);
	const float v = Ray.Direction.Dot(qVector) * inverseDeterminant;

	if (v < 0.0f || u + v > 1.0f)
	{
		return false;
	}

	// 레이 시작점으로부터 교점까지의 거리
	OutT = edge2.Dot(qVector) * inverseDeterminant;

	// t가 음수면 카메라/레이 시작점 뒤에 있는 삼각형
	return OutT > Epsilon;
}
