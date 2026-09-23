#pragma once

#include "UPrimitiveComponent.h"

class UMeshComponent : public UPrimitiveComponent {
    GENERATED_BODY()
    DECLARE_UCLASS(UMeshComponent, UPrimitiveComponent)

public:
    void Initialize() override;
    void Register(UScene& InScene) override;
    void Unregister() override;

    // FRenderData 조회
    virtual TArray<FRenderData> GetRenderDatas(const FCamera& Camera) { 
        return RenderDatas;
    }
    virtual const FRenderData& GetPureRenderData() const { 
        return RenderDatas.at(0); 
    }

    // ID 접근자
    void SetMeshID(const FName& InMeshId)           { RenderDatas.at(0).MeshId = InMeshId; }
    void SetMaterialID(const FName& InMaterialId)   { RenderDatas.at(0).MaterialId = InMaterialId; }
    void SetTextureID(const FName& InTextureId)     { RenderDatas.at(0).TextureId = InTextureId; }
    virtual const FName& GetMeshID() const       { return RenderDatas.at(0).MeshId; }
    virtual const FName& GetMaterialID() const   { return RenderDatas.at(0).MaterialId; }
    const FName& GetTextureID() const            { return RenderDatas.at(0).TextureId; }

    // 텍스처 이름으로 머티리얼 텍스처 교체
    bool SetTextureByName(const FName& InTextureName);

    virtual void Serialize(FArchive& Archive) const override;
    virtual void Deserialize(const FArchive& Archive) override;

protected:
    UMeshComponent() = default;

    TArray<FRenderData> RenderDatas;


};
