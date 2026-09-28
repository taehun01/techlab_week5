#pragma once

#include "Runtime/Math/FVector.h"

struct FAxisAlignedBoundingBox;

struct FRay
{
	FVector Origin;
	FVector Direction;
};

// 광선과 AABB 교차 판정 (slab 방식). 광선 시작점 뒤쪽(t < 0)은 무시한다.
// 교차하면 광선이 박스에 들어가는 거리를 OutTNear에 담는다. 시작점이 박스 안이면 0.
bool IntersectRayAABB(const FRay& Ray, const FAxisAlignedBoundingBox& Box, float& OutTNear);
