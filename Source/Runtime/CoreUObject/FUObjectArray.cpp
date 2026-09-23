#include "FUObjectArray.h"

#include <algorithm>
#include <cassert>

void FUObjectArray::SetNextUUID(uint32 UUID)
{
	NextUUID = UUID;
}

const TSet<UObject*>* FUObjectArray::GetBucket(UClass* ClassType) const
{
	auto It = ClassToObjects.find(ClassType);
	return (It != ClassToObjects.end()) ? &It->second : nullptr;
}

void FUObjectArray::AddObject(UObject* Object)
{
	if (!FreeIndices.empty())
	{
		uint32 Index = FreeIndices.back();
		FreeIndices.pop_back();

		Object->InternalIndex = Index;
		Object->UUID = AcquireUUID();
		Objects.at(Index) = Object;
	}
	else
	{
		Object->InternalIndex = static_cast<uint32>(Objects.size());
		Object->UUID = AcquireUUID();
		Objects.push_back(Object);
	}

	ClassToObjects[Object->GetClass()].insert(Object);
}

void FUObjectArray::RemoveObject(UObject* Object)
{
	const uint32 Index = Object->InternalIndex;
	assert(Index < Objects.size() && Objects[Index] == Object);

	FreeIndices.push_back(Index);
	Objects.at(Index) = nullptr;

	auto It = ClassToObjects.find(Object->GetClass());
	if (It != ClassToObjects.end())
	{
		It->second.erase(Object);
	}

}

void FUObjectArray::DestroyObject(UObject* Object) {
	if (Object == nullptr) return;

	Object->Release();
	RemoveObject(Object);
	delete Object; // 오버라이드해서 통계 구현 필요
}

//bool FUObjectArray::IsValid(const UObject* Object, uint32 UUID) const
//{
//	if (Object == nullptr || UUID == 0) return false;
//
//	const auto It = std::find(Objects.begin(), Objects.end(), Object);
//	return It != Objects.end() && (*It)->UUID == UUID;
//}
