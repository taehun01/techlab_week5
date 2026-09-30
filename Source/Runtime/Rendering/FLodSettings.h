#pragma once 

// LOD 생성 개수
constexpr uint32 MAX_MESH_LOD = 4u;

struct FLodSettings
{
	float Ratios[MAX_MESH_LOD] = { 1.f, 0.5f, 0.25f, 0.05f };
	float TargetError[MAX_MESH_LOD] = { 0.f, 0.01f, 0.05f, 1.0f };
	float SloppyFallbackFactor = 1.5f;
};

struct FLodSelectSettings
{
	float AllowedErrorPixels = 3.0f;
	float Hysteresis = 0.2f;
	//int32 ForceLod = -1;
	//bool bDebugTintByLod = false; // 쓰지 않음
};

struct FLodFrameParams
{
	float ProjScale = 1.f;
	bool bIsOrthogonal = false;
};