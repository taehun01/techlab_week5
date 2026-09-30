#include "UStaticMeshComponent.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "Runtime/Engine/FArchive.h"
#include "Runtime/Engine/UScene.h"
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
        UpdateLocalBounds();
    }

    // 메시가 바뀌면 로컬 바운드도 바뀌므로 옥트리 갱신 예약
    if (Scene)
    {
        Scene->MarkBoundsDirty(this);
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

void UStaticMeshComponent::UpdateLocalBounds()
{
    if (StaticMesh)
    {
        LocalBounds = StaticMesh->GetBounds();
    }
}

// UV 스크롤은 프레임당 한 번만 갱신한다. GetRenderDatas는 뷰포트·섹션마다 호출되므로 여기서 누적하면 안 된다.
void UStaticMeshComponent::Update(float DeltaTime)
{
    Super::Update(DeltaTime);

    if (bIsMovingUV)
    {
        const float MouseDelta = FInputManager::Get().GetMouseWheelScroll();
        offset -= MouseDelta * 0.05f;
        offset -= floorf(offset);
    }
}

TArray<FRenderData> UStaticMeshComponent::GetRenderDatas(const FCamera& Camera)
{
    TArray<FRenderData> OutDatas;
    AppendRenderDatas(Camera, OutDatas);
    return OutDatas;
}

void UStaticMeshComponent::AppendRenderDatas(const FCamera& Camera, TArray<FRenderData>& OutDatas, uint32 LodLevel)
{
    if (!StaticMesh || !StaticMesh->StaticMeshAsset)
    {
        return;
    }

    const FName CurrentMeshId = GetMeshID();
    // UStaticMesh가 이미 들고 있는 메시 에셋. 렌더러가 MeshId 문자열로 다시 찾지 않도록 넘긴다.
    FStaticMesh* const MeshAsset = StaticMesh->StaticMeshAsset.get();

    const auto& Sections = StaticMesh->StaticMeshAsset->Sections;

    if (!Sections.empty())
    {
        const size_t Count = std::min(StaticMesh->Materials.size(), Sections.size());

        for (size_t i = 0; i < Count; ++i)
        {
            FRenderData rdata;
            rdata.MeshId = CurrentMeshId;
            rdata.MeshPtr = MeshAsset;
            FillMaterial(static_cast<int32>(i), rdata);
            uint32 Lod = std::min(LodLevel, MeshAsset->GetMeshLodCount() - 1);
            rdata.startidx = Sections[i].Lods[Lod].FirstIndex;
            rdata.indicesCount = Sections[i].Lods[Lod].IndexCount;

            if (bIsMovingUV)
            {
                rdata.Constants.UVOffset.X = offset;
            }

            OutDatas.push_back(std::move(rdata));
        }
    }
    else
    {
        FRenderData rdata;
        rdata.MeshId = CurrentMeshId;
        rdata.MeshPtr = MeshAsset;
        FillMaterial(0, rdata);

        rdata.startidx = 0;
        rdata.indicesCount = -1; 

        if (bIsMovingUV)
        {
            rdata.Constants.UVOffset.X = offset;
        }

        OutDatas.push_back(std::move(rdata));
    }
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

void UStaticMeshComponent::FillMaterial(int32 Slot, FRenderData& OutData) const
{
    // 1) 컴포넌트 오버라이드: 이름만 넘기고 렌더러가 찾는다 (기존 경로)
    if (Slot >= 0 && Slot < static_cast<int32>(OverrideMaterials.size()) && !OverrideMaterials[Slot].IsNone())
    {
        OutData.MaterialId = OverrideMaterials[Slot];
        return;
    }

    // 2) 메시 슬롯: UStaticMesh가 연결해 둔 머티리얼 포인터를 넘긴다.
    //    등록되지 않은 이름이면 포인터는 nullptr이고, 렌더러가 기존처럼 Simple로 대체한다.
    if (StaticMesh && Slot >= 0 && Slot < StaticMesh->GetMaterialSlotCount())
    {
        OutData.MaterialPtr = StaticMesh->ResolveMaterialSlot(Slot, OutData.MaterialId);
        return;
    }

    // 3) 슬롯이 없으면 기본 머티리얼
    static const FName SimpleMaterialId("Simple");
    OutData.MaterialId = SimpleMaterialId;
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

