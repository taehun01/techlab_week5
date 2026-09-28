#pragma once

#include "../Math/FVector.h"

struct FPlane
{
	FVector Normal = { 0.0f, 1.0f, 0.0f };
	float Distance = 0.0f;

	FPlane() = default;

	FPlane(const FVector& InPoint, const FVector& InNormal)
	{
		Normal = InNormal / InNormal.Size();
		Distance = Normal.Dot(InPoint);
	}
};

struct FFrustum
{
	FPlane Top;
	FPlane Bottom;
	FPlane Left;
	FPlane Right;
	FPlane Near;
	FPlane Far;
};