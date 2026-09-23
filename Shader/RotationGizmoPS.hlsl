#include "Constants.hlsli"

struct PS_INPUT
{
    float4 Position : SV_Position;
    nointerpolation float PivotDepth : TEXCOORD0;
};

float4 main(PS_INPUT Input) : SV_TARGET
{
     clip(Input.PivotDepth - Input.Position.z);
	return float4(ColorOverride, 1.0f);
}
