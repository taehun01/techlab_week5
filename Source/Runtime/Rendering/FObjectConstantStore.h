#pragma once

#include "ShaderConstants.h"
#include "Runtime/Core/IntTypes.h"
#include "Runtime/Core/TArray.h"

#include <cstring>

// 컴포넌트별 영구 오브젝트 상수의 CPU 사본.
// 컴포넌트는 씬에 등록될 때 슬롯을 하나 받아 해제될 때까지 쓰고,
// GPU 쪽 영구 상수 버퍼(FRenderer::UploadPersistentObjectConstants)는 같은 슬롯 배치를 그대로 따른다.
//
// 불변식: dirty가 아닌 슬롯은 GPU 버퍼 내용과 CPU 사본이 같다.
// 그래서 매 프레임 새로 만든 상수를 사본과 비교해 달라진 슬롯만 올리면 된다
// (트랜스폼·색·선택 등 값을 바꾸는 곳마다 dirty를 걸 필요가 없다).
class FObjectConstantStore
{
public:
    static constexpr uint32 InvalidSlot = ~0u;

    // 슬롯 하나 = 256바이트 (VSSetConstantBuffers1 오프셋 단위). 연속 구간을 한 번에 올릴 수 있게 GPU와 같은 간격으로 둔다.
    struct FSlot
    {
        FObjectConstants Constants;
        uint8 Padding[256 - sizeof(FObjectConstants)];
    };
    static_assert(sizeof(FSlot) == 256);

    static FObjectConstantStore& Get()
    {
        static FObjectConstantStore Instance;
        return Instance;
    }

    // 메인 스레드에서만 호출한다 (병렬 수집 중에는 배열 크기가 바뀌면 안 된다).
    uint32 Allocate()
    {
        uint32 Slot;
        if (!FreeSlots.empty())
        {
            Slot = FreeSlots.back();
            FreeSlots.pop_back();
        }
        else
        {
            Slot = static_cast<uint32>(Slots.size());
            Slots.emplace_back();
            DirtyFlags.push_back(0u);
        }
        // 새로 받은 슬롯의 GPU 내용은 CPU 사본과 같다는 보장이 없으므로 한 번은 반드시 올린다
        DirtyFlags[Slot] = 1u;
        return Slot;
    }

    void Free(uint32 Slot)
    {
        if (Slot < Slots.size())
        {
            FreeSlots.push_back(Slot);
        }
    }

    // 병렬 수집 단계에서 호출한다. 슬롯마다 한 스레드만 쓰므로 잠금이 필요 없다.
    void Update(uint32 Slot, const FObjectConstants& Constants)
    {
        FObjectConstants& Stored = Slots[Slot].Constants;
        if (std::memcmp(&Stored, &Constants, sizeof(FObjectConstants)) != 0)
        {
            Stored = Constants;
            DirtyFlags[Slot] = 1u;
        }
    }

    [[nodiscard]] const FObjectConstants& GetConstants(uint32 Slot) const { return Slots[Slot].Constants; }
    [[nodiscard]] uint32 GetSlotCount() const { return static_cast<uint32>(Slots.size()); }
    [[nodiscard]] const FSlot* GetSlotData() const { return Slots.data(); }
    [[nodiscard]] uint8* GetDirtyFlags() { return DirtyFlags.data(); }

    // GPU 버퍼를 새로 만들었을 때: 전부 다시 올린다
    void MarkAllDirty()
    {
        std::memset(DirtyFlags.data(), 1, DirtyFlags.size());
    }

private:
    FObjectConstantStore() = default;

    TArray<FSlot> Slots;
    TArray<uint8> DirtyFlags;
    TArray<uint32> FreeSlots;
};
