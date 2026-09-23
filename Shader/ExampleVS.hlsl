#include "Constants.hlsli"

struct VS_INPUT
{
    float3 Position : POSITION;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float3 Bitangent : BINORMAL;
};

struct PS_INPUT
{
    float4 Position : SV_Position;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float3 Bitangent : BINORMAL;
};

PS_INPUT MainVS(VS_INPUT Input)
{
    PS_INPUT Output;

    Output.Position = mul(float4(Input.Position, 1.0f), MVP);
    Output.Color = Input.Color;
    Output.UV = Input.UV * UVScale + UVOffset;

    // 월드 공간 변환
    Output.Normal = mul(float4(Input.Normal, 0.0f), World).xyz;
    Output.Tangent = mul(float4(Input.Tangent, 0.0f), World).xyz;
    Output.Bitangent = mul(float4(Input.Bitangent, 0.0f), World).xyz;

    return Output;
}