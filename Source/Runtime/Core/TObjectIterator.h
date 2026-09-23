#pragma once

#include <cassert>

#include "Runtime/Core/IntTypes.h"
#include "Runtime/CoreUObject/FUObjectArray.h"
#include "Runtime/CoreUObject/UObject.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/Core/TSet.h"
#include "Runtime/Core/TArray.h"

template<typename TObject>
class TObjectIterator // UClass가 동일한 버킷 기반 탐색
{
private:
    struct FSnapshotEntry
    {
        uint32 UUID;
        uint32 Index;
    };
    TArray<FSnapshotEntry> Snapshot;
    size_t Cursor = 0;
    bool bIncludeDerived;

    void CollectBucket(const TSet<UObject*>* Bucket)
    {
        if (!Bucket) 
        { 
            return; 
        }

        Snapshot.reserve(Snapshot.size() + Bucket->size());
        for (UObject* Object : *Bucket)
        {
            Snapshot.push_back({ Object->GetUUID(), Object->GetInternalIndex() });
        }
    }

    TObject* Resolve() const
    {
        if (Cursor >= Snapshot.size())
            return nullptr;

        const FSnapshotEntry SnapshotEntry = Snapshot.at(Cursor);
        UObject* Object = FUObjectArray::Get().GetObjectByIndex(SnapshotEntry.Index);

        if (!Object || SnapshotEntry.UUID != Object->GetUUID())
            return nullptr;

        return (static_cast<TObject*>(Object));
    }

public:
    TObjectIterator(bool bInIncludeDerivedClasses = true) : bIncludeDerived(bInIncludeDerivedClasses)
    {
        assert(UClass::AreTypeBitsetsResolved() && "UClass::ResolveTypeBitsets() not call");

        UClass* TargetClass = TObject::StaticClass();
        FUObjectArray& ObjectArray = FUObjectArray::Get();

        if (!bIncludeDerived)
        {
            CollectBucket(ObjectArray.GetBucket(TargetClass));
            return;
        }

        // 등록 클래스를 훑으며 파생 클래스 버킷을 합친다.
        // FClassIdSet을 이용해 비트 테스트만으로 판정된다.
        const uint32 ClassCount = UClass::GetRegisteredCount();
        for (uint32 Id = 0; Id < ClassCount; ++Id)
        {
            UClass* ClassType = UClass::GetClassById(Id);
            if (!ClassType || !ClassType->IsChildOrSelfOf(TargetClass))
            {
                continue;
            }
            CollectBucket(ObjectArray.GetBucket(ClassType));
        }

    }

    explicit operator bool() const 
    { 
        return (Cursor < Snapshot.size()); 
    }

    TObject* operator*() const 
    {
        return (Resolve());
    }

    TObject* operator->() const 
    {
        return (Resolve());
    }

    TObjectIterator& operator++() 
    { 
        ++Cursor;
        return (*this); 
    }

    bool operator==(const TObjectIterator& Other) const 
    { 
        return (Resolve() == Other.Resolve());
    }

    bool operator!=(const TObjectIterator& Other) const 
    { 
        return (!(*this == Other));
    }

};