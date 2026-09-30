#include "Constants.hlsli"

struct VS_INPUT
{
    // 정점 데이터
    float3 Position : POSITION;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;

    // 인스턴스 데이터
    row_major float4x4 InstanceWorld : INSTANCE_WORLD;
    float4 InstanceColor : INSTANCE_COLOR;
    float2 InstanceUVScale : INSTANCE_UV_SCALE;
    float2 InstanceUVOffset : INSTANCE_UV_OFFSET;
};

struct PS_INPUT
{
    float4 Position : SV_Position;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
};

PS_INPUT MainVS(VS_INPUT Input)
{
    PS_INPUT Output;

    // 인스턴스 월드 변환
    float4 WorldPos = mul(float4(Input.Position, 1.0f), Input.InstanceWorld);

    Output.Position = mul(WorldPos, ViewProj);
    
    // 색상 결합
    Output.Color = Input.Color * Input.InstanceColor;

    //UV 변환
    Output.UV = Input.UV * Input.InstanceUVScale + Input.InstanceUVOffset;

    return Output;
}
