#pragma once

#include "Editor/Visualizer/IVisualizer.h"

class FTextVisualizer : public IVisualizer
{
public:
    void Draw(
        const UMeshComponent& Component,
        FRenderView& RenderView,
        const FCamera& Camera,
        const FVector4& Color
    ) const override;
};
