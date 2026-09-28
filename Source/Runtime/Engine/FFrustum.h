#pragma once

#include "../Math/FVector.h"
#include "../Geometry/FAxisAlignedBoundingBox.h"

struct FPlane
{
	FVector Normal = { 0.0f, 1.0f, 0.0f };
	float Distance = 0.0f;

	FPlane() = default;

	FPlane(const FVector& InPoint, const FVector& InNormal);

	[[nodiscard]] float GetSignedDistanceToPlane(const FVector& Point) const;
};

struct FFrustum
{
	FPlane Top;
	FPlane Bottom;
	FPlane Left;
	FPlane Right;
	FPlane Near;
	FPlane Far;

	[[nodiscard]] bool Intersects(const FAxisAlignedBoundingBox& Box) const;
	[[nodiscard]] bool BoxIsOnOrForwardPlane(const FVector& BoxCenter, const FVector& BoxExtents, const FPlane& Plane) const;
};