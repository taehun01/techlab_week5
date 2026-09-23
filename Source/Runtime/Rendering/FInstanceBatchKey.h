#pragma once

#include "Runtime/Core/FName.h"
#include "Runtime/Utility/EngineUtil.h"

#include <functional>

struct FInstanceBatchKey
{
	FName MaterialID;
	FName MeshID;

	bool operator==(const FInstanceBatchKey& Other) const = default;
};

template <>
struct std::hash<FInstanceBatchKey>
{
	size_t operator()(const FInstanceBatchKey& Key) const noexcept
	{
		const size_t MaterialHash = std::hash<FName>{}(Key.MaterialID);
		const size_t MeshHash = std::hash<FName>{}(Key.MeshID);
		return EngineUtil::HashCombine(MaterialHash, MeshHash);
	}
};
