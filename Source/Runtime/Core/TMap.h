#pragma once

#include <unordered_map>
#include <unordered_set>

template<typename T, typename U>
using TMap = std::unordered_map<T, U>;

template<typename T>
using TSet = std::unordered_set<T>;
