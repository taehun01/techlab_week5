#include "FName.h"
#include "Runtime/Core/FNamePool.h"
#include "Runtime/Utility/EngineUtil.h"

// "None"은 매우 자주 만들어지므로 풀 조회는 처음 한 번만 한다.
static const FNameEntry& GetNoneEntry()
{
	static const FNameEntry NoneEntry = FNamePool::AddEntry("None");
	return NoneEntry;
}

FName::FName()
	: Entry{ GetNoneEntry() }
{
}

FName::FName(const char* CharPtr)
	: FName{ FString{ CharPtr } }
{
}

FName::FName(const FString& Str)
	: Entry{ FNamePool::AddEntry(Str) }
{
}

bool FName::IsNone() const
{
	const FNameEntry& None = GetNoneEntry();
	return Entry.ComparisonBucketIndex == None.ComparisonBucketIndex
		&& Entry.ComparisonIndex == None.ComparisonIndex;
}

int32 FName::Compare(const FName& Other) const
{
	if (*this == Other) { return 0; }

	const FString& ThisStr = FNamePool::GetComparisonString(Entry);
	const FString& OtherStr = FNamePool::GetComparisonString(Other.Entry);

	return ThisStr.compare(OtherStr);
}

int32 FName::CompareSensitive(const FName& Other) const
{
	if (
		Entry.DisplayBucketIndex == Other.Entry.DisplayBucketIndex &&
		Entry.DisplayIndex == Other.Entry.DisplayIndex
		)
	{

		return 0;
	}

	const FString& ThisStr = FNamePool::GetDisplayString(Entry);
	const FString& OtherStr = FNamePool::GetDisplayString(Other.Entry);

	return ThisStr.compare(OtherStr);
}

bool FName::operator==(const FName& Other) const
{
	if (
		Entry.ComparisonBucketIndex == Other.Entry.ComparisonBucketIndex &&
		Entry.ComparisonIndex == Other.Entry.ComparisonIndex
		)
	{
		
		return true;
	}
	else
	{
		return false;
	}
}

FString FName::ToString() const
{
	return FNamePool::GetDisplayString(Entry);
}

size_t FName::GetHash() const
{
	const size_t Bucket =
		static_cast<size_t>(Entry.ComparisonBucketIndex);
	const size_t Index =
		static_cast<size_t>(Entry.ComparisonIndex);
	
	return EngineUtil::HashCombine(Bucket, Index);
}
