#include "FAxisAlignedBoundingBox.h"
#include "Runtime/Rendering/FMesh.h"
#include "Runtime/Math/FVector.h"
#include "Runtime/Math/FMatrix.h"

#include <algorithm>

FAxisAlignedBoundingBox::FAxisAlignedBoundingBox(const FStaticMesh& Mesh)
{	
	for (auto& Item : Mesh.GetPositions())
	{
		for (int i = 0; i < 3; ++i)
		{
			Min[i] = std::min(Item[i], Min[i]);
			Max[i] = std::max(Item[i], Max[i]);
		}
	}
}

FAxisAlignedBoundingBox::FAxisAlignedBoundingBox(const FStaticMesh& Mesh, const FMatrix& ModelMatrix)
{
	for (auto& Item : Mesh.GetPositions())
	{
		FVector WorldVector = ModelMatrix.TransformPointRow(Item);

		for (int i = 0; i < 3; ++i)
		{
			Min[i] = std::min(WorldVector[i], Min[i]);
			Max[i] = std::max(WorldVector[i], Max[i]);
		}
	}
}