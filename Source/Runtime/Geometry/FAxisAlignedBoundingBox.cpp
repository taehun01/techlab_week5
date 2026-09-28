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

FAxisAlignedBoundingBox::FAxisAlignedBoundingBox(const FAxisAlignedBoundingBox& LocalBox, const FMatrix& ModelMatrix)
{
	FVector Vertices[8] = {
		FVector(LocalBox.Min.X, LocalBox.Min.Y, LocalBox.Min.Z),
		FVector(LocalBox.Min.X, LocalBox.Min.Y, LocalBox.Max.Z),
		FVector(LocalBox.Min.X, LocalBox.Max.Y, LocalBox.Min.Z),
		FVector(LocalBox.Min.X, LocalBox.Max.Y, LocalBox.Max.Z),
		FVector(LocalBox.Max.X, LocalBox.Min.Y, LocalBox.Min.Z),
		FVector(LocalBox.Max.X, LocalBox.Min.Y, LocalBox.Max.Z),
		FVector(LocalBox.Max.X, LocalBox.Max.Y, LocalBox.Min.Z),
		FVector(LocalBox.Max.X, LocalBox.Max.Y, LocalBox.Max.Z)
	};

	for (FVector& Vertex : Vertices)
	{
		FVector WorldVertex = ModelMatrix.TransformPointRow(Vertex);

		for (int j = 0; j < 3; ++j)
		{
			Min[j] = std::min(WorldVertex[j], Min[j]);
			Max[j] = std::max(WorldVertex[j], Max[j]);
		}
	}
}

FVector FAxisAlignedBoundingBox::GetCenter() const
{
	return (Min + Max) * 0.5f;
}

FVector FAxisAlignedBoundingBox::GetExtents() const
{
	return (Max - Min) * 0.5f;
}