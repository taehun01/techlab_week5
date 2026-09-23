#include "FObjectIterator.h"

void FObjectIterator::AdvanceToNextValidObject() {
    while (true)
    {
        FUObjectArray& ObjectArray = FUObjectArray::Get();
        if (CurrentIndex >= ObjectArray.GetMaxIndex())
            return;
        UObject* Object = ObjectArray.GetObjectByIndex(CurrentIndex);
        if (Object && (bIncludeDerived ? Object->IsA(TargetClass) : Object->GetClass() == TargetClass))
            return;
        ++CurrentIndex;
    }
}

FObjectIterator::FObjectIterator(UClass* Class, bool bInIncludeDerivedClasses) : 
    CurrentIndex(0), TargetClass(Class), bIncludeDerived(bInIncludeDerivedClasses)
{
    assert(UClass::AreTypeBitsetsResolved() && "UClass::ResolveTypeBitsets() not call");
    AdvanceToNextValidObject();
}

FObjectIterator& FObjectIterator::operator++()
{
    ++CurrentIndex;
    AdvanceToNextValidObject();
    return *this;
}

UObject* FObjectIterator::operator*() const
{
    return FUObjectArray::Get().GetObjectByIndex(CurrentIndex);
}

bool FObjectIterator::operator==(const FObjectIterator& Other) const
{
    return (this->CurrentIndex == Other.CurrentIndex &&
        this->TargetClass == Other.TargetClass);
}

bool FObjectIterator::operator!=(const FObjectIterator& Other) const
{
    return (!(*this == Other));
}

UObject* FObjectIterator::operator->() const
{
    return FUObjectArray::Get().GetObjectByIndex(CurrentIndex);
}

FObjectIterator::operator bool() const
{
    return (CurrentIndex < FUObjectArray::Get().GetMaxIndex());
}