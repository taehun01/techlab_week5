#pragma once

#include "UObject.h"
#include "Runtime/Core/TArray.h"
#include "Runtime/Core/IntTypes.h"
#include "Runtime/Core/TSet.h"
#include <utility>

class UObject;

class FUObjectArray final
{
	friend class UObject;

public:


	static FUObjectArray& Get() {
		static FUObjectArray Instance;
		return Instance;
	}

	void SetNextUUID(uint32 UUID);
	[[nodiscard]] uint32 GetNextUUID() const { return NextUUID; }
	[[nodiscard]] uint32 GetMaxIndex() const { return static_cast<uint32>(Objects.size()); }
	[[nodiscard]] uint32 GetNumObjects() const { return static_cast<uint32>(Objects.size() - FreeIndices.size()); }
	[[nodiscard]] UObject* GetObjectByIndex(uint32 Index) const { return (Index < Objects.size() ? Objects[Index] : nullptr); } //free된 인덱스거나 범위를 벗어나면 nullptr을 반환한다.
	[[nodiscard]] const TSet<UObject*>* GetBucket(UClass* ClassType) const;

	FUObjectArray(const FUObjectArray&) = delete;
	FUObjectArray& operator=(const FUObjectArray&) = delete;

	FUObjectArray(FUObjectArray&&) = delete;
	FUObjectArray& operator=(FUObjectArray&&) = delete;

private:
	FUObjectArray() = default;
	~FUObjectArray() = default;

	void AddObject(UObject* Object);
	void RemoveObject(UObject* Object);

	[[nodiscard]] uint32 AcquireUUID() { return NextUUID++; }

	TMap<UClass*, TSet<UObject*>> ClassToObjects;
	TArray<UObject*>	Objects;
	TArray<uint32>		FreeIndices;
	uint32 NextUUID = 1u;

	template <typename TObject, typename ... TArgs>
		requires std::derived_from<TObject, UObject>
	friend TObject* NewObject(TArgs&&... Args);
	friend UObject* NewObject(UClass* ClassType);
	friend void DestroyObject(UObject* Object);

	friend class FGarbageCollector;
	void DestroyObject(UObject* Object);
};
