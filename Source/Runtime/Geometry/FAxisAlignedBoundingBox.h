#pragma once

#include "Runtime/Math/FVector.h"
#include "Runtime/Core/TArray.h"

#include <limits>

class FStaticMesh;
struct FMatrix;

struct FAxisAlignedBoundingBox
{
	FVector Min
	{
		std::numeric_limits<float>::max(),
		std::numeric_limits<float>::max(),
		std::numeric_limits<float>::max(),
	};

	FVector Max
	{
		std::numeric_limits<float>::lowest(),
		std::numeric_limits<float>::lowest(),
		std::numeric_limits<float>::lowest(),
	};

	FAxisAlignedBoundingBox() = default;
	FAxisAlignedBoundingBox(const FStaticMesh& Mesh);
	FAxisAlignedBoundingBox(const FStaticMesh& Mesh, const FMatrix& ModelMatrix);	
};