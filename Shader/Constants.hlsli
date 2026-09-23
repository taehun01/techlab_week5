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
