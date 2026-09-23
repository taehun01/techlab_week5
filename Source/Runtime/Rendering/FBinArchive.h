#pragma once

#include "Runtime/Core/IntTypes.h"
#include "Runtime/Core/FString.h"
#include "Runtime/Core/TArray.h"
#include "FObjDecoder.h"

class FBinArchive
{
public:
    FBinArchive();
    ~FBinArchive() = default;

    void Clear();
    void ResetReadPosition();
    const TArray<uint8>& GetBytes() const;
    size_t GetRemainingBytes() const;
    void SetBytes(TArray<uint8>&& InBytes);

    bool SerializeUInt32(uint32 Value);
    bool DeserializeUInt32(uint32& Value);
    bool SerializeInt32(int32 Value);
    bool DeserializeInt32(int32& Value);
    bool SerializeFloat(float Value);
    bool DeserializeFloat(float& Value);
    bool SerializeBool(bool Value);
    bool DeserializeBool(bool& Value);

    // 문자열/배열 객체의 내부 포인터가 아닌 실제 문자만 저장한다.
    bool SerializeString(const FString& Value);
    bool DeserializeString(FString& Value);

    bool SerializeObjModel(const FObjModelData& Model);
    bool DeserializeObjModel(FObjModelData& OutModel);
    bool SerializeMaterials(const TArray<FObjMaterialInfo>& Materials);
    bool DeserializeMaterials(TArray<FObjMaterialInfo>& OutMaterials);

private:
    bool SerializeBytes(const void* Data, uint32 ByteCount);
    bool DeserializeBytes(void* OutData, uint32 ByteCount);

    bool SerializeValue(const FVector& Value);
    bool DeserializeValue(FVector& Value);
    bool SerializeValue(const FVertexData& Value);
    bool DeserializeValue(FVertexData& Value);
    bool SerializeValue(const FMeshSection& Value);
    bool DeserializeValue(FMeshSection& Value);
    bool SerializeValue(const FObjMaterialInfo& Value);
    bool DeserializeValue(FObjMaterialInfo& Value);
    bool SerializeValue(const FObjGroupInfo& Value);
    bool DeserializeValue(FObjGroupInfo& Value);
    bool SerializeValue(const FObjObjectInfo& Value);
    bool DeserializeValue(FObjObjectInfo& Value);
    bool SerializeValue(const FString& Value);
    bool DeserializeValue(FString& Value);
    bool SerializeValue(const uint32& Value);
    bool DeserializeValue(uint32& Value);

    template<typename T>
    bool SerializeArray(const TArray<T>& Values);

    template<typename T>
    bool DeserializeArray(TArray<T>& Values);

    bool ValidateObjModel(const FObjModelData& Model);

public:
    static constexpr size_t MaxArchiveBytes = 512ULL * 1024 * 1024;

private:
    static constexpr uint32 ObjFileSignature = 0x4D4A424F; // "OBJM"
    static constexpr uint32 MaterialFileSignature = 0x4C54414D; // "MATL"
    static constexpr uint32 MaxElementCount = 16 * 1024 * 1024;

    TArray<uint8> Bytes;
    size_t ReadOffset;
};
