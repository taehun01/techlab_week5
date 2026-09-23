#pragma once

#include <cassert>

#include "Runtime/Core/IntTypes.h"
#include "Runtime/CoreUObject/FUObjectArray.h"
#include "Runtime/CoreUObject/UObject.h"
#include "Runtime/CoreUObject/UClass.h"

class FObjectIterator // FUObjectArray 전체를 선형탐색
{
private:
    uint32  CurrentIndex;
    UClass* TargetClass;
    bool    bIncludeDerived;


    void AdvanceToNextValidObject();

public:

    explicit FObjectIterator(UClass* Class = UObject::StaticClass(), bool bInIncludeDerivedClasses = true);

    FObjectIterator& operator++();

    UObject* operator*() const;
    bool operator==(const FObjectIterator& ref) const;

    bool operator!=(const FObjectIterator& ref) const;

    UObject* operator->() const;

    explicit operator bool() const;
};
