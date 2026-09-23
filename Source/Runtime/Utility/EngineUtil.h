#pragma once

#include <cstddef>

namespace EngineUtil
{

	/// <summary>
	/// 두 해시 값을 하나의 해시 값으로 만듭니다.
	/// </summary>
	/// <param name="FirstHash">해시1</param>
	/// <param name="SecondHash">해시2</param>
	/// <returns>새로 만든 해시값</returns>
	size_t HashCombine(size_t FirstHash, size_t SecondHash);
}
