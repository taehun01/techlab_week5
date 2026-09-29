#pragma once

#include "UPrimitiveComponent.h"

class UMeshComponent : public UPrimitiveComponent {
    GENERATED_BODY()
    DECLARE_UCLASS(UMeshComponent, UPrimitiveComponent)

public:
    void Initialize() override;
    void Register(UScene& InScene) override;
    void Unregister() override;

    // MeshId로 등록된 메시의 로컬 바운드 (빌보드, 텍스트 등 UStaticMesh가 없는 컴포넌트용)
    FAxisAlignedBoundingBox CalcLocalBounds() override;

    // FRenderData 조회
    virtual TArray<FRenderData> GetRenderDatas(const FCamera& Camera) {
        return RenderDatas;
    }

    // OutDatas 뒤에 이번 프레임의 FRenderData를 덧붙인다.
    // 렌더러는 매 프레임 같은 배열을 재사용해 호출하므로, 오버라이드하면 오브젝트마다 배열 할당이 생기지 않는다.
    // 기본 구현은 GetRenderDatas 결과를 옮겨 담는다.
    virtual void AppendRenderDatas(const FCamera& Camera, TArray<FRenderData>& OutDatas) {
        TArray<FRenderData> Datas = GetRenderDatas(Camera);
        OutDatas.insert(OutDatas.end(), std::make_move_iterator(Datas.begin()), std::make_move_iterator(Datas.end()));
    }
    virtual const FRenderData& GetPureRenderData() const { 
        return RenderDatas.at(0); 
    }

    // ID 접근자
    void SetMeshID(const FName& InMeshId)           { RenderDatas.at(0).MeshId = InMeshId; }
    void SetMaterialID(const FName& InMaterialId)   { RenderDatas.at(0).MaterialId = InMaterialId; }
    void SetTextureID(const FName& InTextureId)     { RenderDatas.at(0).TextureId = InTextureId; }
    virtual const FName& GetMeshID() const       { return RenderDatas.at(0).MeshId; }
    virtual FName GetMaterialID() const          { return RenderDatas.at(0).MaterialId; }
    const FName& GetTextureID() const            { return RenderDatas.at(0).TextureId; }

    // 텍스처 이름으로 머티리얼 텍스처 교체
    bool SetTextureByName(const FName& InTextureName);

    virtual void Serialize(FArchive& Archive) const override;
    virtual void Deserialize(const FArchive& Archive) override;

protected:
    UMeshComponent() = default;

    TArray<FRenderData> RenderDatas;
};
