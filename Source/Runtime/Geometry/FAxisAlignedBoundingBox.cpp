#include "FAxisAlignedBoundingBox.h"
#include "Runtime/Rendering/FMesh.h"
#include "Runtime/Math/FVector.h"
#include "Runtime/Math/FMatrix.h"

#include <algorithm>
#include <cmath>

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
	// 유효한 박스는 중심/반경 변환(Arvo)으로 구한다. 8개 꼭짓점을 변환해 min/max를 구한 것과 같은 결과를
	// 점 변환 1번 + 3x3 절댓값 곱으로 얻는다. (행벡터 규약: p' = p * M)
	if (LocalBox.Min.X <= LocalBox.Max.X && LocalBox.Min.Y <= LocalBox.Max.Y && LocalBox.Min.Z <= LocalBox.Max.Z)
	{
		const FVector LocalCenter = (LocalBox.Min + LocalBox.Max) * 0.5f;
		const FVector LocalExtent = (LocalBox.Max - LocalBox.Min) * 0.5f;
		const FVector WorldCenter = ModelMatrix.TransformPointRow(LocalCenter);

		FVector WorldExtent;
		for (int j = 0; j < 3; ++j)
		{
			WorldExtent[j] = std::abs(ModelMatrix.M[0][j]) * LocalExtent.X
				+ std::abs(ModelMatrix.M[1][j]) * LocalExtent.Y
				+ std::abs(ModelMatrix.M[2][j]) * LocalExtent.Z;
		}

		Min = WorldCenter - WorldExtent;
		Max = WorldCenter + WorldExtent;
		return;
	}

	// 비어 있는(무효) 박스는 기존 방식 그대로 처리한다
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