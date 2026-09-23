// Todo: Bin - 이 파일 전체는 바이너리 캐시 기능을 위해 추가한 구현이다.
#pragma once

#include <cassert>
#include <cmath>
#include <cstring>
#include <utility>

#include "Runtime/Core/FString.h"
#include "Runtime/Core/TArray.h"

#include "FBinArchive.h"

FBinArchive::FBinArchive()
    : ReadOffset(0)
{
}

void FBinArchive::Clear()
{
    Bytes.clear();

    ResetReadPosition();
}

void FBinArchive::ResetReadPosition()
{
    ReadOffset = 0;
}

const TArray<uint8>& FBinArchive::GetBytes() const
{
    return Bytes;
}

size_t FBinArchive::GetRemainingBytes() const
{
    return Bytes.size() - ReadOffset;
}

void FBinArchive::SetBytes(TArray<uint8>&& InBytes)
{
    assert(InBytes.size() <= MaxArchiveBytes);

    Bytes = std::move(InBytes);
    ResetReadPosition();
}


bool FBinArchive::SerializeUInt32(uint32 Value)
{
    return SerializeBytes(&Value, sizeof(Value));
}

bool FBinArchive::DeserializeUInt32(uint32& Value)
{
    return DeserializeBytes(&Value, sizeof(Value));
}

bool FBinArchive::SerializeInt32(int32 Value)
{
    return SerializeBytes(&Value, sizeof(Value));
}

bool FBinArchive::DeserializeInt32(int32& Value)
{
    return DeserializeBytes(&Value, sizeof(Value));
}

bool FBinArchive::SerializeFloat(float Value)
{
    return SerializeBytes(&Value, sizeof(Value));
}

bool FBinArchive::DeserializeFloat(float& Value)
{
    return DeserializeBytes(&Value, sizeof(Value));
}

bool FBinArchive::SerializeBool(bool bValue)
{
    const uint8 Byte = bValue ? 1 : 0;

    return SerializeBytes(&Byte, sizeof(Byte));
}

bool FBinArchive::DeserializeBool(bool& bValue)
{
    uint8 Byte = 0;
    if (!DeserializeBytes(&Byte, sizeof(Byte)) || Byte > 1)
    {
        return false;
    }

    bValue = Byte != 0;

    return true;
}

// 문자열/배열 객체의 내부 포인터가 아닌 실제 문자만 저장한다.
bool FBinArchive::SerializeString(const FString& Value)
{
    if (Value.size() > MaxArchiveBytes)
    {
        return false;
    }

    return SerializeUInt32(static_cast<uint32>(Value.size())) 
        && SerializeBytes(Value.data(), Value.size());
}

bool FBinArchive::DeserializeString(FString& Value)
{
    uint32 Length = 0;
    if (!DeserializeUInt32(Length) || Length > GetRemainingBytes())
    {
        return false;
    }

    FString Loaded(Length, '\0');

    if (!DeserializeBytes(Loaded.data(), Length))
    {
        return false;
    }

    Value = std::move(Loaded);

    return true;
}

// 메모리 내용을 배열 끝에 추가한다.
bool FBinArchive::SerializeBytes(const void* Data, uint32 ByteCount)
{
    assert(Data != nullptr);

    if (ByteCount > MaxArchiveBytes - Bytes.size())
    {
        return false;
    }

    if (ByteCount == 0)
    {
        return true;
    }

    const auto* BeginPtr = static_cast<const uint8*>(Data);
    Bytes.insert(Bytes.end(), BeginPtr, BeginPtr + ByteCount);

    return true;
}

// 읽기 범위를 확인한 뒤, 값을 복원하고 커서를 이동한다.
bool FBinArchive::DeserializeBytes(void* OutData, uint32 ByteCount)
{
    assert(OutData != nullptr);

    if (ByteCount > GetRemainingBytes())
    {
        return false;
    }

    if (ByteCount == 0)
    {
        return true;
    }

    std::memcpy(OutData, Bytes.data() + ReadOffset, ByteCount);
    ReadOffset += ByteCount;

    return true;
}

bool FBinArchive::SerializeValue(const FVector& Vector)
{
    return SerializeFloat(Vector.X)
        && SerializeFloat(Vector.Y)
        && SerializeFloat(Vector.Z);
}

bool FBinArchive::DeserializeValue(FVector& Vector)
{
    return DeserializeFloat(Vector.X)
        && DeserializeFloat(Vector.Y)
        && DeserializeFloat(Vector.Z);
}

bool FBinArchive::SerializeValue(const FVertexData& Value)
{
    return SerializeFloat(Value.x)
        && SerializeFloat(Value.y)
        && SerializeFloat(Value.z)
        && SerializeFloat(Value.r)
        && SerializeFloat(Value.g)
        && SerializeFloat(Value.b)
        && SerializeFloat(Value.a)
        && SerializeFloat(Value.u)
        && SerializeFloat(Value.v)
        && SerializeFloat(Value.nx)
        && SerializeFloat(Value.ny)
        && SerializeFloat(Value.nz)
        && SerializeFloat(Value.tx)
        && SerializeFloat(Value.ty)
        && SerializeFloat(Value.tz)
        && SerializeFloat(Value.bx)
        && SerializeFloat(Value.by)
        && SerializeFloat(Value.bz);
}

bool FBinArchive::DeserializeValue(FVertexData& Value)
{
    return DeserializeFloat(Value.x)
        && DeserializeFloat(Value.y)
        && DeserializeFloat(Value.z)
        && DeserializeFloat(Value.r)
        && DeserializeFloat(Value.g)
        && DeserializeFloat(Value.b)
        && DeserializeFloat(Value.a)
        && DeserializeFloat(Value.u)
        && DeserializeFloat(Value.v)
        && DeserializeFloat(Value.nx)
        && DeserializeFloat(Value.ny)
        && DeserializeFloat(Value.nz)
        && DeserializeFloat(Value.tx)
        && DeserializeFloat(Value.ty)
        && DeserializeFloat(Value.tz)
        && DeserializeFloat(Value.bx)
        && DeserializeFloat(Value.by)
        && DeserializeFloat(Value.bz);
}

bool FBinArchive::SerializeValue(const FMeshSection& Value)
{
    return SerializeUInt32(Value.FirstIndex)
        && SerializeUInt32(Value.IndexCount)
        && SerializeString(Value.MaterialName)
        && SerializeValue(Value.LocalBounds.Min)
        && SerializeValue(Value.LocalBounds.Max);
}

bool FBinArchive::DeserializeValue(FMeshSection& Value)
{
    return DeserializeUInt32(Value.FirstIndex)
        && DeserializeUInt32(Value.IndexCount)
        && DeserializeString(Value.MaterialName)
        && DeserializeValue(Value.LocalBounds.Min)
        && DeserializeValue(Value.LocalBounds.Max);
}

bool FBinArchive::SerializeValue(const FObjMaterialInfo& Value)
{
    return SerializeString(Value.MaterialName)
        && SerializeValue(Value.Ambient)
        && SerializeValue(Value.Diffuse)
        && SerializeValue(Value.Specular)
        && SerializeValue(Value.Emissive)
        && SerializeValue(Value.TransmissionFilter)
        && SerializeFloat(Value.SpecularExponent)
        && SerializeFloat(Value.Opacity)
        && SerializeFloat(Value.OpticalDensity)
        && SerializeInt32(Value.IlluminationModel)
        && SerializeString(Value.DiffuseTextureName)
        && SerializeString(Value.AmbientTextureName)
        && SerializeString(Value.SpecularTextureName)
        && SerializeString(Value.AlphaTextureName)
        && SerializeString(Value.NormalTextureName)
        && SerializeString(Value.EmissiveTexture)
        && SerializeString(Value.SpecularExponentTexture)
        && SerializeString(Value.ReflectionTexture)
        && SerializeString(Value.DisplacementTexture)
        && SerializeString(Value.DecalTexture);
}

bool FBinArchive::DeserializeValue(FObjMaterialInfo& Value)
{
    return DeserializeString(Value.MaterialName)
        && DeserializeValue(Value.Ambient)
        && DeserializeValue(Value.Diffuse)
        && DeserializeValue(Value.Specular)
        && DeserializeValue(Value.Emissive)
        && DeserializeValue(Value.TransmissionFilter)
        && DeserializeFloat(Value.SpecularExponent)
        && DeserializeFloat(Value.Opacity)
        && DeserializeFloat(Value.OpticalDensity)
        && DeserializeInt32(Value.IlluminationModel)
        && DeserializeString(Value.DiffuseTextureName)
        && DeserializeString(Value.AmbientTextureName)
        && DeserializeString(Value.SpecularTextureName)
        && DeserializeString(Value.AlphaTextureName)
        && DeserializeString(Value.NormalTextureName)
        && DeserializeString(Value.EmissiveTexture)
        && DeserializeString(Value.SpecularExponentTexture)
        && DeserializeString(Value.ReflectionTexture)
        && DeserializeString(Value.DisplacementTexture)
        && DeserializeString(Value.DecalTexture);
}

bool FBinArchive::SerializeValue(const FObjGroupInfo& Value)
{
    return SerializeString(Value.Name);
}

bool FBinArchive::DeserializeValue(FObjGroupInfo& Value)
{
    return DeserializeString(Value.Name);
}

bool FBinArchive::SerializeValue(const FObjObjectInfo& Value)
{
    return SerializeString(Value.Name);
}

bool FBinArchive::DeserializeValue(FObjObjectInfo& Value)
{
    return DeserializeString(Value.Name);
}

bool FBinArchive::SerializeValue(const FString& Value)
{
    return SerializeString(Value);
}

bool FBinArchive::DeserializeValue(FString& Value)
{
    return DeserializeString(Value);
}

bool FBinArchive::SerializeValue(const uint32& Value)
{
    return SerializeUInt32(Value);
}

bool FBinArchive::DeserializeValue(uint32& Value)
{
    return DeserializeUInt32(Value);
}

template<typename T>
bool FBinArchive::SerializeArray(const TArray<T>& Values)
{
    if (Values.size() > MaxElementCount
        || !SerializeUInt32(static_cast<uint32>(Values.size())))
    {
        return false;
    }

    for (const T& Value : Values)
    {
        if (!SerializeValue(Value))
        {
            return false;
        }
    }

    return true;
}

template<typename T>
bool FBinArchive::DeserializeArray(TArray<T>& Values)
{
    uint32 Count = 0;
    if (!DeserializeUInt32(Count)
        || Count > MaxElementCount
        || Count > GetRemainingBytes())
    {
        return false;
    }

    Values.clear();
    for (uint32 Index = 0; Index < Count; ++Index)
    {
        T Value{};
        if (!DeserializeValue(Value))
        {
            return false;
        }

        Values.push_back(std::move(Value));
    }

    return true;
}

bool FBinArchive::ValidateObjModel(const FObjModelData& Model)
{
    if (Model.Vertices.empty() || Model.Indices.empty()
        || Model.Indices.size() % 3 != 0)
    {
        return false;
    }

    for (const FVertexData& Vertex : Model.Vertices)
    {
        const float Fields[] = {
            Vertex.x, Vertex.y, Vertex.z,
            Vertex.r, Vertex.g, Vertex.b, Vertex.a,
            Vertex.u, Vertex.v,
            Vertex.nx, Vertex.ny, Vertex.nz,
            Vertex.tx, Vertex.ty, Vertex.tz,
            Vertex.bx, Vertex.by, Vertex.bz
        };

        for (float Field : Fields)
        {
            if (!std::isfinite(Field))
            {
                return false;
            }
        }
    }

    for (uint32 Index : Model.Indices)
    {
        if (Index >= Model.Vertices.size())
        {
            return false;
        }
    }

    for (const FMeshSection& Section : Model.Sections)
    {
        if (Section.IndexCount == 0
            || Section.IndexCount % 3 != 0
            || Section.FirstIndex % 3 != 0
            || Section.FirstIndex > Model.Indices.size()
            || Section.IndexCount > Model.Indices.size() - Section.FirstIndex)
        {
            return false;
        }
    }

    return true;
}

bool FBinArchive::SerializeObjModel(const FObjModelData& Model)
{
    if (!ValidateObjModel(Model))
    {
        return false;
    }

    Clear();
    return SerializeUInt32(ObjFileSignature)
        && SerializeString(Model.PathFileName)
        && SerializeArray(Model.Vertices)
        && SerializeArray(Model.Indices)
        && SerializeArray(Model.Sections)
        && SerializeArray(Model.MaterialLibraryPaths)
        && SerializeArray(Model.Groups)
        && SerializeArray(Model.ObjectNames);
}

bool FBinArchive::DeserializeObjModel(FObjModelData& OutModel)
{
    ResetReadPosition();

    uint32 FileSignature = 0;
    if (!DeserializeUInt32(FileSignature)
        || FileSignature != ObjFileSignature)
    {
        return false;
    }

    FObjModelData Loaded;
    if (!DeserializeString(Loaded.PathFileName)
        || !DeserializeArray(Loaded.Vertices)
        || !DeserializeArray(Loaded.Indices)
        || !DeserializeArray(Loaded.Sections)
        || !DeserializeArray(Loaded.MaterialLibraryPaths)
        || !DeserializeArray(Loaded.Groups)
        || !DeserializeArray(Loaded.ObjectNames)
        || GetRemainingBytes() != 0
        || !ValidateObjModel(Loaded))
    {
        return false;
    }

    Loaded.bIsValid = true;
    OutModel = std::move(Loaded);
    return true;
}

bool FBinArchive::SerializeMaterials(const TArray<FObjMaterialInfo>& Materials)
{
    Clear();
    return SerializeUInt32(MaterialFileSignature)
        && SerializeArray(Materials);
}

bool FBinArchive::DeserializeMaterials(TArray<FObjMaterialInfo>& OutMaterials)
{
    ResetReadPosition();

    uint32 FileSignature = 0;
    if (!DeserializeUInt32(FileSignature)
        || FileSignature != MaterialFileSignature)
    {
        return false;
    }

    TArray<FObjMaterialInfo> Loaded;
    if (!DeserializeArray(Loaded) || GetRemainingBytes() != 0)
    {
        return false;
    }

    for (const FObjMaterialInfo& Material : Loaded)
    {
        if (Material.MaterialName.empty() || !std::isfinite(Material.Opacity))
        {
            return false;
        }
    }

    OutMaterials = std::move(Loaded);
    return true;
}


