#include "Constants.hlsli"

struct VS_INPUT
{
    float3 Position : POSITION;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
    float3 Normal : NORMAL;
};

struct PS_INPUT
{
    float4 Position : SV_Position;
    nointerpolation float PivotDepth : TEXCOORD0;
};

static const float RingWidthPixels = 3.0f;
static const float RingRadius = 1.0f;

PS_INPUT main(VS_INPUT Input)
{
    PS_INPUT Output;

    const float Side = Input.Color.r;
    float2 Radial = normalize(Input.Position.yz);

    float3 CenterLocal = float3(0.0f, Radial) * RingRadius;
    float4 CenterClip = mul(float4(CenterLocal, 1.0f), MVP);

    float3 Tangent = float3(0.0f, -Radial.y, Radial.x);
    float4 NeighborClip = mul(float4(CenterLocal + Tangent * 0.01f, 1.0f), MVP);

    float2 CenterNDC = CenterClip.xy / CenterClip.w;
    float2 TangentNDC = NeighborClip.xy / NeighborClip.w - CenterNDC;
    float2 TangentPixels = TangentNDC * ViewportSize;

    float2 Normal = normalize(float2(-TangentPixels.y, TangentPixels.x));

    float2 Offset = Normal * Side * RingWidthPixels / ViewportSize;

    Output.Position = CenterClip;
    Output.Position.xy += Offset * CenterClip.w;

    float4 PivotClip = mul(float4(0.0f, 0.0f, 0.0f, 1.0f), MVP);
    Output.PivotDepth = PivotClip.z / PivotClip.w;

    return Output;
}