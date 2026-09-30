#pragma once

#include "Vertices.h"
#include "Runtime/Math/FVector.h"
#include "Runtime/Core/IntTypes.h"
#include "Runtime/Core/FName.h"
#include "Runtime/Core/FString.h"
#include <d3d11.h>
#include <wrl/client.h>
#include "Runtime/Core/TArray.h"
#include "Runtime/Core/PointerTypes.h"
#include "Runtime/Geometry/FAxisAlignedBoundingBox.h"
#include "Runtime/Rendering/FLodSettings.h"

// 각 인덱스가 어느 LOD에 속하는지 
struct FMeshLODRange
{
	uint32 FirstIndex = 0u;
	uint32 IndexCount = 0u;
};

// 인덱스 버퍼 구간 분할 및 렌더링 정보
struct FMeshSection
{
	// LOD0
	uint32 FirstIndex = 0u;
	uint32 IndexCount = 0u;

	float Opacity = 1.0f;
	bool bIsAlpha = false;
	int32 IlluminationModel = 0;
	FString MaterialName;

	FAxisAlignedBoundingBox LocalBounds;

	// LOD1~
	FMeshLODRange Lods[MAX_MESH_LOD];
	float LodErrors[MAX_MESH_LOD] = {};
	uint32 LodCount = 1u;

};

class FRenderer;
class FStaticMeshBVH;

// 정적 메시 클래스
class FStaticMesh final
{
	friend class FRenderer;

public:
	[[nodiscard]] bool HasIndices() const { return IndexCount > 0; }
	[[nodiscard]] uint32 GetVertexCount() const { return VertexCount; }
	[[nodiscard]] uint32 GetIndexCount() const { return IndexCount; }
	[[nodiscard]] const TArray<FVector>& GetPositions() const { return Positions; }
	[[nodiscard]] const TArray<uint32>& GetIndices() const { return Indices; }
	[[nodiscard]] const FAxisAlignedBoundingBox& GetLocalBounds() const { return LocalBounds; }
	[[nodiscard]] const TArray<FMeshSection>& GetSections() const { return Sections; }
	[[nodiscard]] bool HasSections() const { return !Sections.empty(); }

	// 삼각형 레이캐스트용 BVH. 처음 요청할 때 빌드하고 UpdateBuffers로 정점이 바뀌면 다시 빌드한다.
	[[nodiscard]] const FStaticMeshBVH& GetBVH() const;

	// 식별자 및 텍스처 접근자
	[[nodiscard]] const FName& GetMeshId() const { return MeshId; }
	[[nodiscard]] const FName& GetDefaultTextureId() const { return DefaultTextureId; }
	[[nodiscard]] const FName& GetDefaultNormalTextureId() const { return DefaultNormalTextureId; }
	[[nodiscard]] const FName& GetDefaultSpecularTextureId() const { return DefaultSpecularTextureId; }
	[[nodiscard]] const FString& GetPathFileName() const { return PathFileName; }
	[[nodiscard]] bool HasTexture() const { return !DefaultTextureId.empty() && DefaultTextureId != "None"; }
	[[nodiscard]] bool HasNormalMap() const { return !DefaultNormalTextureId.IsNone() && DefaultNormalTextureId != FName("None"); }
	[[nodiscard]] bool HasSpecularMap() const { return !DefaultSpecularTextureId.IsNone() && DefaultSpecularTextureId != FName("None"); }
	[[nodiscard]] const uint32 GetVertexBufferSize() const { return VertexBufferSize; }
	[[nodiscard]] const uint32 GetIndexBufferSize() const { return IndexBufferSize; }
	[[nodiscard]] const float* GetMeshLodErrors() const { return MeshLodErrors; }
	[[nodiscard]] uint32 GetMeshLodCount() const { return MeshLodCount; }
	void SetMeshLodCount(uint32 InCount) { MeshLodCount = InCount; }


	// 버퍼 데이터 갱신
	bool UpdateBuffers(ID3D11Device* Device, ID3D11DeviceContext* Context, const struct FMeshDesc& Desc);

	void BuildMeshLodSummary();

	// 공개 에셋 속성
	FName MeshId{ "None" };
	FString DefaultTextureId{ "None" };
	FName DefaultNormalTextureId{ "None" };
	FName DefaultSpecularTextureId{ "None" };

	FString PathFileName;
	TArray<FMeshSection> Sections;
	uint16 SortID = 0u;

	float LodSwitchDistance[MAX_MESH_LOD] = {};
private:
	void BindResources(ID3D11DeviceContext& Context) const;

	Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer;
	uint32 VertexCount = 0u;
	uint32 VertexStride = 0u;
	uint32 VertexBufferSize = 0u;

	Microsoft::WRL::ComPtr<ID3D11Buffer> IndexBuffer;
	uint32 IndexBufferSize = 0u;
	// 정적 메시는 정점이 65536개 이하면 16비트 인덱스로 올려 인덱스 대역폭을 절반으로 줄인다.
	// CPU 측 Indices는 항상 32비트다.
	DXGI_FORMAT IndexFormat = DXGI_FORMAT_R32_UINT;

	// LOD0 인덱스 카운트
	uint32 IndexCount = 0u;
	// LOD 포함 전체 인덱스 카운드 
	//uint32 TotalIndexCount = 0u;

	TArray<FVector> Positions;
	TArray<uint32> Indices;

	D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	FAxisAlignedBoundingBox LocalBounds{};

	float MeshLodErrors[MAX_MESH_LOD] = {};
	uint32 MeshLodCount = 1u;

	mutable TSharedPtr<FStaticMeshBVH> BVH;
};

struct FMeshDesc
{
	const void* VertexData = nullptr;
	uint32 VertexDataSize = 0u;
	uint32 VertexStride = sizeof(FVertexData);
	uint32 VertexCount = 0u;

	const void* IndexData = nullptr;
	uint32 IndexDataSize = 0u;
	uint32 IndexCount = 0u; // LOD index까지 모두 포함된 인덱스 카운트
	uint32 CpuIndexCount = 0u; // LOD 제외한 원본 인덱스 카운트. picking이나 mesh에서 cpu 계산 시 사용

	bool bIsLine = false;
};
