#pragma once
#include "Source/Editor/Core/FEditor.h"

class FImguiStatPicking final
{
public:
	FImguiStatPicking() = default;

	FImguiStatPicking(const FImguiStatPicking&) = delete;
	FImguiStatPicking& operator=(const FImguiStatPicking&) = delete;



	const void Process(FEditor& InEditor) const;


};

