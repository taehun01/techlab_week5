#pragma once

#include "Runtime/Core/TArray.h"
#include "Runtime/Core/TFunction.h"

class UObject;
class FReferenceCollector;

class FGarbageCollector final {
public:
	static FGarbageCollector& Get() {
		static FGarbageCollector Instance;
		return Instance;
	}

	void AddRoot(UObject* Object);
	void RemoveRoot(UObject* Object);

	void CollectGarbage();
	FGarbageCollector(const FGarbageCollector&) = delete;
	FGarbageCollector& operator=(const FGarbageCollector&) = delete;

	FGarbageCollector(FGarbageCollector&&) = delete;
	FGarbageCollector& operator=(FGarbageCollector&&) = delete;

private:
	FGarbageCollector() = default;
	~FGarbageCollector() = default;

	TArray<UObject*> RootObjects;
};