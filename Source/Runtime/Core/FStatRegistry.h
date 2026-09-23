#pragma once
#include "Source/Runtime/CoreUObject/UObject.h"
#include "Source/Runtime/CoreUObject/FUObjectArray.h"
#include "Source/Runtime/Engine/FTimeManager.h"
#include "Runtime/Rendering/FRenderResourceLibrary.h"

#include <Psapi.h>

#define STATS FStatRegistry::Get()

struct FStatRegistry
{
private:
	uint32 OpaqueQNum = 0;
	uint32 TranslucentQNum = 0;
	uint32 TextQNum = 0;
	uint32 InstancingQNum = 0;
	uint32 StaticMeshByte = 0;
	uint32 DrawIndexCount = 0;
	uint32 DrawVertexCount = 0;
	FVector2 WindowSize = {};

	uint32 StaticIndexBufferSize = 0;
	uint32 StaticVertexBufferSize = 0;
	uint32 DrawCallNum = 0;
	uint32 LineBatchNum = 0;

	// static counter
	uint32 TextureByte = 0;
	uint32 EditorTextureByte = 0;
	uint32 MeshThumbnailByte = 0;
	uint32 FStaticMeshCount = 0;
	uint32 UStaticMeshCount = 0;
	uint32 MaterialCount = 0;
	uint32 TextureCount = 0;
	uint32 EditorTextureCount = 0;
	uint32 MeshThumbnailCount = 0;
	uint32 FontCount = 0;

	PROCESS_MEMORY_COUNTERS Pmc{};

	bool bShowFps = false;
	bool bShowMemory = false;

	FStatRegistry() = default;
	~FStatRegistry() = default;
public:
	FStatRegistry(const FStatRegistry& Other) = delete;
	FStatRegistry& operator= (const FStatRegistry& Other) = delete;

	static FStatRegistry& Get()
	{
		static FStatRegistry Instance;
		return Instance;
	}

	void Initialize() // init static counter
	{
		TextureByte = 0;
		for (auto& It : FRenderResourceLibrary::Get().AllTextureMap)
		{
			// nullptr 체크 추가
			if (It.second)
			{
				TextureByte += static_cast<uint32>(It.second->GetWidth() * It.second->GetHeight() * 1 * 1.333f);
			}
		}

		EditorTextureByte = 0;
		for (auto& It : FRenderResourceLibrary::Get().AllEditorTextureMap)
		{
			if (It.second)
			{
				EditorTextureByte += static_cast<uint32>(It.second->GetWidth() * It.second->GetHeight() * 1 * 1.333f);
			}
		}

		MeshThumbnailByte = 0;
		for (auto& It : FRenderResourceLibrary::Get().AllMeshThumbnailMap)
		{
			if (It.second)
			{
				MeshThumbnailByte += static_cast<uint32>(It.second->GetWidth() * It.second->GetHeight() * 1 * 1.333f);
			}
		}
		FStaticMeshCount = static_cast<uint32>(FRenderResourceLibrary::Get().AllFStaticMeshMap.size());
		UStaticMeshCount = static_cast<uint32>(FRenderResourceLibrary::Get().AllUStaticMeshMap.size());
		MaterialCount = static_cast<uint32>(FRenderResourceLibrary::Get().AllMaterialMap.size());
		TextureCount = static_cast<uint32>(FRenderResourceLibrary::Get().AllTextureMap.size());
		EditorTextureCount = static_cast<uint32>(FRenderResourceLibrary::Get().AllEditorTextureMap.size());
		MeshThumbnailCount = static_cast<uint32>(FRenderResourceLibrary::Get().AllMeshThumbnailMap.size());
		FontCount = static_cast<uint32>(FRenderResourceLibrary::Get().AllFontMap.size());
	}

	void Reset()
	{
		DrawIndexCount = 0;
		DrawVertexCount = 0;
		DrawCallNum = 0;
		LineBatchNum = 0;
	}
	bool IsStatFps() { return bShowFps; }
	void OnStatFPS() { bShowFps = true; }
	void OffStatFPS() { bShowFps = false; }
	bool IsStatMemory() { return bShowMemory; }
	void OnStatMemory() { bShowMemory = true; }
	void OffStatMemory() { bShowMemory = false; }

	uint32 GetAllocationBytes(){ return static_cast<uint32>(UObject::GetTotalAllocationBytes()); }
	uint32 GetAllocationCount(){ return static_cast<uint32>(UObject::GetTotalAllocationCount()); }
	uint32 GetUObjectArrayNum(){ return FUObjectArray::Get().GetNumObjects(); }
	LARGE_INTEGER GetFrequency(){ return FTimeManager::Get().GetFrequency(); }
	float GetDeltaTime(){ return FTimeManager::Get().GetDeltaTime(); }
	float GetTargetFPS() { return FTimeManager::Get().GetTargetFPS(); }
	void SetTargetFPS(float InTargetFPS) { FTimeManager::Get().SetTargetFPS(InTargetFPS); }
	void UpdateRenderQueueNum(size_t InOpaqueNum, size_t InTranslucentNum, size_t InTextNum, size_t InInstancingNum) {
		OpaqueQNum = static_cast<uint32>(InOpaqueNum);
		TranslucentQNum = static_cast<uint32>(InTranslucentNum);
		TextQNum = static_cast<uint32>(InTextNum);
		InstancingQNum = static_cast<uint32>(InInstancingNum);
	}
	uint32 GetOpaqueQNum() { return OpaqueQNum; }
	uint32 GetTranslucentQNum() { return TranslucentQNum; }
	uint32 GetTextQNum() { return TextQNum; }
	uint32 GetInstancingQNum() { return InstancingQNum; }
	void AddStaticMeshByte(uint32 InIndexBufferSize, uint32 InVertexBufferSize) {
		StaticIndexBufferSize += InIndexBufferSize;
		StaticVertexBufferSize += InVertexBufferSize;
		StaticMeshByte += InIndexBufferSize + InVertexBufferSize;
	}
	uint32 GetStaticMeshMemory() { return StaticMeshByte; }
	uint32 GetStaticIndexBufferSize() { return StaticIndexBufferSize; }
	uint32 GetStaticVertexBufferSize() { return StaticVertexBufferSize; }

	uint32 GetPipelineNum() { return static_cast<uint32>(FRenderResourceLibrary::Get().AllPipelineMap.size()); }
	void UpdateDrawCallCount(uint32 InIndexCount, uint32 InVertexCount) { 
		DrawCallNum++; 
		DrawIndexCount += InIndexCount;
		DrawVertexCount += InVertexCount;
	}
	uint32 GetDrawCallNum() { return DrawCallNum; }
	uint32 GetDrawTriangleCount() { return DrawIndexCount / 3; }
	uint32 GetDrawVertexCount() { return DrawVertexCount; }

	void UpdateWindowSize(FVector2 InWindowSize) { WindowSize = InWindowSize; }
	FVector2 GetWindowSize() { return WindowSize; }

	uint32 GetInstanceNum() { return static_cast<uint32>(FRenderResourceLibrary::Get().AllInstancingArrayMap.size()); }

	void AddLineBatchNum(uint32 InNum) { LineBatchNum += InNum; }
	uint32 GetLineBatchNum() { return LineBatchNum; }
	uint32 GetLineBatchByte() { return LineBatchNum * sizeof(FVertexData); }

	// real process memory
	uint32 GetProcessMemory() {
		GetProcessMemoryInfo(GetCurrentProcess(), &Pmc, sizeof(Pmc));
		return static_cast<uint32>(Pmc.WorkingSetSize);
	}
	// static counter
	uint32 GetTextureByte() { return TextureByte; }
	uint32 GetEditorTextureByte() { return EditorTextureByte; }
	uint32 GetMeshThumbnailByte() { return MeshThumbnailByte; }
	uint32 GetFStaticMeshCount() { return FStaticMeshCount; }
	uint32 GetUStaticMeshCount() { return UStaticMeshCount; }
	uint32 GetMaterialCount() { return MaterialCount; }
	uint32 GetTextureCount() { return TextureCount; }
	uint32 GetEditorTextureCount() { return EditorTextureCount; }
	uint32 GetMeshThumbnailCount() { return MeshThumbnailCount; }
	uint32 GetFontCount() { return FontCount; }
};