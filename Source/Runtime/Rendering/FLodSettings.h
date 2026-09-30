#pragma once 

struct FLodSelectSettings
{
	float AllowedErrorPixels = 10.0f;
	float Hysteresis = 0.2f;
	int32 ForceLod = -1;
	bool bDebugTintByLod = false;
};

struct FLodFrameParams
{
	float ProjScale = 1.f;
	bool bIsOrthogonal = false;
};