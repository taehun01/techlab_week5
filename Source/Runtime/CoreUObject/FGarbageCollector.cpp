#include "FGarbageCollector.h"

#include "FReferenceCollector.h"
#include "FUObjectArray.h"

#include <algorithm>

void FGarbageCollector::AddRoot(UObject* Object)
{
	if (Object == nullptr) return;

	if (std::find(RootObjects.begin(), RootObjects.end(), Object) == RootObjects.end())
		RootObjects.push_back(Object);
}

void FGarbageCollector::RemoveRoot(UObject* Object)
{
	std::erase(RootObjects, Object);
}

void FGarbageCollector::CollectGarbage()
{
	FReferenceCollector Collector;

	for (UObject* Root : RootObjects)
		Collector.AddReferencedObject(Root);

	// 마킹 단계
	Collector.ProcessReferences();

	// 스위프 단계
	FUObjectArray& ObjectArray = FUObjectArray::Get();

	uint32 MaxIndex = ObjectArray.GetMaxIndex();

	for (uint32 Index = 0; Index < MaxIndex; Index++)
	{
		UObject* Object = ObjectArray.GetObjectByIndex(Index);
		if (Object == nullptr)
			continue;

		if (!Collector.bIsReferenced(Object))
			ObjectArray.DestroyObject(Object);
	}
}
