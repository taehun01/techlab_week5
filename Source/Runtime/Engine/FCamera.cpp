#include "FCamera.h"

FMatrix FCamera::GetRotationMatrix() const
{
	// 카메라 회전 행렬 반환
	return FMatrix::MakeRotation(FVector(0.0f, Pitch, Yaw));
}

FMatrix FCamera::GetViewMatrix() const
{
	return FMatrix::MakeTranslation(-Position) * GetRotationMatrix().Transpose();
}

FMatrix FCamera::GetProjectionMatrix() const
{
	// 카메라 투영 행렬 반환
	return Projection.CreateProjectionMatrix();
}

FMatrix FCamera::CreateViewProjectionMatrix() const
{
	// 뷰 행렬 계산
	const FMatrix ViewMatrix = GetViewMatrix();
	const FMatrix ProjectionMatrix = GetProjectionMatrix();

	return ViewMatrix * ProjectionMatrix;
}

FFrustum FCamera::CreateFrustum() const
{
	FFrustum Frustum;

	const FMatrix Rotation = GetRotationMatrix();
	const FVector Forward = FVector{ Rotation.M[0][0], Rotation.M[0][1], Rotation.M[0][2] };
	const FVector Right = FVector{ Rotation.M[1][0], Rotation.M[1][1], Rotation.M[1][2] };
	const FVector Up = FVector{ Rotation.M[2][0], Rotation.M[2][1], Rotation.M[2][2] };
	const float FovRadian = Projection.FOV * std::numbers::pi_v<float> / 180.0f;
	const float HalfHeight = std::tanf(FovRadian * 0.5f) * Projection.FarZ;
	const float HalfWidth = Projection.Aspect * HalfHeight;
	const FVector ForwardMultFar = Forward * Projection.FarZ;
	const FVector RightMultHalfWidth = Right * HalfWidth;
	const FVector UpMultHalfHeight = Up * HalfHeight;

	Frustum.Near = { Position + Forward * Projection.NearZ, Forward };
	Frustum.Far = { Position + ForwardMultFar, -Forward };
	Frustum.Left = { Position, Up.Cross(ForwardMultFar - RightMultHalfWidth) };
	Frustum.Right = { Position, (ForwardMultFar + RightMultHalfWidth).Cross(Up) };
	Frustum.Top = { Position, Right.Cross(ForwardMultFar + UpMultHalfHeight)};
	Frustum.Bottom = {Position, (ForwardMultFar - UpMultHalfHeight).Cross(Right)};

	return Frustum;
}