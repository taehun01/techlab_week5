#pragma once

#include "Runtime/Math/FVector.h"
#include "Runtime/Math/FQuaternion.h"
#include "Runtime/Math/FMatrix.h"

struct FTransform
{
	FVector Location{ 0.0, 0.0, 0.0 };
	FQuaternion Rotation{ 0.0, 0.0, 0.0, 1.0 };
	FVector Scale3D{ 1.0, 1.0, 1.0 };

	FMatrix ToMatrix() const;

	// 부모 트랜스폼과 자식 트랜스폼 합성 연산자
	FTransform operator*(const FTransform& Child) const
	{
		FTransform Result;
		//크기 합성
		Result.Scale3D = FVector(Scale3D.X * Child.Scale3D.X, Scale3D.Y * Child.Scale3D.Y, Scale3D.Z * Child.Scale3D.Z);

		//회전 합성
		Result.Rotation = (Rotation * Child.Rotation).Normalized();

		//위치 합성 (부모의 스케일과 회전을 자식 상대 위치에 적용 후 부모 위치에 더함)
		FVector ScaledChildLoc = FVector(Child.Location.X * Scale3D.X, Child.Location.Y * Scale3D.Y, Child.Location.Z * Scale3D.Z);
		Result.Location = Location + Rotation.RotateVector(ScaledChildLoc);

		return Result;
	}

};

inline FMatrix FTransform::ToMatrix() const
{
	FMatrix R = Rotation.ToMatrixRow();
	for (int j = 0; j < 3; ++j)
	{
		R.M[0][j] *= Scale3D.X;
		R.M[1][j] *= Scale3D.Y;
		R.M[2][j] *= Scale3D.Z;
	}
	R.M[3][0] = Location.X;
	R.M[3][1] = Location.Y;
	R.M[3][2] = Location.Z;
	R.M[3][3] = 1.0f;
	return R;
}
