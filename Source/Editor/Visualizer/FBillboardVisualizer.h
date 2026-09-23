#pragma once

#include "Editor/Visualizer/IVisualizer.h"


class FBillboardVisualizer : public IVisualizer
{
public:
    void Draw(
        const UMeshComponent& Component,
        FRenderView& RenderView,
        const FCamera& Camera,
        const FVector4& Color
    ) const override;
};
