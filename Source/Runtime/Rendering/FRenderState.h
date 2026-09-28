#pragma once

#include "Runtime/Rendering/FRenderPipeline.h"
#include "Runtime/Core/PointerTypes.h"
#include "Runtime/Rendering/FMaterial.h"
#include "Runtime/Rendering/FMesh.h"

struct FRenderState
{
	FRenderPipeline* Pipeline = nullptr;
	const FMaterial* Material = nullptr;
	const FStaticMesh* Mesh = nullptr;
	bool bIsConstantBufferBind = false;
	UINT StencilRef = 0u;

	void Reset()
	{
		Pipeline = nullptr;
		Material = nullptr;
		Mesh = nullptr;
		bIsConstantBufferBind = false;
		StencilRef = 0u;
	}
};