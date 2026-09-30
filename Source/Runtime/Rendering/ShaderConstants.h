#pragma once

#include "Runtime/Math/FMatrix.h"
#include "Runtime/Math/FVector.h"
#include "Runtime/Math/FVector2.h"
#include "Runtime/Math/FVector4.h"


// b0에 바인딩. 카메라에 의존하는 값은 두지 않는다.
// MVP는 셰이더가 World와 b1의 ViewProj(FFrameConstants)로 계산한다.
struct FObjectConstants {
  FVector ColorOverride{0.0f, 0.0f, 0.0f};
  float ColorOverrideAmount = 0.0f;
  FVector2 UVScale{1.0f, 1.0f};
  FVector2 UVOffset{0.0f, 0.0f};
  FMatrix World = FMatrix::GetIdentity();
  float DisableShading = 0.0f;
  FVector Padding;
  // MaterialDiffuse는 머티리얼 상수 버퍼(b3, FMaterialConstants)로 옮겼다
};
static_assert(sizeof(FObjectConstants) % 16 == 0);

// b3에 바인딩. 머티리얼마다 하나씩 두고 머티리얼을 바인딩할 때 같이 바인딩한다.
struct FMaterialConstants {
  FVector4 MaterialDiffuse{ 1.f, 1.f, 1.f, 1.f };
};
static_assert(sizeof(FMaterialConstants) % 16 == 0);

// b0에 바인딩
struct FGridConstants {
  FMatrix MVP;
  FMatrix World;
  float CellSize;
  FVector Padding;
};

static_assert(sizeof(FGridConstants) % 16 == 0);

// LINE_LIST 기반 에디터 그리드용 상수 버퍼 (b0).
struct FGridLineConstants {
  FMatrix MVP;
  FVector CameraPosition;
  float FadeStartDistance;
  float FadeEndDistance;
  FVector Padding;
};

static_assert(sizeof(FGridLineConstants) % 16 == 0);

// b1에 바인딩 (뷰 단위)
struct FFrameConstants {
  FVector2 ViewportSize;
  // 뷰 모드(Unlit)에 따른 셰이딩 끄기 (FRenderer::SetRenderMode). b0의 DisableShading과 OR로 쓴다.
  float ViewDisableShading = 0.0f;
  float Padding = 0.0f;
  // D3D 클립 변환까지 곱한 ViewProj (FRenderer::SetViewProjection)
  FMatrix ViewProj = FMatrix::GetIdentity();
};
static_assert(sizeof(FFrameConstants) % 16 == 0);


struct FLightConstants {
  // 기본 조명 파라미터
  FVector LightDirection{-0.5f, -0.5f, -1.0f};
  float Intensity = 1.0f;

  FVector LightColor{1.0f, 1.0f, 1.0f};
  float AmbientIntensity = 0.2f;
};

static_assert(sizeof(FLightConstants) % 16 == 0);
