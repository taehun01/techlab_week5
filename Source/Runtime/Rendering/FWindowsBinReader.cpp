
#include "FWindowsBinReader.h"
#include "Runtime/Core/Log.h"

bool FWindowsBinReader::Load(const std::filesystem::path& Path, FBinArchive* OutArchive)
{
    // 파일 전체를 읽은 후 Archive에 넘긴다. 실패하면 기존 Archive는 유지한다.
    // std::ifstream는 RAII 원칙을 따름. Fail 해도 close()를 명시적으로 해주지 않아도 됨
    std::ifstream LoadFileStream(Path, std::ios::binary | std::ios::ate);
    if (!LoadFileStream)
    {
        UE_LOG("FWindowsBinReader Load(), 파일 스트림을 여는데 실패했습니다.");

        return false;
    }

    const std::streampos FileEnd = LoadFileStream.tellg();
    if (FileEnd == std::streampos(-1))
    {
        return false;
    }

    const size_t FileSize = static_cast<size_t>(FileEnd);
    if (FileSize < 0 || static_cast<uint64>(FileSize) > FBinArchive::MaxArchiveBytes)
    {
        return false;
    }

    TArray<uint8> LoadedBytes(FileSize);

    LoadFileStream.seekg(0, std::ios::beg);
    if (!LoadedBytes.empty())
    {
        LoadFileStream.read(reinterpret_cast<char*>(LoadedBytes.data()), static_cast<std::streamsize>(FileSize));

        // 정확히 읽지 못했다면 실패
        if (!LoadFileStream)
        {
            UE_LOG("FWindowsBinWrite Load(), 파일 읽기에 실패했습니다.");

            return false;
        }
    }

    OutArchive->SetBytes(std::move(LoadedBytes));

    return true;
}
