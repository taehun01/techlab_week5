#pragma once

#include "Source/ThirdParty/Imgui/imgui.h"
#include "Source/Editor/Core/FEditor.h"
#include "Source/Runtime/Core/FStatRegistry.h"
#include "FImguiStatHelper.h"


struct FImguiStatMemory final
{
	inline static FStatHistory UObjectArrayNum;
	inline static FStatHistory DrawCallNum;
	inline static FStatHistory DrawTriangleCount;
	inline static FStatHistory DrawVertexCount;
	inline static FStatHistory LineBatchNum;
	inline static FStatHistory OpaqueQNum;
	inline static FStatHistory TranslucentQNum;
	inline static FStatHistory TextQNum;
	inline static FStatHistory InstancingQNum;
	inline static FStatHistory InstanceNum;

	inline static uint32 TextureMemoryPool;
	inline static uint32 ProcessMemory;
	inline static uint32 UObjectMemory;
	inline static uint32 StaticMeshMemory;
	inline static uint32 StaticIndexBufferSize;
	inline static uint32 StaticVertexBufferSize;
	inline static uint32 LineBatchMemory;
	inline static uint32 TextureMemory;
	inline static uint32 EditorTextureMemory;
	inline static uint32 ThumbnailTextureMemory;

	const void Process(const FEditor InEditor) const
	{
		const ImGuiViewport* VP = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(60.f, 80.f));
		ImGui::SetNextWindowBgAlpha(0.f);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);  // 창 테두리 제거
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6, 1));

		// 표 배경색 (반투명)
		ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, IM_COL32(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, IM_COL32(0, 0, 0, 0)); // 헤더 밑선, 바깥 테두리
		ImGui::PushStyleColor(ImGuiCol_TableBorderLight, IM_COL32(0, 0, 0, 0)); // 안쪽 행·열 구분선
		ImGui::PushStyleColor(ImGuiCol_TableRowBg, IM_COL32(10, 10, 10, 210));
		ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, IM_COL32(5, 5, 5, 210));
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255)); // 초록 글씨

		ImGuiWindowFlags Flags = ImGuiWindowFlags_NoDecoration
			| ImGuiWindowFlags_AlwaysAutoResize
			| ImGuiWindowFlags_NoBringToFrontOnFocus
			| ImGuiWindowFlags_NoSavedSettings
			| ImGuiWindowFlags_NoFocusOnAppearing
			| ImGuiWindowFlags_NoNav
			| ImGuiWindowFlags_NoMove
			| ImGuiWindowFlags_NoInputs; 

		const uint32 ColumnCount = 6u;
		ImGui::Begin("Stat Memory", nullptr, Flags);

		TArray<FMemoryStatRow> MemoryList;
		MemoryList.clear();

		MemoryList.push_back({ FName("Texture Memory Pool [Texture]"), EStatPool::Texture,  ToCapacity(EStatPool::Texture),    &TextureMemoryPool });
		MemoryList.push_back({ FName("Process Working Set"),      EStatPool::Physical,     STATS.GetProcessMemory(),          &ProcessMemory });
		MemoryList.push_back({ FName("UObject Memory"),           EStatPool::Physical, STATS.GetAllocationBytes(),        &UObjectMemory });
		MemoryList.push_back({ FName("StaticMesh Total Memory"),  EStatPool::Physical, STATS.GetStaticMeshMemory(),       &StaticMeshMemory });
		MemoryList.push_back({ FName("StaticMesh Index Buffer"),  EStatPool::Physical, STATS.GetStaticIndexBufferSize(),  &StaticIndexBufferSize });
		MemoryList.push_back({ FName("StaticMesh Vertex Buffer"), EStatPool::Physical, STATS.GetStaticVertexBufferSize(), &StaticVertexBufferSize });
		MemoryList.push_back({ FName("Line Batch Total Memory"),  EStatPool::Physical, STATS.GetLineBatchByte(),          &LineBatchMemory });
		MemoryList.push_back({ FName("Texture Memory"),           EStatPool::Texture,  STATS.GetTextureByte(),            &TextureMemory });
		MemoryList.push_back({ FName("Editor Texture Memory"),    EStatPool::Texture,  STATS.GetEditorTextureByte(),      &EditorTextureMemory });
		MemoryList.push_back({ FName("Thumbnail Texture Memory"), EStatPool::Texture,  STATS.GetMeshThumbnailByte(),      &ThumbnailTextureMemory });

		if (ImGui::BeginTable("Memory counters", ColumnCount, ImGuiTableFlags_RowBg | ImGuiTableFlags_NoHostExtendX))
		{
			ImGui::TableSetupColumn("Memory Counters", ImGuiTableColumnFlags_WidthFixed, 350.f);
			ImGui::TableSetupColumn("Current", ImGuiTableColumnFlags_WidthFixed, 100.f);
			ImGui::TableSetupColumn("UsedMax", ImGuiTableColumnFlags_WidthFixed, 100.f);
			ImGui::TableSetupColumn("Mem%", ImGuiTableColumnFlags_WidthFixed, 100.f);
			ImGui::TableSetupColumn("MemPool", ImGuiTableColumnFlags_WidthFixed, 100.f);
			ImGui::TableSetupColumn("Pool Capacity", ImGuiTableColumnFlags_WidthFixed, 100.f);
			ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 60, 0, 255));
			ImGui::TableHeadersRow();
			ImGui::PopStyleColor();

			std::sort(MemoryList.begin(), MemoryList.end(), [](const FMemoryStatRow& A, const FMemoryStatRow& B) { return *A.PeakByte > *B.PeakByte; });

			for (uint32 Row = 0; Row < MemoryList.size(); ++Row)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn(); ImGui::Text("%s", MemoryList[Row].RowName.ToString().c_str());
				ImGui::TableNextColumn(); ImGui::Text("%.2f MB", ByteToMB(MemoryList[Row].UsedByte));
				ImGui::TableNextColumn(); ImGui::Text("%.2f MB", ByteToMB(MemoryList[Row].UpdateUsedGetPeak(MemoryList[Row].UsedByte)));
				if (MemoryList[Row].Pool == EStatPool::Texture){
					ImGui::TableNextColumn(); ImGui::Text("%.1f%%", 100.f * static_cast<float>(*MemoryList[Row].PeakByte) / static_cast<float>(ToCapacity(MemoryList[Row].Pool)));
					ImGui::TableNextColumn(); ImGui::Text("%s", ToString(MemoryList[Row].Pool));
					ImGui::TableNextColumn(); ImGui::Text("%.0f MB", ByteToMB(ToCapacity(MemoryList[Row].Pool)));
				}
				else {
					ImGui::TableNextColumn(); ImGui::Text("");
					ImGui::TableNextColumn(); ImGui::Text("%s", ToString(MemoryList[Row].Pool));
					ImGui::TableNextColumn(); ImGui::Text("");
				}
			}
			ImGui::EndTable();
		}

		TArray<FRenderCountStatRow> RenderCountList;
		RenderCountList.clear();
		RenderCountList.push_back({ FName("Live UObjects"),      &UObjectArrayNum,   STATS.GetUObjectArrayNum() });
		RenderCountList.push_back({ FName("Draw Calls"),         &DrawCallNum,       STATS.GetDrawCallNum() });
		RenderCountList.push_back({ FName("Triangles Rendered"), &DrawTriangleCount, STATS.GetDrawTriangleCount() });
		RenderCountList.push_back({ FName("Vertices Rendered"),  &DrawVertexCount,   STATS.GetDrawVertexCount() });
		RenderCountList.push_back({ FName("Lines Rendered"),     &LineBatchNum,      STATS.GetLineBatchNum() });
		RenderCountList.push_back({ FName("Opaque Queue"),       &OpaqueQNum,        STATS.GetOpaqueQNum() });
		RenderCountList.push_back({ FName("Translucent Queue"),  &TranslucentQNum,   STATS.GetTranslucentQNum() });
		RenderCountList.push_back({ FName("Text Queue"),         &TextQNum,          STATS.GetTextQNum() });
		RenderCountList.push_back({ FName("Instancing Queue"),   &InstancingQNum,    STATS.GetInstancingQNum() });
		RenderCountList.push_back({ FName("Total Instance"),     &InstanceNum,       STATS.GetInstanceNum() });

		//std::sort(RenderCountList.begin(), RenderCountList.end(), [](const FRenderCountStatRow& A, const FRenderCountStatRow& B) { return A.Count > B.Count; });

		if (ImGui::BeginTable("Render Counters", ColumnCount, ImGuiTableFlags_RowBg | ImGuiTableFlags_NoHostExtendX))
		{
			ImGui::TableSetupColumn("Render Counters", ImGuiTableColumnFlags_WidthFixed, 350.f);
			ImGui::TableSetupColumn("Current", ImGuiTableColumnFlags_WidthFixed, 100.f);
			ImGui::TableSetupColumn("Average", ImGuiTableColumnFlags_WidthFixed, 100.f);
			ImGui::TableSetupColumn("Max", ImGuiTableColumnFlags_WidthFixed, 100.f);
			ImGui::TableSetupColumn("Min", ImGuiTableColumnFlags_WidthFixed, 100.f);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 100.f);
			ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 60, 0, 255));
			ImGui::TableHeadersRow();
			ImGui::PopStyleColor();

			for (uint32 Row = 0; Row < RenderCountList.size(); Row++)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn(); ImGui::Text("%s", RenderCountList[Row].RowName.ToString().c_str());
				ImGui::TableNextColumn(); ImGui::Text("%u", RenderCountList[Row].History->Update(RenderCountList[Row].Count));
				ImGui::TableNextColumn(); ImGui::Text("%.1f", RenderCountList[Row].History->GetAverage());
				ImGui::TableNextColumn(); ImGui::Text("%u", RenderCountList[Row].History->GetMax());
				ImGui::TableNextColumn(); ImGui::Text("%u", RenderCountList[Row].History->GetMax());
			}
			ImGui::EndTable();
		}

		TArray<FStaticCountStatRow> StaticCountList;
		StaticCountList.clear();
		StaticCountList.push_back({ FName("Pipeline Count"),       STATS.GetPipelineNum() });
		StaticCountList.push_back({ FName("FStaticMesh Count"),    STATS.GetFStaticMeshCount() });
		//StaticCountList.push_back({ FName("UStaticMesh Count"),    STATS.GetUStaticMeshCount() });
		StaticCountList.push_back({ FName("Material Count"),       STATS.GetMaterialCount() });
		StaticCountList.push_back({ FName("Texture Count"),        STATS.GetTextureCount() });
		//StaticCountList.push_back({ FName("Editor Texture Count"), STATS.GetEditorTextureCount() });
		//StaticCountList.push_back({ FName("Mesh Thumbnail Count"), STATS.GetMeshThumbnailCount() });
		StaticCountList.push_back({ FName("Font Count"),           STATS.GetFontCount() });

		if (ImGui::BeginTable("Static Counters", ColumnCount, ImGuiTableFlags_RowBg | ImGuiTableFlags_NoHostExtendX))
		{
			ImGui::TableSetupColumn("Static Counters", ImGuiTableColumnFlags_WidthFixed, 350.f);
			ImGui::TableSetupColumn("Counts", ImGuiTableColumnFlags_WidthFixed, 100.f);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 100.f);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 100.f);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 100.f);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 100.f);
			ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 60, 0, 255));
			ImGui::TableHeadersRow();
			ImGui::PopStyleColor();

			for (uint32 Row = 0; Row < StaticCountList.size(); Row++)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn(); ImGui::Text("%s", StaticCountList[Row].RowName.ToString().c_str());
				ImGui::TableNextColumn(); ImGui::Text("%u", StaticCountList[Row].Count);
			}
			ImGui::EndTable();
		}
		ImGui::PopStyleColor(6);
		ImGui::PopStyleVar(3);
		ImGui::End();
	}

	static float ByteToMB(uint32 Byte) 
	{
		return static_cast<float>(Byte) / (1024.f * 1024.f);
	}
	static float ByteToKB(uint32 Byte)
	{
		return static_cast<float>(Byte) / (1024.f);
	}
};