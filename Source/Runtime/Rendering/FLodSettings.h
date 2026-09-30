#pragma once 

struct FLodSelectSettings
{
	float AllowedErrorPixels = 1.0f;
	float Hysteresis = 0.1f;
	int32 ForceLod = -1;
	bool bDebugTintByLod = false;
};

struct FLodFrameParams
{
	float ProjScale = 1.f;
	bool bIsOrthogonal = false;
};