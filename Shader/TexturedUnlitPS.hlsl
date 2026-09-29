#include "Constants.hlsli"

// TexturedUnlitVS와 짝을 이루는 PS. TexturedPS에서 실제로 동작하는 부분(샘플링, 알파 클립, 틴트)만 남겼다.
Texture2D DiffuseTexture : register(t0);
SamplerState DiffuseSampler : register(s0);

struct PS_INPUT
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
};

float4 MainPS(PS_INPUT Input) : SV_Target
{
    float4 Sampled = DiffuseTexture.Sample(DiffuseSampler, Input.UV);
    clip(Sampled.a - 0.1f);

    // 하이라이트 색상 보간
    float3 Tint = lerp(float3(1.0f, 1.0f, 1.0f), ColorOverride, ColorOverrideAmount);
    float3 BaseColor = Sampled.rgb * MaterialDiffuse.rgb * Tint;

    return float4(BaseColor, Sampled.a);
}
