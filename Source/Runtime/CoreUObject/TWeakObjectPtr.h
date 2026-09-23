#pragma once

#include "FUObjectArray.h"
#include <cstddef>
#include <concepts>

class UObject;

// 객체 해제 시 자동으로 널포인터를 반환하는 약한 참조 포인터
//template<typename U = T>
//	requires std::derived_from<U, UObject> 이거 여기에 선언하면 다른곳에서 순환오류 날수도있음
template<typename T>
class TWeakObjectPtr
{
private:
	uint32 ObjectIndex = 0;
	uint32 ObjectUUID = 0;

public:
	TWeakObjectPtr() = default;

	template<typename U = T>
		requires std::derived_from<U, UObject>
	TWeakObjectPtr(T* InPtr)
		: ObjectIndex(InPtr ? InPtr->GetInternalIndex() : 0), ObjectUUID(InPtr ? InPtr->GetUUID() : 0)
	{
	}

	TWeakObjectPtr(std::nullptr_t)
		: ObjectIndex(0), ObjectUUID(0)
	{
	}

	template<typename U = T>
		requires std::derived_from<U, UObject>
	TWeakObjectPtr& operator=(T* InPtr)
	{
		ObjectIndex = InPtr ? InPtr->GetInternalIndex() : 0;
		ObjectUUID = InPtr ? InPtr->GetUUID() : 0;
		return *this;
	}

	TWeakObjectPtr& operator=(std::nullptr_t)
	{
		ObjectIndex = 0;
		ObjectUUID = 0;
		return *this;
	}

	T* Get() const
	{
		if (ObjectUUID == 0)
			return (nullptr);

		UObject* Object = FUObjectArray::Get().GetObjectByIndex(ObjectIndex);
		if (Object == nullptr || Object->GetUUID() != ObjectUUID)
			return (nullptr);
		return (static_cast<T*>(Object));
	}

	T* operator->() const { return Get(); }
	operator T*() const { return Get(); }
	explicit operator bool() const { return Get() != nullptr; }

	bool operator==(const TWeakObjectPtr& Other) const { return Get() == Other.Get(); }
	bool operator!=(const TWeakObjectPtr& Other) const { return Get() != Other.Get(); }
	bool operator==(const T* Other) const { return Get() == Other; }
	bool operator!=(const T* Other) const { return Get() != Other; }

	bool IsValid() const { return Get() != nullptr; }
	void Reset() { ObjectIndex = 0; ObjectUUID = 0; }
};
