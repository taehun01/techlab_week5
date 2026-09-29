#include "FRay.h"
#include "FAxisAlignedBoundingBox.h"

#include <algorithm>
#include <cmath>
#include <limits>

bool IntersectRayAABB(const FRay& Ray, const FAxisAlignedBoundingBox& Box, float& OutTNear)
{
	constexpr float Epsilon = 0.000001f;

	float TNear = 0.0f;
	float TFar = (std::numeric_limits<float>::max)();

	for (int32 i = 0; i < 3; ++i)
	{
		// 광선이 축과 평행한 경우
		if (std::abs(Ray.Direction[i]) < Epsilon)
		{
			// 시작점이 상자 범위 밖이면 제외
			if (Ray.Origin[i] < Box.Min[i] || Ray.Origin[i] > Box.Max[i])
			{
				return false;
			}
			continue;
		}

		// 교점 거리 계산
		float T0 = (Box.Min[i] - Ray.Origin[i]) / Ray.Direction[i];
		float T1 = (Box.Max[i] - Ray.Origin[i]) / Ray.Direction[i];

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

bool IntersectRayTriangle(const FRay& Ray, const FVector& A, const FVector& B, const FVector& C, float& OutT)
{
	constexpr float Epsilon = 0.000001f;

	const FVector Edge1 = B - A;
	const FVector Edge2 = C - A;

	const FVector PVector = Ray.Direction.Cross(Edge2);
	const float Determinant = Edge1.Dot(PVector);

	// 광선과 삼각형 평면이 평행함
	if (std::abs(Determinant) < Epsilon)
	{
		return false;
	}

	const float InverseDeterminant = 1.0f / Determinant;

	// Barycentric u
	const FVector TVector = Ray.Origin - A;
	const float U = TVector.Dot(PVector) * InverseDeterminant;
	if (U < 0.0f || U > 1.0f)
	{
		return false;
	}

	// Barycentric v
	const FVector QVector = TVector.Cross(Edge1);
	const float V = Ray.Direction.Dot(QVector) * InverseDeterminant;
	if (V < 0.0f || U + V > 1.0f)
	{
		return false;
	}

	OutT = Edge2.Dot(QVector) * InverseDeterminant;
	return OutT > Epsilon;
}
