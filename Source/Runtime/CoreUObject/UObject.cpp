#include "UObject.h"
#include "Runtime/Engine/FArchive.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "FUObjectArray.h"

IMPLEMENT_ROOT_UCLASS(UObject)
UCLASS_META(UObject, DisplayName, "Object")

void UObject::Initialize()
{
}

void UObject::Release()
{
}

void UObject::Destroy()
{
	FUObjectArray::Get().DestroyObject(this);
}

void UObject::Serialize(FArchive& Archive) const
{
	Archive.SetInt32("UUID", UUID);
	Archive.SetString("Type", GetClass()->GetUClassName());
}

void UObject::Deserialize(const FArchive& Archive)
{
	UUID = Archive.GetInt32("UUID");
}

void UObject::AddReferencedObjects(FReferenceCollector& Collector)
{
}

void* UObject::operator new(std::size_t Size)
{
	void* Memory = ::operator new(Size);
	TotalAllocationBytes += Size;
	++TotalAllocationCount;

	return Memory;
}

void UObject::operator delete(void* Memory, std::size_t Size) noexcept
{
	if (Memory == nullptr) return;

	TotalAllocationBytes -= Size;
	--TotalAllocationCount;

	::operator delete(Memory);
}

void* UObject::operator new(std::size_t Size, std::align_val_t Alignment)
{
	void* Memory = ::operator new(Size, Alignment);

	TotalAllocationBytes += Size;
	++TotalAllocationCount;

	return Memory;
}

void UObject::operator delete(void* Memory, std::size_t Size, std::align_val_t Alignment) noexcept
{
	if (Memory == nullptr) return;

	TotalAllocationBytes -= Size;
	--TotalAllocationCount;

	::operator delete(Memory, Alignment);
}
