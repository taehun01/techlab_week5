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
	// UUID는 생성 시 FUObjectArray가 발급한 값을 유지한다.
	// 파일 값으로 덮어쓰면 살아 있는 다른 객체와 UUID가 겹칠 수 있다.
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
