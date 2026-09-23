#include "FRayCastingManager.h"
#include "Runtime/Input/FInputManager.h"
#include "Runtime/Math/FMatrix.h"
#include "Runtime/Rendering/FMesh.h"
#include "Runtime/Rendering/FRenderResourceLibrary.h"
#include <limits>
#include "Runtime/CoreUObject/UPrimitiveComponent.h"
#include "Runtime/CoreUObject/UMeshComponent.h"
#include "Runtime/Core/Log.h"
#include <Runtime\Core\TArray.h>

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

bool FRayCastingManager::RayIntersectsMeshes(const FRay& Ray, const FCamera& Camera, const TArray<UMeshComponent*>& Components, UMeshComponent*& HitComponent, FVector& OutImpactPoint)
{
	HitComponent = nullptr;

	float ClosestHit = (std::numeric_limits<float>::max)();
	UMeshComponent* ClosestComponent = nullptr;
	FVector ClosestImpactPoint;

	for (UMeshComponent* Component : Components)
	{
		if (!Component)
		{
			continue;
		}

		auto Mesh = FRenderResourceLibrary::Get().GetMesh(Component->GetPureRenderData().MeshId);
		if (!Mesh)
		{
			continue;
		}

		FMatrix World = Component->GetRenderMatrix(Camera);

		float HitDistance;
		FVector ImpactPoint;
		if (RayIntersectsMesh(Ray, *Mesh, World, HitDistance, ImpactPoint) &&
			HitDistance < ClosestHit)
		{
			ClosestHit = HitDistance;
			ClosestComponent = Component;
			ClosestImpactPoint = ImpactPoint;
		}
	}

	HitComponent = ClosestComponent;
	OutImpactPoint = ClosestImpactPoint;

	return ClosestComponent != nullptr;
}



bool FRayCastingManager::RayIntersectsAABB(const FRay& Ray, const FAxisAlignedBoundingBox& AABB)
{
	float TNear = 0.0f;
	float TFar = (std::numeric_limits<float>::max)();

	for (int i = 0; i < 3; ++i)
	{
		// 광선이 축과 평행한 경우
		if (std::abs(Ray.Direction[i]) < Epsilon)
		{
			// 시작점이 상자 범위 밖이면 제외
			if (Ray.Origin[i] < AABB.Min[i] || Ray.Origin[i] > AABB.Max[i])
			{
				return false;
			}
			continue;
		}

		// 교점 거리 계산
		float T0 = (AABB.Min[i] - Ray.Origin[i]) / Ray.Direction[i];
		float T1 = (AABB.Max[i] - Ray.Origin[i]) / Ray.Direction[i];

		if (T0 > T1)
		{
			std::swap(T0, T1);
		}

		TNear = std::max(TNear, T0);
		TFar = std::min(TFar, T1);

		if (TNear > TFar)
		{
			return false;
		}
	}

	return true;
}

bool FRayCastingManager::RayIntersectsMesh(const FRay& Ray, const FStaticMesh& Mesh, const FMatrix& ModelMatrix, float& OutDistance, FVector& OutImpactPoint)
{
	const auto& Positions = Mesh.GetPositions();
	const auto& Indices = Mesh.GetIndices();

	if (Positions.size() < 3)
	{
		return false;
	}
	
	// Ray를 Object 좌표계로 변환
	FMatrix InvM;
	if (!ModelMatrix.Inverse(InvM))
	{
		return false;
	}

	const FVector ObjectOrigin = InvM.TransformPointRow(Ray.Origin);
	const FVector ObjectDirection = InvM.TransformPointRow(Ray.Direction, 0.0f); // 1.0은 점을 나타내므로 0.0으로 하여 벡터로 유지
	const FRay ObjectRay{ ObjectOrigin, ObjectDirection };

	FAxisAlignedBoundingBox AABB = Mesh.GetLocalBounds();
	if (!RayIntersectsAABB(ObjectRay, AABB))
	{
		return false;
	}

	const uint32 elementCount = Mesh.HasIndices()
		? static_cast<uint32>(Indices.size())
		: static_cast<uint32>(Positions.size());

	float ClosestHit = (std::numeric_limits<float>::max)();
	FVector ClosestImpactPoint;
	bool bHit = false;
	for (uint32 i = 0; i + 2 < elementCount; i += 3)
	{
		const uint32 i0 = Mesh.HasIndices() ? Indices[i] : i;
		const uint32 i1 = Mesh.HasIndices() ? Indices[i + 1] : i + 1;
		const uint32 i2 = Mesh.HasIndices() ? Indices[i + 2] : i + 2;

		// 잘못된 인덱스 방어
		if (i0 >= Positions.size() ||
			i1 >= Positions.size() ||
			i2 >= Positions.size())
		{
			continue;
		}

		FVector A = Positions[i0];
		FVector B = Positions[i1];
		FVector C = Positions[i2];

		float HitT = 0.0f;
		if (RayIntersectsTriangle(ObjectRay, A, B, C, HitT) &&
			HitT < ClosestHit)
		{
			ClosestHit = HitT;
			ClosestImpactPoint = Ray.Origin + Ray.Direction * HitT;
			bHit = true;
		}
	}

	OutDistance = ClosestHit;
	OutImpactPoint = ClosestImpactPoint;
	
	return bHit;
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
