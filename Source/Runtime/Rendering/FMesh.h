#pragma once

#include "Vertices.h"
#include "Runtime/Math/FVector.h"
#include "Runtime/Core/IntTypes.h"
#include "Runtime/Core/FName.h"
#include "Runtime/Core/FString.h"
#include <d3d11.h>
#include <wrl/client.h>
#include "Runtime/Core/TArray.h"
#include "Runtime/Geometry/FAxisAlignedBoundingBox.h"

class FRenderer;

// 인덱스 버퍼 구간 분할 및 렌더링 정보
struct FMeshSection
{
	uint32 FirstIndex = 0;
	uint32 IndexCount = 0;

	float Opacity = 1.0f;
	bool bIsAlpha = false;
	int32 IlluminationModel = 0;
	FString MaterialName;

	FAxisAlignedBoundingBox LocalBounds;
};

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

	// 식별자 및 텍스처 접근자
	[[nodiscard]] const FName& GetMeshId() const { return MeshId; }
	[[nodiscard]] const FName& GetDefaultTextureId() const { return DefaultTextureId; }
	[[nodiscard]] const FName& GetDefaultNormalTextureId() const { return DefaultNormalTextureId; }
	[[nodiscard]] const FName& GetDefaultSpecularTextureId() const { return DefaultSpecularTextureId; }
	[[nodiscard]] const FString& GetPathFileName() const { return PathFileName; }
	[[nodiscard]] bool HasTexture() const { return !DefaultTextureId.empty() && DefaultTextureId != "None"; }
	[[nodiscard]] bool HasNormalMap() const { return !DefaultNormalTextureId.IsNone() && DefaultNormalTextureId != FName("None"); }
	[[nodiscard]] bool HasSpecularMap() const { return !DefaultSpecularTextureId.IsNone() && DefaultSpecularTextureId != FName("None"); }
	[[nodiscard]] uint32 GetVertexBufferSize() { return VertexBufferSize; }
	[[nodiscard]] uint32 GetIndexBufferSize() { return IndexBufferSize; }

	// 버퍼 데이터 갱신
	bool UpdateBuffers(ID3D11Device* Device, ID3D11DeviceContext* Context, const struct FMeshDesc& Desc);

	// 공개 에셋 속성
	FName MeshId{ "None" };
	FString DefaultTextureId{ "None" };
	FName DefaultNormalTextureId{ "None" };
	FName DefaultSpecularTextureId{ "None" };

	FString PathFileName;
	TArray<FMeshSection> Sections;
private:
	void BindResources(ID3D11DeviceContext& Context) const;

	Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer;
	uint32 VertexCount = 0u;
	uint32 VertexStride = 0u;
	uint32 VertexBufferSize = 0u;

	Microsoft::WRL::ComPtr<ID3D11Buffer> IndexBuffer;
	uint32 IndexCount = 0u;
	uint32 IndexBufferSize = 0u;

	TArray<FVector> Positions;
	TArray<uint32> Indices;

	D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	FAxisAlignedBoundingBox LocalBounds{};
};

struct FMeshDesc
{
	const void* VertexData = nullptr;
	uint32 VertexDataSize = 0u;
	uint32 VertexStride = sizeof(FVertexData);
	uint32 VertexCount = 0u;

	const void* IndexData = nullptr;
	uint32 IndexDataSize = 0u;
	uint32 IndexCount = 0u;

	bool bIsLine = false;
};
