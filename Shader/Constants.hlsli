// 오브젝트 상수. 카메라에 의존하는 값은 두지 않는다 (ViewProj는 b1).
// 화면 위치 = mul(mul(pos, World), ViewProj)
cbuffer ObjectConstants : register(b0)
{
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

// 뷰(카메라) 단위 상수. ViewProj에는 D3D 클립 변환까지 곱해 둔다 (FRenderer::SetViewProjection).
cbuffer FrameConstants : register(b1)
{
    float2 ViewportSize;
    // 뷰 모드(Unlit)에 따른 셰이딩 끄기. 오브젝트 상수(b0)의 DisableShading과 OR로 쓴다.
    // 뷰마다 다른 값이라 오브젝트 상수에 두면 영구 슬롯이 뷰마다 바뀌므로 여기에 둔다.
    float ViewDisableShading;
    float FramePadding;
    row_major float4x4 ViewProj;
}


cbuffer LightConstants : register(b2)
{
    float3 LightDirection;
    float Intensity;
    float3 LightColor;
    float AmbientIntensity;
};
