#pragma once

#include "FMesh.h"
#include "FMaterial.h"
#include "ShaderConstants.h"
#include "Runtime/Core/TArray.h"
#include "Runtime/Core/PointerTypes.h"
#include <Runtime\Core\IntTypes.h>

// 렌더링에 필요한 드로우 정보
struct FRenderData
{
    FName MeshId{"None"};
    FName MaterialId{"None"};
    FName TextureId{ "None" };
    FObjectConstants Constants;
    int32 startidx = 0;
    int32 indicesCount = -1;
    bool bSelected = false;
    TArray<FInstanceData> Instances;
};

// 한 프레임의 드로우 요청을 수집하는 큐
class FRenderQueue
{
public:
    // 패스별 아이템 추가
    void PushOpaque(const FRenderData& Data) { OpaqueRenderQ.push_back(Data); }
    void PushTranslucent(const FRenderData& Data) { TranslucentRenderQ.push_back(Data); }
    void PushText(const FRenderData& Data) { TextRenderQ.push_back(Data); }
    void PushInstancing(const FRenderData& Data) { InstancingRenderQ.push_back(Data); }

    // 수집된 아이템 조회
    const TArray<FRenderData>& GetOpaqueRenderQ() const { return OpaqueRenderQ; }
    const TArray<FRenderData>& GetTranslucentRenderQ() const { return TranslucentRenderQ; }
    const TArray<FRenderData>& GetTextRenderQ() const { return TextRenderQ; }
    const TArray<FRenderData>& GetInstancingRenderQ() const { return InstancingRenderQ; }

    // 프레임 끝에 호출
    void Clear() { 
        OpaqueRenderQ.clear();
        TranslucentRenderQ.clear();
        TextRenderQ.clear();
        InstancingRenderQ.clear();
    }

    bool IsOpaqueRQEmpty() const { return OpaqueRenderQ.empty(); }
    bool IsTranslucentRQEmpty() const { return TranslucentRenderQ.empty(); }
    bool IsTextRQEmpty() const { return TextRenderQ.empty(); }
    bool IsInstancingRQEmpty() const { return InstancingRenderQ.empty(); }

private:
    TArray<FRenderData> OpaqueRenderQ;
    TArray<FRenderData> TranslucentRenderQ;
    TArray<FRenderData> TextRenderQ;
    TArray<FRenderData> InstancingRenderQ;
};
