#include "FMaterial.h"
#include "FRenderer.h"
#include "FRenderResourceLibrary.h"
#include "Runtime/Core/Log.h"
#include <d3d11.h>
#include <algorithm>

void FMaterial::SetPipeLine(const TSharedPtr<FRenderPipeline>& InPipeline)
{
    Pipeline = InPipeline;
}

TSharedPtr<FMaterial> FMaterial::Clone() const
{
    auto NewMat = MakeShared<FMaterial>();
    NewMat->MaterialId = this->MaterialId;
    NewMat->Pipeline = this->Pipeline;
    NewMat->WireframePipeline = this->WireframePipeline;

    // 슬롯 텍스처 복사
    for (size_t i = 0; i < static_cast<size_t>(EMaterialTextureSlot::Count); ++i)
    {
        NewMat->Textures[i] = this->Textures[i];
    }
    return NewMat;
}


void FMaterial::SetTexture(EMaterialTextureSlot Slot, const TSharedPtr<FTexture>& InTexture)
{
    const size_t Index = static_cast<size_t>(Slot);
    if (Index < static_cast<size_t>(EMaterialTextureSlot::Count))
    {
        Textures[Index] = InTexture;
    }
}

TSharedPtr<FTexture> FMaterial::GetTexture(EMaterialTextureSlot Slot) const
{
    const size_t Index = static_cast<size_t>(Slot);
    if (Index < static_cast<size_t>(EMaterialTextureSlot::Count))
    {
        return Textures[Index];
    }
    return nullptr;
}

bool FMaterial::SetTextureByName(EMaterialTextureSlot Slot, const FName& InTextureName)
{
    auto& lib = FRenderResourceLibrary::Get();

    auto it = lib.GetTexture(InTextureName);
    if (it == nullptr)
    {
        UE_LOG("[Material] Texture '%s' not found in texture map.", InTextureName.ToString().c_str());
        return false;
    }

    SetTexture(Slot, it);
    return true;
}

void FMaterial::BindResources(ID3D11DeviceContext& Context) const
{
    // 슬롯별 리소스 뷰 배열 구성
    ID3D11ShaderResourceView* SRVs[static_cast<size_t>(EMaterialTextureSlot::Count)] = {
        Textures[static_cast<size_t>(EMaterialTextureSlot::Diffuse)] ? Textures[static_cast<size_t>(EMaterialTextureSlot::Diffuse)]->GetSRV() : nullptr,
        Textures[static_cast<size_t>(EMaterialTextureSlot::Normal)] ? Textures[static_cast<size_t>(EMaterialTextureSlot::Normal)]->GetSRV() : nullptr,
        Textures[static_cast<size_t>(EMaterialTextureSlot::Specular)] ? Textures[static_cast<size_t>(EMaterialTextureSlot::Specular)]->GetSRV() : nullptr
    };

    // 픽셀 셰이더 슬롯에 한 번에 바인딩
    Context.PSSetShaderResources(0u, static_cast<UINT>(EMaterialTextureSlot::Count), SRVs);
}
