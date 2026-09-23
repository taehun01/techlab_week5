#pragma once
struct FContentDragPayload
{
    enum class EKind : int
    {
        Unknown = 0,
        Texture,
        Mesh,
        Material,
    };

    // 전체 경로 (UTF-8)
    char Path[260] = {};
    // 확장자를 뗀 소문자 이름. 리소스 라이브러리 키와 같은 규칙이다.
    char Key[128] = {};
    EKind Kind = EKind::Unknown;
};


inline constexpr const char* ContentDragPayloadType = "ENGINE_CONTENT";