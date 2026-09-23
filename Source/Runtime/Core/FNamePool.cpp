#include "FNamePool.h"


// 해시 함수
// 어쩌면 그냥 이미 있는 해시 함수 라이브러리를 쓰는게 편하고 더 좋을지도..

namespace
{
	uint64 Hash(FStringView Str)
	{
		constexpr uint64 MOD_NUM = 1'000'000'007;
		constexpr uint64 BASE = 257;

		// 반환될 해시 값
		uint64 Result = 0;

		const char* RawPtr = Str.data();
		for (size_t i = 0; i < Str.size(); ++i)
		{
			// uint64로 1'000'000'007 * 257 정도는 문제 없음
			uint64 byte = static_cast<uint64>(RawPtr[i]);
			Result = (Result * BASE + byte) % MOD_NUM;
		}

		return Result;
	}
}

TArray<TArray<FString>>& FNamePool::GetComparisonTable()
{
	static TArray<TArray<FString>> ComparisonTable{ BUCKET_COUNT };
	return ComparisonTable;
}

TArray<TArray<FString>>& FNamePool::GetDisplayTable()
{
	static TArray<TArray<FString>> DisplayTable{ BUCKET_COUNT };
	return DisplayTable;
}

FNameEntry FNamePool::AddEntry(const FString& Item)
{
	FString LowerItem{ Item };

	for (size_t i = 0; i < LowerItem.size(); ++i)
	{
		if (LowerItem[i] >= 65 && LowerItem[i] <= 90)
		{
			LowerItem[i] += 32;
		}
	}

	uint64 ComparisonHash = Hash(LowerItem);
	uint64 DisplayHash = Hash(Item);

	FNameEntry Entry;

	// 비트 마스크로 빠른 나머지 계산
	Entry.ComparisonBucketIndex = ComparisonHash & (BUCKET_COUNT - 1);
	Entry.DisplayBucketIndex = DisplayHash & (BUCKET_COUNT - 1);

	TArray<FString>& ComparisonBucket = GetComparisonTable()[Entry.ComparisonBucketIndex];
	TArray<FString>& DisplayBucket = GetDisplayTable()[Entry.DisplayBucketIndex];

	bool bFound = false;
	for (size_t i = 0; i < ComparisonBucket.size(); ++i)
	{
		if (LowerItem == ComparisonBucket[i])
		{
			bFound = true;
			Entry.ComparisonIndex = static_cast<int32>(i);
			break;
		}
	}

	if (!bFound)
	{
		Entry.ComparisonIndex = static_cast<int32>(ComparisonBucket.size());
		ComparisonBucket.push_back(LowerItem);
	}


	bFound = false;
	for (size_t i = 0; i < DisplayBucket.size(); ++i)
	{
		if (Item == DisplayBucket[i])
		{
			bFound = true;
			Entry.DisplayIndex = static_cast<int32>(i);
			break;
		}
	}

	if (!bFound)
	{
		Entry.DisplayIndex = static_cast<int32>(DisplayBucket.size());
		DisplayBucket.push_back(Item);
	}

	return Entry;
}

const FString& FNamePool::GetComparisonString(const FNameEntry& Entry)
{
	auto& Table = GetComparisonTable();
	if (Entry.ComparisonBucketIndex < Table.size())
	{
		auto& Bucket = Table[Entry.ComparisonBucketIndex];
		if (Entry.ComparisonIndex >= 0 && static_cast<size_t>(Entry.ComparisonIndex) < Bucket.size())
		{
			return Bucket[Entry.ComparisonIndex];
		}
	}
	static const FString EmptyString{};
	return EmptyString;
}

const FString& FNamePool::GetDisplayString(const FNameEntry& Entry)
{
	auto& Table = GetDisplayTable();
	if (Entry.DisplayBucketIndex < Table.size())
	{
		auto& Bucket = Table[Entry.DisplayBucketIndex];
		if (Entry.DisplayIndex >= 0 && static_cast<size_t>(Entry.DisplayIndex) < Bucket.size())
		{
			return Bucket[Entry.DisplayIndex];
		}
	}
	static const FString EmptyString{};
	return EmptyString;
}