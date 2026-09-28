#pragma once

#include "FVector.h"
#include <numbers>
#include <cmath>
#include <immintrin.h>

// TODO: 스타일 정리 필요
struct FMatrix
{
	float M[4][4];

	static const FMatrix Identity;

	[[nodiscard]] FMatrix() = default;
	[[nodiscard]] FMatrix(const FVector& InX, const FVector& InY, const FVector& InZ, const FVector& InW);
	[[nodiscard]] explicit FMatrix(float N);

	inline static FMatrix GetIdentity()
	{
		FMatrix t;

		for (int i = 0; i < 4; ++i)
			for (int j = 0; j < 4; ++j)
				t.M[i][j] = (i == j) ? 1.0f : 0.0f;

		return t;
	}

	inline FMatrix Transpose() const
	{
		__m128 R0 = _mm_loadu_ps(M[0]);
		__m128 R1 = _mm_loadu_ps(M[1]);
		__m128 R2 = _mm_loadu_ps(M[2]);
		__m128 R3 = _mm_loadu_ps(M[3]);

		_MM_TRANSPOSE4_PS(R0, R1, R2, R3);

		FMatrix Result;
		_mm_storeu_ps(Result.M[0], R0);
		_mm_storeu_ps(Result.M[1], R1);
		_mm_storeu_ps(Result.M[2], R2);
		_mm_storeu_ps(Result.M[3], R3);
		return Result;
	}

	// 2x2 행렬식 12개를 한 번만 구해서 행렬식과 역행렬에 같이 쓴다.
	inline bool Inverse(FMatrix& Dst) const
	{
		const __m128 R0 = _mm_loadu_ps(M[0]);
		const __m128 R1 = _mm_loadu_ps(M[1]);
		const __m128 R2 = _mm_loadu_ps(M[2]);
		const __m128 R3 = _mm_loadu_ps(M[3]);

		// 열 쌍 (0,1) (0,2) (0,3) (1,2) (1,3) (2,3) 순서
		// 행 0,1: S0123 = [s0 s1 s2 s3], S45 = [s4 s5 s4 s5]
		const __m128 S0123 = _mm_sub_ps(
			_mm_mul_ps(_mm_shuffle_ps(R0, R0, _MM_SHUFFLE(1, 0, 0, 0)), _mm_shuffle_ps(R1, R1, _MM_SHUFFLE(2, 3, 2, 1))),
			_mm_mul_ps(_mm_shuffle_ps(R1, R1, _MM_SHUFFLE(1, 0, 0, 0)), _mm_shuffle_ps(R0, R0, _MM_SHUFFLE(2, 3, 2, 1))));
		const __m128 S45 = _mm_sub_ps(
			_mm_mul_ps(_mm_shuffle_ps(R0, R0, _MM_SHUFFLE(2, 1, 2, 1)), _mm_shuffle_ps(R1, R1, _MM_SHUFFLE(3, 3, 3, 3))),
			_mm_mul_ps(_mm_shuffle_ps(R1, R1, _MM_SHUFFLE(2, 1, 2, 1)), _mm_shuffle_ps(R0, R0, _MM_SHUFFLE(3, 3, 3, 3))));
		// 행 2,3: C0123 = [c0 c1 c2 c3], C45 = [c4 c5 c4 c5]
		const __m128 C0123 = _mm_sub_ps(
			_mm_mul_ps(_mm_shuffle_ps(R2, R2, _MM_SHUFFLE(1, 0, 0, 0)), _mm_shuffle_ps(R3, R3, _MM_SHUFFLE(2, 3, 2, 1))),
			_mm_mul_ps(_mm_shuffle_ps(R3, R3, _MM_SHUFFLE(1, 0, 0, 0)), _mm_shuffle_ps(R2, R2, _MM_SHUFFLE(2, 3, 2, 1))));
		const __m128 C45 = _mm_sub_ps(
			_mm_mul_ps(_mm_shuffle_ps(R2, R2, _MM_SHUFFLE(2, 1, 2, 1)), _mm_shuffle_ps(R3, R3, _MM_SHUFFLE(3, 3, 3, 3))),
			_mm_mul_ps(_mm_shuffle_ps(R3, R3, _MM_SHUFFLE(2, 1, 2, 1)), _mm_shuffle_ps(R2, R2, _MM_SHUFFLE(3, 3, 3, 3))));

		// det = s0*c5 - s1*c4 + s2*c3 + s3*c2 - s4*c1 + s5*c0
		__m128 P = _mm_mul_ps(S0123, _mm_shuffle_ps(C45, C0123, _MM_SHUFFLE(2, 3, 0, 1)));
		__m128 Q = _mm_mul_ps(S45, _mm_shuffle_ps(C0123, C0123, _MM_SHUFFLE(0, 1, 0, 1)));
		P = _mm_mul_ps(P, _mm_setr_ps(1.0f, -1.0f, 1.0f, 1.0f));
		Q = _mm_mul_ps(Q, _mm_setr_ps(-1.0f, 1.0f, 0.0f, 0.0f));
		__m128 T = _mm_add_ps(P, Q);
		T = _mm_add_ps(T, _mm_movehl_ps(T, T));
		T = _mm_add_ss(T, _mm_shuffle_ps(T, T, _MM_SHUFFLE(1, 1, 1, 1)));
		const float Det = _mm_cvtss_f32(T);
		if (fabsf(Det) < 1e-8f)
			return false;

		// Kk = [ck ck sk sk]: 결과의 왼쪽 두 열은 c, 오른쪽 두 열은 s를 씀
		const __m128 K0 = _mm_shuffle_ps(C0123, S0123, _MM_SHUFFLE(0, 0, 0, 0));
		const __m128 K1 = _mm_shuffle_ps(C0123, S0123, _MM_SHUFFLE(1, 1, 1, 1));
		const __m128 K2 = _mm_shuffle_ps(C0123, S0123, _MM_SHUFFLE(2, 2, 2, 2));
		const __m128 K3 = _mm_shuffle_ps(C0123, S0123, _MM_SHUFFLE(3, 3, 3, 3));
		const __m128 K4 = _mm_shuffle_ps(C45, S45, _MM_SHUFFLE(0, 0, 0, 0));
		const __m128 K5 = _mm_shuffle_ps(C45, S45, _MM_SHUFFLE(1, 1, 1, 1));

		// Vj = [M[1][j] M[0][j] M[3][j] M[2][j]]: 전치한 행을 (1,0,3,2) 순서로
		__m128 V0 = R0, V1 = R1, V2 = R2, V3 = R3;
		_MM_TRANSPOSE4_PS(V0, V1, V2, V3);
		V0 = _mm_shuffle_ps(V0, V0, _MM_SHUFFLE(2, 3, 0, 1));
		V1 = _mm_shuffle_ps(V1, V1, _MM_SHUFFLE(2, 3, 0, 1));
		V2 = _mm_shuffle_ps(V2, V2, _MM_SHUFFLE(2, 3, 0, 1));
		V3 = _mm_shuffle_ps(V3, V3, _MM_SHUFFLE(2, 3, 0, 1));

		// 부호와 1/Det를 한 번에 곱함
		const float InvDet = 1.0f / Det;
		const __m128 PosNeg = _mm_setr_ps(InvDet, -InvDet, InvDet, -InvDet);
		const __m128 NegPos = _mm_setr_ps(-InvDet, InvDet, -InvDet, InvDet);

		const __m128 Out0 = _mm_add_ps(_mm_sub_ps(_mm_mul_ps(V1, K5), _mm_mul_ps(V2, K4)), _mm_mul_ps(V3, K3));
		const __m128 Out1 = _mm_add_ps(_mm_sub_ps(_mm_mul_ps(V0, K5), _mm_mul_ps(V2, K2)), _mm_mul_ps(V3, K1));
		const __m128 Out2 = _mm_add_ps(_mm_sub_ps(_mm_mul_ps(V0, K4), _mm_mul_ps(V1, K2)), _mm_mul_ps(V3, K0));
		const __m128 Out3 = _mm_add_ps(_mm_sub_ps(_mm_mul_ps(V0, K3), _mm_mul_ps(V1, K1)), _mm_mul_ps(V2, K0));

		// 입력을 모두 읽은 뒤 저장하므로 m.Inverse(m)도 안전
		_mm_storeu_ps(Dst.M[0], _mm_mul_ps(Out0, PosNeg));
		_mm_storeu_ps(Dst.M[1], _mm_mul_ps(Out1, NegPos));
		_mm_storeu_ps(Dst.M[2], _mm_mul_ps(Out2, PosNeg));
		_mm_storeu_ps(Dst.M[3], _mm_mul_ps(Out3, NegPos));
		return true;
	}


	//행 0,1의 2x2 행렬식 × 행 2,3의 상보 2x2 행렬식
	inline float Determinant() const
	{
		const __m128 R0 = _mm_loadu_ps(M[0]);
		const __m128 R1 = _mm_loadu_ps(M[1]);
		const __m128 R2 = _mm_loadu_ps(M[2]);
		const __m128 R3 = _mm_loadu_ps(M[3]);

		const __m128 S0123 = _mm_sub_ps(
			_mm_mul_ps(_mm_shuffle_ps(R0, R0, _MM_SHUFFLE(1, 0, 0, 0)), _mm_shuffle_ps(R1, R1, _MM_SHUFFLE(2, 3, 2, 1))),
			_mm_mul_ps(_mm_shuffle_ps(R1, R1, _MM_SHUFFLE(1, 0, 0, 0)), _mm_shuffle_ps(R0, R0, _MM_SHUFFLE(2, 3, 2, 1))));
		const __m128 S45 = _mm_sub_ps(
			_mm_mul_ps(_mm_shuffle_ps(R0, R0, _MM_SHUFFLE(2, 1, 2, 1)), _mm_shuffle_ps(R1, R1, _MM_SHUFFLE(3, 3, 3, 3))),
			_mm_mul_ps(_mm_shuffle_ps(R1, R1, _MM_SHUFFLE(2, 1, 2, 1)), _mm_shuffle_ps(R0, R0, _MM_SHUFFLE(3, 3, 3, 3))));
		const __m128 C0123 = _mm_sub_ps(
			_mm_mul_ps(_mm_shuffle_ps(R2, R2, _MM_SHUFFLE(1, 0, 0, 0)), _mm_shuffle_ps(R3, R3, _MM_SHUFFLE(2, 3, 2, 1))),
			_mm_mul_ps(_mm_shuffle_ps(R3, R3, _MM_SHUFFLE(1, 0, 0, 0)), _mm_shuffle_ps(R2, R2, _MM_SHUFFLE(2, 3, 2, 1))));
		const __m128 C45 = _mm_sub_ps(
			_mm_mul_ps(_mm_shuffle_ps(R2, R2, _MM_SHUFFLE(2, 1, 2, 1)), _mm_shuffle_ps(R3, R3, _MM_SHUFFLE(3, 3, 3, 3))),
			_mm_mul_ps(_mm_shuffle_ps(R3, R3, _MM_SHUFFLE(2, 1, 2, 1)), _mm_shuffle_ps(R2, R2, _MM_SHUFFLE(3, 3, 3, 3))));

		// s0*c5 - s1*c4 + s2*c3 + s3*c2 - s4*c1 + s5*c0
		__m128 P = _mm_mul_ps(S0123, _mm_shuffle_ps(C45, C0123, _MM_SHUFFLE(2, 3, 0, 1)));
		__m128 Q = _mm_mul_ps(S45, _mm_shuffle_ps(C0123, C0123, _MM_SHUFFLE(0, 1, 0, 1)));
		P = _mm_mul_ps(P, _mm_setr_ps(1.0f, -1.0f, 1.0f, 1.0f));
		Q = _mm_mul_ps(Q, _mm_setr_ps(-1.0f, 1.0f, 0.0f, 0.0f));
		__m128 T = _mm_add_ps(P, Q);
		T = _mm_add_ps(T, _mm_movehl_ps(T, T));
		T = _mm_add_ss(T, _mm_shuffle_ps(T, T, _MM_SHUFFLE(1, 1, 1, 1)));
		return _mm_cvtss_f32(T);
	}



	inline static FMatrix MakeScale(const FVector& S)
	{
		FMatrix R = GetIdentity();
		R.M[0][0] = S.X;
		R.M[1][1] = S.Y;
		R.M[2][2] = S.Z;
		return R;
	}

	inline static FMatrix MakeTranslation(const FVector& T)
	{
		FMatrix R = GetIdentity();
		R.M[3][0] = T.X;
		R.M[3][1] = T.Y;
		R.M[3][2] = T.Z;
		return R;
	}

	inline static FMatrix MakeRotationX(float Rad)
	{
		const float c = cosf(Rad), s = sinf(Rad);
		FMatrix R = GetIdentity();
		R.M[1][1] = c;  R.M[1][2] = -s;
		R.M[2][1] = s;  R.M[2][2] = c;
		return R;
	}

	inline static FMatrix MakeRotationY(float Rad)
	{
		const float c = cosf(Rad), s = sinf(Rad);
		FMatrix R = GetIdentity();
		R.M[0][0] = c;  R.M[0][2] = s;
		R.M[2][0] = -s;  R.M[2][2] = c;
		return R;
	}

	inline static FMatrix MakeRotationZ(float Rad)
	{
		const float c = cosf(Rad), s = sinf(Rad);
		FMatrix R = GetIdentity();
		R.M[0][0] = c;  R.M[0][1] = s;
		R.M[1][0] = -s;  R.M[1][1] = c;
		return R;
	}

	
	inline FMatrix operator*(const FMatrix& Other) const
	{
		const __m128 B0 = _mm_loadu_ps(Other.M[0]);
		const __m128 B1 = _mm_loadu_ps(Other.M[1]);
		const __m128 B2 = _mm_loadu_ps(Other.M[2]);
		const __m128 B3 = _mm_loadu_ps(Other.M[3]);

		FMatrix R;
		for (int i = 0; i < 4; ++i)
		{
			const __m128 A = _mm_loadu_ps(M[i]);
			__m128 Row = _mm_mul_ps(_mm_shuffle_ps(A, A, _MM_SHUFFLE(0, 0, 0, 0)), B0);
			Row = _mm_add_ps(Row, _mm_mul_ps(_mm_shuffle_ps(A, A, _MM_SHUFFLE(1, 1, 1, 1)), B1));
			Row = _mm_add_ps(Row, _mm_mul_ps(_mm_shuffle_ps(A, A, _MM_SHUFFLE(2, 2, 2, 2)), B2));
			Row = _mm_add_ps(Row, _mm_mul_ps(_mm_shuffle_ps(A, A, _MM_SHUFFLE(3, 3, 3, 3)), B3));
			_mm_storeu_ps(R.M[i], Row);
		}
		return R;
	}

	inline FMatrix operator*(float Scalar) const
	{
		const __m128 S = _mm_set1_ps(Scalar);

		FMatrix Result;
		for (int i = 0; i < 4; ++i)
		{
			_mm_storeu_ps(Result.M[i], _mm_mul_ps(_mm_loadu_ps(M[i]), S));
		}
		return Result;
	}

	FMatrix& operator*=(const FMatrix& Other)
	{
		*this = *this * Other;
		return *this;
	}

	[[nodiscard]]
	static FMatrix MakeRotation(const FVector& Deg);

	// Yaw-Pitch-Roll
	[[nodiscard]]
	static FMatrix MakeRotationXYZ(const FVector& Deg);

	// Roll-Pitch-Yaw
	[[nodiscard]]
	static FMatrix MakeRotationZYX(const FVector& Deg);

	[[nodiscard]]
	FVector TransformPointRow(const FVector& p, float w = 1.0f) const;
};

inline const FMatrix FMatrix::Identity = FMatrix{
	FVector(1.0f, 0.0f, 0.0f),
	FVector(0.0f, 1.0f, 0.0f),
	FVector(0.0f, 0.0f, 1.0f),
	FVector(0.0f, 0.0f, 0.0f)
};

inline FMatrix::FMatrix(const FVector& InX, const FVector& InY, const FVector& InZ, const FVector& InW)
{
	M[0][0] = InX.X; M[0][1] = InX.Y; M[0][2] = InX.Z; M[0][3] = 0.0f;
	M[1][0] = InY.X; M[1][1] = InY.Y; M[1][2] = InY.Z; M[1][3] = 0.0f;
	M[2][0] = InZ.X; M[2][1] = InZ.Y; M[2][2] = InZ.Z; M[2][3] = 0.0f;
	M[3][0] = InW.X; M[3][1] = InW.Y; M[3][2] = InW.Z; M[3][3] = 1.0f;
}

inline FMatrix::FMatrix(float N)
{
	for (auto& i : M)
	{
		for (float& j : i)
		{
			j = N;
		}
	}
}

inline FMatrix FMatrix::MakeRotation(const FVector& Deg)
{
	return MakeRotationXYZ(Deg);
}

inline FMatrix FMatrix::MakeRotationXYZ(const FVector& Deg)
{
	constexpr float DegToRad = std::numbers::pi_v<float> / 180.0f;
	return MakeRotationX(Deg.X * DegToRad)
		* MakeRotationY(Deg.Y * DegToRad)
		* MakeRotationZ(Deg.Z * DegToRad);
}

inline FMatrix FMatrix::MakeRotationZYX(const FVector& Deg)
{
	constexpr float DegToRad = std::numbers::pi_v<float> / 180.0f;
	return MakeRotationZ(Deg.Z * DegToRad)
		* MakeRotationY(Deg.Y * DegToRad)
		* MakeRotationX(Deg.X * DegToRad);
}


inline FVector FMatrix::TransformPointRow (const FVector& p, float w) const
{
	// [p.X p.Y p.Z w] * M 을 행 단위로 계산
	__m128 R = _mm_mul_ps(_mm_set1_ps(p.X), _mm_loadu_ps(M[0]));
	R = _mm_add_ps(R, _mm_mul_ps(_mm_set1_ps(p.Y), _mm_loadu_ps(M[1])));
	R = _mm_add_ps(R, _mm_mul_ps(_mm_set1_ps(p.Z), _mm_loadu_ps(M[2])));
	R = _mm_add_ps(R, _mm_mul_ps(_mm_set1_ps(w), _mm_loadu_ps(M[3])));

	// FVector는 12바이트라 직접 저장하지 않고 배열을 거친다.
	alignas(16) float Out[4];
	_mm_store_ps(Out, R);

	float x = Out[0], y = Out[1], z = Out[2];
	const float ww = Out[3];
	if (ww != 0.0f && ww != 1.0f) { x /= ww; y /= ww; z /= ww; }
	return FVector(x, y, z);
}
