#include "Constants.hlsli"

Texture2D DiffuseTexture : register(t0);
Texture2D NormalTexture : register(t1);
Texture2D SpecularTexture : register(t2);
SamplerState DiffuseSampler : register(s0);

struct PS_INPUT
{
    float4 Position : SV_Position;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float3 Bitangent : BINORMAL;
};

float4 MainPS(PS_INPUT Input) : SV_Target
{
    float4 Sampled = DiffuseTexture.Sample(DiffuseSampler, Input.UV);
    clip(Sampled.a - 0.1f);
    
    // 하이라이트 색상 보간
    float3 Tint = lerp(float3(1.0f, 1.0f, 1.0f), ColorOverride, ColorOverrideAmount);
    float3 BaseColor = Sampled.rgb * MaterialDiffuse.rgb * Tint;

    // 탄젠트 공간 기저 정규화
    float3 N = normalize(Input.Normal);
    float3 T = normalize(Input.Tangent);
    float3 B = normalize(Input.Bitangent);
    float3x3 TBN = float3x3(T, B, N);

    // 노멀맵 샘플링 및 요철 보정
    float4 NormalSample = NormalTexture.Sample(DiffuseSampler, Input.UV);
    float3 MapNormal = NormalSample.rgb * 2.0f - 1.0f;
    float3 PerturbedN = normalize(mul(MapNormal, TBN));
    float HasNormal = step(0.01f, dot(NormalSample.rgb, NormalSample.rgb));
    N = normalize(lerp(N, PerturbedN, HasNormal));

    // 디퓨즈 및 앰비언트 조명 계산
    float3 L = -normalize(LightDirection);
    float NdotL = max(0.0f, dot(N, L));
    float3 Diffuse = LightColor * (Intensity * NdotL);
    float3 Ambient = LightColor * max(AmbientIntensity, 0.4f);
    float3 DirectionalLight = max(Ambient + Diffuse, 0.5f);

    // 스펙큘러맵 샘플링 및 하이라이트 계산
    float3 V = float3(0.0f, 0.0f, 1.0f);
    float3 H = normalize(L + V);
    float NdotH = max(0.0f, dot(N, H));
    float SpecularFactor = pow(NdotH, 16.0f);
    float4 SpecularSample = SpecularTexture.Sample(DiffuseSampler, Input.UV);
    float3 SpecularColor = SpecularSample.rgb * LightColor * (SpecularFactor * Intensity * 2.0f);

    // 음영 비활성화 무분기 보간
    float3 FinalLit = BaseColor * DirectionalLight + SpecularColor;
    float3 FinalColor = lerp(FinalLit, BaseColor, step(0.5f, DisableShading));

    return float4(FinalColor, Sampled.a);
}

