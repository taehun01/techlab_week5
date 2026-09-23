#pragma once

#include <functional>
#include <vector>
#include <cstdint>
#include <algorithm>

// 델리게이트 등록 해제 시 식별자로 사용하는 핸들
struct FDelegateHandle
{
    uint64_t ID = 0;

    bool IsValid() const { return ID != 0; }
    void Reset() { ID = 0; }

    bool operator==(const FDelegateHandle& Other) const { return ID == Other.ID; }
};

template<typename... Args>
class TMulticastDelegate
{
public:
    using FCallback = std::function<void(Args...)>;

    // 람다 또는 일반 함수 등록 (해제용 핸들 반환)
    FDelegateHandle Add(FCallback InFunc)
    {
        if (!InFunc) return FDelegateHandle{};

        uint64_t NewID = ++NextID;
        Entries.push_back({ NewID, std::move(InFunc) });
        return FDelegateHandle{ NewID };
    }

    // 클래스 멤버 함수 등록 편의 함수 (객체 포인터 + 멤버 함수 포인터)
    template<typename UserClass>
    FDelegateHandle AddRaw(UserClass* InUserObject, void(UserClass::* InMethod)(Args...))
    {
        return Add([InUserObject, InMethod](Args... InArgs)
            {
                (InUserObject->*InMethod)(std::forward<Args>(InArgs)...);
            });
    }

    // 특정 핸들러만 구독 해제
    bool Remove(FDelegateHandle& InHandle)
    {
        if (!InHandle.IsValid()) return false;

        auto It = std::remove_if(Entries.begin(), Entries.end(),
            [&InHandle](const FEntry& Entry) { return Entry.ID == InHandle.ID; });

        if (It != Entries.end())
        {
            Entries.erase(It, Entries.end());
            InHandle.Reset();
            return true;
        }
        return false;
    }

    // 모든 리스너 일괄 호출
    void Broadcast(Args... InArgs)
    {
        // 순회 중 콜백 내부에서 Add/Remove가 발생해도 안전하도록 복사본 기반 순회
        auto LocalEntries = Entries;
        for (const auto& Entry : LocalEntries)
        {
            if (Entry.Callback)
            {
                Entry.Callback(InArgs...);
            }
        }
    }

    // 전체 해제
    void Clear()
    {
        Entries.clear();
    }

    bool IsBound() const
    {
        return !Entries.empty();
    }

private:
    struct FEntry
    {
        uint64_t ID;
        FCallback Callback;
    };

    std::vector<FEntry> Entries;
    uint64_t NextID = 0;
};