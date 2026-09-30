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
    virtual void AppendRenderDatas(const FCamera& Camera, TArray<FRenderData>& OutDatas, uint32 LodLevel = 0u ) {
        TArray<FRenderData> Datas = GetRenderDatas(Camera);
        OutDatas.insert(OutDatas.end(), std::make_move_iterator(Datas.begin()), std::make_move_iterator(Datas.end()));
    }
    // 렌더 큐 수집용 경량 경로: FRenderData를 만들지 않고 섹션별 메시·머티리얼·인덱스 범위만 OutItems에 덧붙인다.
    // Material이 nullptr인 항목은 호출 측이 "Simple"로 대체하고, PrimitiveIndex는 호출 측이 채운다.
    // 지원하지 않는 컴포넌트는 false를 반환하고 호출 측은 AppendRenderDatas를 쓴다.
    virtual bool AppendDrawItems(uint32 LodLevel, TArray<FDrawItem>& OutItems, FVector2& OutUVOffset) const {
        return false;
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

    void SetLastLod(uint8 Lod) { LastLod = Lod; }
    uint8 GetLastLod() { return LastLod; }
    virtual FStaticMesh* GetFStaticMesh() const { return nullptr; }

    // 영구 오브젝트 상수 슬롯 (FObjectConstantStore). 씬에 등록돼 있는 동안만 유효하다.
    // 상수가 카메라와 무관한 컴포넌트만 쓴다 (카메라를 향하는 빌보드·텍스트는 매 프레임 올리는 기존 경로).
    virtual bool UsesPersistentConstants() const { return false; }
    [[nodiscard]] uint32 GetPersistentConstantSlot() const { return PersistentConstantSlot; }
    // 영구 상수에 넣을 UV 오프셋 (병렬 수집 단계에서 읽기 전용으로 호출된다)
    [[nodiscard]] virtual FVector2 GetRenderUVOffset() const { return FVector2{ 0.0f, 0.0f }; }

    // 렌더 수집 단계의 드로우 항목 캐시 (AppendDrawItems 경량 경로 전용, FRenderView가 채운다).
    // 머티리얼 바인딩 에포크·LOD·메시가 그대로면 AppendDrawItems와 정렬 키 계산을 건너뛰고 재사용한다.
    [[nodiscard]] bool IsDrawItemCacheValid(uint32 Epoch, uint32 Lod, const FStaticMesh* Mesh) const
    {
        return DrawItemCacheEpoch == Epoch && DrawItemCacheLod == Lod && DrawItemCacheMesh == Mesh;
    }
    // 캐시를 비우고 새 조건으로 표시한 뒤, 채울 배열을 돌려준다
    TArray<FCachedDrawItem>& ResetDrawItemCache(uint32 Epoch, uint32 Lod, const FStaticMesh* Mesh)
    {
        CachedDrawItems.clear();
        DrawItemCacheEpoch = Epoch;
        DrawItemCacheLod = Lod;
        DrawItemCacheMesh = Mesh;
        return CachedDrawItems;
    }
    [[nodiscard]] const TArray<FCachedDrawItem>& GetCachedDrawItems() const { return CachedDrawItems; }

protected:
    UMeshComponent() = default;

    // 이 컴포넌트의 머티리얼·메시 설정이 바뀌면 호출한다 (에포크는 1부터 시작하므로 0은 무효 표시)
    void InvalidateDrawItemCache() { DrawItemCacheEpoch = 0u; }

    uint8 LastLod = 0u;
    uint32 PersistentConstantSlot = ~0u; // FObjectConstantStore::InvalidSlot

    TArray<FCachedDrawItem> CachedDrawItems;
    uint32 DrawItemCacheEpoch = 0u;
    uint32 DrawItemCacheLod = 0u;
    const FStaticMesh* DrawItemCacheMesh = nullptr;

    TArray<FRenderData> RenderDatas;
};
