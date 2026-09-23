#pragma once

#include <filesystem>
#include <string_view>
#include <unordered_map>

#include "FMesh.h"
#include "Runtime/Core/TSortedMap.h"
#include "Runtime/Core/FString.h"
#include "Runtime/Core/TArray.h"
#include "Runtime/Core/IntTypes.h"
#include "Runtime/Math/FVector.h"
#include "Runtime/Math/FVector2.h"
#include "Runtime/Math/FVector4.h"
#include "FRenderResourceLibrary.h"

// Todo: Need to split class

#define EXPLICIT 0
#define SMOOTH 1
#define FLAT 2

struct FTriangleIndices {
    int32 Index[3] = { -1,-1,-1 };

    FTriangleIndices() = default;
    FTriangleIndices(int A, int B, int C) : Index{ A, B, C } {}
};

struct FCorner { int32 V, VT, VN; };

// 섹션 그룹화 키
struct FSectionKey
{
    int32 Object = -1;
    int32 Group = -1;
    int32 Material = -1;
    auto operator<=>(const FSectionKey&) const = default;
    // 비교 연산자 6개를 전부 만들어준다. 결과를 0과 비교해서 사용
    // 예시 if ((A <=> B) < 0) -> A가 작다
};

struct FObjGroupInfo
{
    FString Name;
};

struct FObjObjectInfo
{
    FString Name;
};

// Cooked Data
struct FObjModelData
{
    std::string PathFileName;

    TArray<FVertexData> Vertices;
    TArray<uint32> Indices;

    FName TextureName{ "None" };
    FName NormalTextureName{ "None" };
    FName SpecularTextureName{ "None" };
    bool bIsValid = false;

    TArray<FMeshSection> Sections;

    TArray<FObjMaterialInfo> Materials;
    // Todo: Bin - 메시 캐시에는 정의 대신 참조 MTL 경로와 섹션 ID만 저장한다.
    TArray<FString> MaterialLibraryPaths;
    TArray<FObjGroupInfo> Groups;
    TArray<FObjObjectInfo> ObjectNames;

    bool HasTextures() const
    {
        return (!TextureName.IsNone() && TextureName != FName("None")) ||
            (!NormalTextureName.IsNone() && NormalTextureName != FName("None")) ||
            (!SpecularTextureName.IsNone() && SpecularTextureName != FName("None"));
    }

    bool HasSections() const { return !Sections.empty(); }
};

// 원시 파싱 데이터
struct FObjInfo
{
    TArray<FVector4> VertexList;
    TArray<FVector> ColorList;
    TArray<FVector> UVList;
    TArray<FVector> NormalList;

    TArray<FTriangleIndices> VertexIndexList;
    TArray<FTriangleIndices> UVIndexList;
    TArray<FTriangleIndices> NormalIndexList;

    TArray<int32> MaterialList;
    TArray<int32> GroupList;
    TArray<int32> ObjectNamesList;
    TArray<int32> SmoothingGroupsList;

    TArray<FString> MaterialLibs;
    TArray<FObjMaterialInfo> Materials;

    TArray<FObjGroupInfo>  Groups;
    TArray<FObjObjectInfo> ObjectNames;
};

struct FCornerKey
{
    int32 V;
    int32 VT;
    int32 NormalKind;
    int32 NormalId;
    bool operator==(const FCornerKey&) const = default;
};

struct FCornerKeyHash
{
    size_t operator()(const FCornerKey& Key) const noexcept
    {
        size_t Hash = std::hash<int32>{}(Key.V);
        Hash = EngineUtil::HashCombine(Hash, std::hash<int32>{}(Key.VT));
        Hash = EngineUtil::HashCombine(Hash, std::hash<int32>{}(Key.NormalKind));
        Hash = EngineUtil::HashCombine(Hash, std::hash<int32>{}(Key.NormalId));
        return Hash;
    }
};

struct FSmoothingKey
{
    int32 VertexIndexNumber;
    int32 SmoothingGroupNumber;
    bool operator==(const FSmoothingKey&) const = default;
};

struct FSmoothingKeyHash
{
    size_t operator()(const FSmoothingKey& Key) const noexcept
    {
        size_t Hash = std::hash<int32>{}(Key.SmoothingGroupNumber);
        Hash = EngineUtil::HashCombine(Hash, std::hash<int32>{}(Key.VertexIndexNumber));
        return Hash;
    }
};

using FVertexMap = std::unordered_map<FCornerKey, uint32, FCornerKeyHash>;
using FSmoothingMap = std::unordered_map<FSmoothingKey, FVector, FSmoothingKeyHash>;

// OBJ 디코더 클래스
class FObjDecoder
{
public:
    bool LoadMaterials(
        const FString& MtlPath,
        const FString& BinaryPath,
        TArray<FObjMaterialInfo>& OutMaterials);
    bool LoadObj(const FString& ObjPath, const FString& BinaryPath, FObjModelData& OutModel);
    
    const TArray<FObjMaterialInfo>& GetMaterials() const;

private:
    bool DecodeMaterialsFromFile(const FString& Path, TArray<FObjMaterialInfo>& OutMaterials);

    bool SaveMaterialsBinary(const FString& Path, const TArray<FObjMaterialInfo>& Materials);
    bool LoadMaterialsBinary(const FString& Path, TArray<FObjMaterialInfo>& OutMaterials);

    bool DecodeFromFile(const FString& AbsolutePath, FObjModelData& Out);
    bool SaveObjModelBinary(const FString& Path, const FObjModelData& Model);
    bool LoadObjModelBinary(const FString& Path, FObjModelData& OutModel);

    std::filesystem::path GetAssetDir();
    bool IsUnder(const std::filesystem::path& TargetPath, const std::filesystem::path& BasePath);
    bool ResolveExistingFile(std::string_view FileName, std::filesystem::path& OutPath);
    FString ReadFileToString(std::string_view FileName);
    std::string_view Trim(std::string_view Text);
    std::string_view NextWord(std::string_view& Text);
    bool StringToFloat(std::string_view Text, float& Value);
    bool StringToInt(std::string_view Text, int32& Value);
    int32 ReadFloats(std::string_view Line, float* Out, int32 MaxCount);
    int32 ToZeroBased(int32 ObjIndex, size_t ListSize);
    bool ParseFaceToken(std::string_view Token, int32& V, int32& VT, int32& VN);
    std::string_view NextLine(std::string_view& Remaining);
    bool IsTextureOptionArg(std::string_view Word);
    FString ParseTexturePath(std::string_view Line);
    bool IsIndexValid(int32 Index, size_t ListSize);
    FVertexData MakeVertex(const FObjInfo& Info, const FCornerKey& Key, TArray<FVector>& NormalVectorList, FSmoothingMap& SmoothingMap);
    uint32 GetOrAddVertex(const FObjInfo& Info, const FCornerKey& Key, FVertexMap& Vertices, FObjModelData& Out, TArray<FVector>& NormalVectorList, FSmoothingMap& SmoothingMap);
    FCornerKey MakeCornerKey(int32 V, int32 VT, int32 VN, int32 S, int32 Triangle);
    void CalculateNormalVector(const FObjInfo& Info, TArray<FVector>& NormalVectorList, FSmoothingMap& SmoothingMap);
    const FObjMaterialInfo* FindCachedMaterial(std::string_view MaterialName) const;
    void ResolveSectionMaterials(FObjModelData& Model) const;

    // 정점 및 속성 추가
    void AddVertexList(std::string_view Line);
    void AddUVList(std::string_view Line);
    void AddNormalList(std::string_view Line);

    // 라인 및 면 파싱
    void ParseLine(std::string_view Line);
    void ParseFace(std::string_view Line);

    // 머티리얼 라이브러리
    void AddMaterialLib(std::string_view Line);
    // Todo: Bin - MTL을 다시 파싱하지 않고 이미 로딩한 정보를 연결한다.
    bool ImportMaterialLibrary(const FString& Path);
    void UseMaterial(std::string_view Line);

    // 그룹 및 오브젝트
    void UseGroup(std::string_view Line);
    int32 FindOrAddGroup(std::string_view Name);

    void UseSmoothingGroup(std::string_view Line);
    void SetSmoothingGroup(std::string_view Line);

    void UseObjectName(std::string_view Line);
    int32 FindOrAddObjectName(std::string_view Name);

    FObjInfo ParseObjFile(const FString& File);
    FObjInfo StartObjFileParser(const FString& PathFileName);

    // 머티리얼 파싱
    void ParseMtlFile(const FString& File);
    void ParseMtlLine(std::string_view Line);
    int32 FindOrAddMaterial(std::string_view Name);

    bool IsConvexDot(FVector PrevVertexPosition, FVector EarVertexPosition, FVector NextVertexPosition, FVector Normal);
    bool IsPointInTriangle(FVector A, FVector B, FVector C, FVector Q, FVector Normal);
    void StartEarClipping(const TArray<FCorner>& Corners);

    bool CookStaticMesh(const FObjInfo& Info, FObjModelData& Out);

private:
    static constexpr std::string_view Spaces = " \t\r\n";

    // Todo: Bin - 이름이 전역적으로 유일한 공유 머티리얼 목록.
    TArray<FObjMaterialInfo> CachedMaterials;
    bool bMaterialsLoaded = false;

    FObjInfo ObjInfo;

    FString ObjDirectory;
    int32 CurrentMaterial = -1;
    int32 DefiningMaterial = -1;

    int32 CurrentGroup = -1;
    int32 CurrentObjectName = -1;
    int32 CurrentSmoothingGroup = 0; // 0 or off 사용안함
};
