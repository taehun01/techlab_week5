#pragma once

#include <filesystem>
#include <fstream>

#include "FBinArchive.h"

class FWindowsBinReader
{
public:
    static bool Load(const std::filesystem::path& Path, FBinArchive* OutArchive);
};
