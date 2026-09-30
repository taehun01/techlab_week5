#pragma once

#include "FMesh.h"
#include "FMaterial.h"
#include "ShaderConstants.h"
#include "Runtime/Core/TArray.h"
#include "Runtime/Core/PointerTypes.h"
#include <Runtime\Core\IntTypes.h>
#include <algorithm>
#include <execution>

// 렌더링에 필요한 드로우 정보
struct FRenderData
{
    FName MeshId;
    FName MaterialId;
    FName TextureId;
    FObjectConstants Constants;
    int32 startidx = 0;
    int32 indicesCount = -1;
    bool bSelected = false;
    TArray<FInstanceData> Instances;

    // 수집 단계에서 한 번 조회한 리소스 포인터 (Draw 시 FName 재조회 방지)
    FStaticMesh* MeshPtr = nullptr;
    FMaterial* MaterialPtr = nullptr;
};

// 불투명·반투명 패스의 드로우 항목. FRenderData(FName·상수·TArray 포함) 대신 큐에 넣는 경량 구조체.
// 컴포넌트 단위 상수는 FRenderQueue의 PrimitiveConstants에 한 번만 두고 PrimitiveIndex로 참조한다.
struct FDrawItem
{
    FStaticMesh* Mesh = nullptr;     // 큐에 들어간 항목은 항상 유효
    FMaterial* Material = nullptr;   // 큐에 들어간 항목은 항상 유효 (못 찾으면 "Simple")
    int32 StartIndex = 0;
    int32 IndexCount = -1;           // -1이면 메시 전체
    // bPersistentConstants=true: FObjectConstantStore의 영구 슬롯 번호
    // false: 이번 프레임 FRenderQueue::PrimitiveConstants 인덱스
    uint32 PrimitiveIndex = 0;
    bool bPersistentConstants = false;
};

// 한 프레임의 드로우 요청을 수집하는 큐
class FRenderQueue
{
public:
    // 컴포넌트 단위 상수를 추가하고 참조를 돌려준다. 다음 AddPrimitive 전까지만 참조가 유효하다.
    FObjectConstants& AddPrimitive(uint32& OutIndex) {
        OutIndex = static_cast<uint32>(PrimitiveConstants.size());
        return PrimitiveConstants.emplace_back();
    }

    // 패스별 아이템 추가
    void PushOpaque(const FDrawItem& Item, uint64 SortKey) {
        OpaqueSortKeys.push_back({ SortKey, static_cast<uint32>(OpaqueItems.size()) });
        OpaqueItems.push_back(Item);
    }
    void PushTranslucent(const FDrawItem& Item, uint64 SortKey) {
        TranslucentSortKeys.push_back({ SortKey, static_cast<uint32>(TranslucentItems.size()) });
        TranslucentItems.push_back(Item);
    }
    void PushText(const FRenderData& Data) { TextRenderQ.push_back(Data); }
    void PushInstancing(const FRenderData& Data) { InstancingRenderQ.push_back(Data); }

    // 수집된 아이템 조회
    const TArray<FObjectConstants>& GetPrimitiveConstants() const { return PrimitiveConstants; }
    const TArray<FDrawItem>& GetOpaqueItems() const { return OpaqueItems; }
    const TArray<FDrawItem>& GetTranslucentItems() const { return TranslucentItems; }
    const TArray<FRenderData>& GetTextRenderQ() const { return TextRenderQ; }
    const TArray<FRenderData>& GetInstancingRenderQ() const { return InstancingRenderQ; }

    const TArray<std::pair<uint64, uint32>>& GetOpaqueSortKeys() const { return OpaqueSortKeys; }
    const TArray<std::pair<uint64, uint32>>& GetTranslucentSortKeys() const { return TranslucentSortKeys; }

    // 프레임 끝에 호출
    void Clear() {
        PrimitiveConstants.clear();
        OpaqueItems.clear();
        TranslucentItems.clear();
        TextRenderQ.clear();
        InstancingRenderQ.clear();

        OpaqueSortKeys.clear();
        TranslucentSortKeys.clear();
    }

    void Sort()
    {
        // 키 5만 개 규모라 병렬 정렬이 이득이다. 작은 배열은 표준 라이브러리가 알아서 순차로 처리한다.
        std::sort(std::execution::par, OpaqueSortKeys.begin(), OpaqueSortKeys.end(), [](const auto& X, const auto& Y) {return X.first < Y.first;});
        std::sort(std::execution::par, TranslucentSortKeys.begin(), TranslucentSortKeys.end(), [](const auto& X, const auto& Y) {return X.first < Y.first;});
    }

    bool IsOpaqueRQEmpty() const { return OpaqueItems.empty(); }
    bool IsTranslucentRQEmpty() const { return TranslucentItems.empty(); }
    bool IsTextRQEmpty() const { return TextRenderQ.empty(); }
    bool IsInstancingRQEmpty() const { return InstancingRenderQ.empty(); }

private:
    TArray<FObjectConstants> PrimitiveConstants;
    TArray<FDrawItem> OpaqueItems;
    TArray<FDrawItem> TranslucentItems;
    TArray<FRenderData> TextRenderQ;
    TArray<FRenderData> InstancingRenderQ;

    TArray<std::pair<uint64, uint32>> OpaqueSortKeys;
    TArray<std::pair<uint64, uint32>> TranslucentSortKeys;
};
