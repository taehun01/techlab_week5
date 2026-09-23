
#include "FWindowsBinWriter.h"
#include "Runtime/Core/Log.h"

bool FWindowsBinWriter::Save(const std::filesystem::path& Path, const FBinArchive& Archive)
{
    // Archive의 내용을 해석하지 않고 그대로 디스크에 기록한다.

    const TArray<uint8_t>& Bytes = Archive.GetBytes();
    if (Bytes.empty())
    {
        return false;
    }

    std::ofstream SaveBinFileStream(Path, std::ios::binary | std::ios::trunc);
    if (!SaveBinFileStream)
    {
        UE_LOG("FWindowsBinWrite Save(), 파일 스트림을 여는데 실패했습니다.");

        return false;
    }

    SaveBinFileStream.write(reinterpret_cast<const char*>(Bytes.data()), static_cast<std::streamsize>(Bytes.size()));
    SaveBinFileStream.close(); // 마지막 버퍼 기록의 실패까지 확인한다.

    if (SaveBinFileStream.fail())
    {
        UE_LOG("FWindowsBinWrite Save(), 파일 기록에 실패했습니다.");

        return false;
    }

    return true;
}
