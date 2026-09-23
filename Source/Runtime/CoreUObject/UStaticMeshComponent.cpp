#include "UStaticMeshComponent.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "Runtime/Engine/FArchive.h"
#include "Runtime/Rendering/FRenderResourceLibrary.h"
#include "Runtime/Core/Log.h"
#include "UClass.h"
#include "Source/Runtime/Input/FInputManager.h"



IMPLEMENT_UCLASS(UStaticMeshComponent, UMeshComponent)

void UStaticMeshComponent::Initialize()
{
    Super::Initialize();
}


bool UStaticMeshComponent::SetStaticMesh(UStaticMesh* InStaticMesh)
{
    StaticMesh = InStaticMesh;
    if (StaticMesh)
    {
        CalcLocalBounds();
    }
    return true;
}

const FName& UStaticMeshComponent::GetMeshID() const
{
    if (StaticMesh && !StaticMesh->MeshId.IsNone())
    {
        return StaticMesh->MeshId;
    }
    return Super::GetMeshID();
}

FName UStaticMeshComponent::GetMaterialID() const
{
    return GetMaterial(0);
}

FAxisAlignedBoundingBox UStaticMeshComponent::CalcLocalBounds()
{
    if (StaticMesh)
    {
        return StaticMesh->GetBounds();
    }
    return Super::CalcLocalBounds();
}

TArray<FRenderData> UStaticMeshComponent::GetRenderDatas(const FCamera& Camera)
{
    TArray<FRenderData> OutDatas;

    if (!StaticMesh || !StaticMesh->StaticMeshAsset)
    {
        return OutDatas;
    }


    const FName CurrentMeshId = GetMeshID();

    if (!StaticMesh || !StaticMesh->StaticMeshAsset)
    {
        return OutDatas;
    }

    const auto& Sections = StaticMesh->StaticMeshAsset->Sections;

    if (!Sections.empty())
    {
        const size_t Count = std::min(StaticMesh->Materials.size(), Sections.size());
        OutDatas.reserve(Count);

        for (size_t i = 0; i < Count; ++i)
        {
            FRenderData rdata;
            rdata.MeshId = CurrentMeshId;
            rdata.MaterialId = GetMaterial(static_cast<int32>(i));

            rdata.startidx = Sections[i].FirstIndex;
            rdata.indicesCount = Sections[i].IndexCount;

            if (bIsMovingUV)
            {
                float MouseDelta = FInputManager::Get().GetMouseWheelScroll();
                offset -= MouseDelta * 0.05f;
                offset -= floorf(offset);
                rdata.Constants.UVOffset.X = offset;
            }


            OutDatas.push_back(std::move(rdata));
        }
    }
    else
    {
        FRenderData rdata;
        rdata.MeshId = CurrentMeshId;
        rdata.MaterialId = GetMaterial(0);

        rdata.startidx = 0;
        rdata.indicesCount = -1; 

        if (bIsMovingUV)
        {
            float MouseDelta = FInputManager::Get().GetMouseWheelScroll();
            offset -= MouseDelta * 0.05f;
            offset -= floorf(offset);
            rdata.Constants.UVOffset.X = offset;
        }

        OutDatas.push_back(std::move(rdata));
    }

    return OutDatas;
}

const FRenderData& UStaticMeshComponent::GetPureRenderData() const
{
    FRenderData& MutableData = const_cast<FRenderData&>(RenderDatas.at(0));
    MutableData.MeshId = GetMeshID();
    MutableData.MaterialId = GetMaterial(0);
    MutableData.startidx = 0;
    MutableData.indicesCount = (StaticMesh && StaticMesh->StaticMeshAsset)
        ? StaticMesh->StaticMeshAsset->GetIndexCount()
        : -1;

    return MutableData;
}

void UStaticMeshComponent::SetMaterial(int32 Slot, const FName& InMaterialId)
{
    if (Slot < 0) return;
    if (Slot >= static_cast<int32>(OverrideMaterials.size()))
    {
        OverrideMaterials.resize(Slot + 1, FName("None"));
    }

    OverrideMaterials[Slot] = InMaterialId;
}

FName UStaticMeshComponent::GetMaterial(int32 Slot) const
{
    if (Slot >= 0 && Slot < static_cast<int32>(OverrideMaterials.size()) && !OverrideMaterials[Slot].IsNone())
    {
        return OverrideMaterials[Slot];
    }

    if (StaticMesh && StaticMesh->Materials.size() > Slot)
    {
        return FName(StaticMesh->Materials[Slot]);
    }

    return FName("Simple");
}

void UStaticMeshComponent::Serialize(FArchive& Archive) const
{
    Super::Serialize(Archive);


    Archive.SetString("StaticMesh", StaticMesh ? StaticMesh->MeshId.ToString() : FString());

    TArray<FString> SerializeMaterials;
    for (FName Material : OverrideMaterials)
    {
        SerializeMaterials.push_back(Material.ToString());
    }
    Archive.SetArray("OverrideMaterials", SerializeMaterials);
    Archive.SetBool("bIsMovingUV", bIsMovingUV);

}

void UStaticMeshComponent::Deserialize(const FArchive& Archive)
{
    Super::Deserialize(Archive);

    if (!Archive.IsNull("StaticMesh"))
    {
        FString StaticMeshId = Archive.GetString("StaticMesh");
        if (UStaticMesh* Found = FRenderResourceLibrary::Get().GetUStaticMesh(FName(StaticMeshId)))
        {
            SetStaticMesh(Found);
        }
        else
        {
            UE_LOG_WARN("[UStaticMeshComponent::Deserialize] 메시 %s 를 찾을 수 없습니다.", StaticMeshId.c_str());
        }
    }

    if (!Archive.IsNull("OverrideMaterials"))
    {
        TArray<FString> DeserializeMaterials;
        OverrideMaterials.clear();
        OverrideMaterials.reserve(DeserializeMaterials.size());
        DeserializeMaterials = Archive.GetArray<FString>("OverrideMaterials");
        for (const FString& Material : DeserializeMaterials)
        {
            OverrideMaterials.push_back(Material);
        }
    }
    if (!Archive.IsNull("bIsMovingUV"))
    {
        bIsMovingUV = Archive.GetBool("bIsMovingUV");
    }
}

