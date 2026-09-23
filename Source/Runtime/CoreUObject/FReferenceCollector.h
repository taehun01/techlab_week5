#pragma once

#include "Runtime/Core/TArray.h"
#include <unordered_set>

class UObject;

template <typename T>
class TWeakObjectPtr;

class FReferenceCollector final {
public:
	// 기본 참조 수집 함수
	void AddReferencedObject(UObject* Object);
	void ProcessReferences();

	[[nodiscard]]
	bool bIsReferenced(const UObject* Object) const;

	// 단일 객체 포인터 등록
	template <typename T>
	void Add(T* Object)
	{
		AddReferencedObject(Object);
	}

	// 배열 내의 객체 포인터들 일괄 등록
	template <typename T>
	void Add(const TArray<T*>& ObjectArray)
	{
		for (T* Object : ObjectArray)
		{
			AddReferencedObject(Object);
		}
	}

	// 약한 참조 포인터는 수집 대상이 아니므로 무시
	template <typename T>
	void Add(const TWeakObjectPtr<T>&)
	{
	}

	// 매크로 인자가 없을 때 호출되는 기본 함수
	void AddAll()
	{
	}

	// 여러 변수들을 순차적으로 등록하는 가변 인자 함수
	template <typename... TArgs>
	void AddAll(const TArgs&... Args)
	{
		(Add(Args), ...);
	}

private:
	std::unordered_set<const UObject*> ReferencedObjects;
	TArray<UObject*> PendingObjects;
};