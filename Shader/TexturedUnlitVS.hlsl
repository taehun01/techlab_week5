#include "Constants.hlsli"

// Textured 파이프라인 전용 경량 VS.
// TexturedPS의 조명 코드가 꺼져 있어 픽셀 셰이더가 UV만 쓰므로,
// 노멀/탄젠트/바이탄젠트 변환과 출력을 생략해 정점당 비용과 보간 데이터를 줄인다.
// 조명을 다시 켜면 Textured 파이프라인을 ExampleVS + TexturedPS로 되돌린다.
struct VS_INPUT
{
    float3 Position : POSITION;
    float2 UV : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
};

PS_INPUT MainVS(VS_INPUT Input)
{
    PS_INPUT Output;
    Output.Position = mul(float4(Input.Position, 1.0f), MVP);
    Output.UV = Input.UV * UVScale + UVOffset;
    return Output;
}
