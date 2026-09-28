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
