#pragma once

#include <filesystem>
#include <fstream>

#include "FBinArchive.h"

class FWindowsBinWriter
{
public:
    static bool Save(const std::filesystem::path& Path, const FBinArchive& Archive);
};
