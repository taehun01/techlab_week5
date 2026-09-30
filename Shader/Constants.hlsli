cbuffer ObjectConstants : register(b0)
{
    row_major float4x4 MVP;
    float3 ColorOverride;
    float ColorOverrideAmount;
    float2 UVScale;
    float2 UVOffset;
    row_major float4x4 World;
    float DisableShading;
    float3 ObjectPadding;
}

// 머티리얼 단위 상수. 오브젝트 상수(b0)는 컴포넌트 단위로 공유되므로
// 섹션마다 다른 머티리얼 값은 여기(FMaterial::BindResources에서 바인딩)로 분리한다.
cbuffer MaterialConstants : register(b3)
{
    float4 MaterialDiffuse;
}

cbuffer FrameConstants : register(b1)
{
    float2 ViewportSize;
    float2 Padding;
}


cbuffer LightConstants : register(b2)
{
    float3 LightDirection;
    float Intensity;
    float3 LightColor;
    float AmbientIntensity;
};
