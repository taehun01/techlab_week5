#pragma once

#include "FMaterial.h"
#include "FMesh.h"
#include "FRenderPipeline.h"
#include "FRenderResourceLibrary.h"
#include "Runtime/Core/IntTypes.h"
#include "Runtime/Core/PointerTypes.h"
#include "Runtime/Core/TMap.h"
#include "Runtime/Math/FVector2.h"
#include "Runtime/Rendering/FLineBatcher.h"
#include "ShaderConstants.h"
#include "Vertices.h"
#include "Runtime/Core/FStatRegistry.h"

#include <Windows.h>
#include <d3d11.h>
#include <filesystem>
#include <wrl/client.h>
#include <WICTextureLoader.h>

class FTexture;
struct FTextureDesc;
struct FCamera;
class UTextInstanceComponent;
struct FPreviewRenderTarget;
class UStaticMesh;

inline FWString GetExecutableDirectory() {
    wchar_t Buffer[MAX_PATH * 4];
    GetModuleFileNameW(nullptr, Buffer, MAX_PATH * 4);
    return std::filesystem::path(Buffer).parent_path();
}

inline std::filesystem::path GetResourcesDirectory()
{
    const std::filesystem::path ExeDir(GetExecutableDirectory());
    const std::filesystem::path Candidates[] = {
        ExeDir / L"Resources",
        ExeDir.parent_path().parent_path().parent_path() / L"Resources"
    };

    for (const auto& Candidate : Candidates)
    {
        std::error_code Error;
        if (std::filesystem::is_directory(Candidate, Error))
        {
            return Candidate;
        }
    }
    return {};
}

#include "Runtime/Engine/ShowFlags.h"

class FRenderer final {
public:
    bool Initialize(HWND Window);
    void Shutdown();
    void BeginFrame();
    void BindEditorViewportRenderTargets();
    void SetViewportUV(FVector2 TopLeftUV, FVector2 LengthUV);
    void ClearDepth();
    void SwapBuffer();
    void OnWindowSize(UINT Width, UINT Height);

    EViewModeIndex GetRenderMode() const { return CurrentRenderMode; }
    void SetRenderMode(EViewModeIndex InMode) { CurrentRenderMode = InMode; }

    [[nodiscard]]
    TSharedPtr<FStaticMesh> CreateMesh(const FMeshDesc& Desc);
    [[nodiscard]]
    TSharedPtr<FStaticMesh> CreateDynamicMesh(const FMeshDesc& Desc);
    [[nodiscard]]
    TSharedPtr<FMaterial> CreateMaterial(const FMaterialDesc& Desc);

    void GetDeviceAndContext_ImplDX11(ID3D11Device*& DeviceOut,
        ID3D11DeviceContext*& ContextOut);
    [[nodiscard]] ID3D11Device* GetDevice() const { return Device.Get(); }
    [[nodiscard]] ID3D11DeviceContext* GetContext() const {
        return Context.Get();
    }

    [[nodiscard]]
    TSharedPtr<FRenderPipeline>
        CreateRenderPipeline(const FRenderPipelineDesc& Desc,
            EViewModeIndex RenderMode = EViewModeIndex::VMI_Lit);
    [[nodiscard]]
    TSharedPtr<FTexture> CreateTexture(const wchar_t* path);
    [[nodiscard]]
    TSharedPtr<FRenderPipeline> GetPipeline(const FName& Id) const;

    FLineBatcher& GetLineBatcher() { return LineBatcher; }

    void UpdateLightConstants(const FLightConstants& Constants, const EViewModeIndex InMode);

    void AddTextInstanceArray(const TArray<FInstanceData>& Instances, const FName& MeshId, const FName& MaterialId);
    void DrawInstances(const FCamera& Camera);
    void DrawTextInstances(const FCamera& Camera, const FName& MeshId, const FName& MaterialId);
    void ClearTextInstances();

    void RenderOutline(FVector2 TopLeftUV, FVector2 LengthUV);
    void BindBackBufferWithDepth();
    ID3D11RenderTargetView* GetBackBuffer() { return BackBufferRTV.Get(); }

    void RenderMeshPreviewScene(FPreviewRenderTarget& RenderTarget, const FCamera& Camera, UStaticMesh* TargetMesh, uint32 Width, uint32 Height, bool bDrawGrid = false, TSharedPtr<FMaterial> OverrideMaterial=nullptr);
    void RenderMaterialPreviewScene(FPreviewRenderTarget& RenderTarget, const FCamera& Camera, TSharedPtr<FStaticMesh> Meshasset,  TSharedPtr<FMaterial> Material, uint32 Width, uint32 Height, bool bDrawGrid = false);
private:
    bool InitializeDeviceAndSwapChain(HWND Window);
    bool InitializeBackBufferAndDepthStencil();
    bool InitializeConstantBuffers();
    bool InitializeTextureLoader();

private:
    FLineBatcher LineBatcher;
    Microsoft::WRL::ComPtr<ID3D11Device> Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> Context;
    Microsoft::WRL::ComPtr<IDXGISwapChain> SwapChain;
    D3D11_VIEWPORT Viewport{};

    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> BackBufferRTV;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> DepthStencilBuffer;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> DepthStencilView;

    static constexpr UINT ConstantBufferSize = 256u;
    Microsoft::WRL::ComPtr<ID3D11Buffer> b0ConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> FrameConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> LightConstantBuffer;

    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> EditorViewPortRTV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> EditorViewPortSRV;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> renderTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> DepthStencilSRV;

    bool InitializeEditorViewportRenderTarget();

    Microsoft::WRL::ComPtr<ID3D11Buffer> InstanceBuffer;
    UINT TextInstanceBufferSize = 0;

    EViewModeIndex CurrentRenderMode = EViewModeIndex::VMI_Lit;

public:
    template <typename TConstants>
    void FlushLineBatch(
        const TConstants& Constants,
        const FName& PipelineId = FName("Simple_Line")
    ) {
        UpdateBuffer(Constants);
        LineBatcher.Flush(*Context.Get(), GetPipeline(PipelineId));
    }

    // bApplyViewMode=false면 뷰모드(와이어프레임) 오버라이드를 건너뛴다
    template <typename TConstants>
    void Draw(
        const FStaticMesh& Mesh,
        const FMaterial& Material,
        const TConstants& Constants,
        int32 startidx = 0,
        int32 indicesCount = -1,
        uint32 Slot = 0,
        bool bApplyViewMode = true
    )
    {
        TConstants LocalConstants = Constants;
        LocalConstants.MaterialDiffuse = Material.GetDiffuseColor();
        UpdateBuffer(LocalConstants, Slot);

        TSharedPtr<FRenderPipeline> Pipeline = Material.Pipeline;
        if (bApplyViewMode && CurrentRenderMode == EViewModeIndex::VMI_Wireframe) {
            Pipeline = GetPipeline(FName("Simple_Wireframe"));
        }
        if (Pipeline) {
            Pipeline->Bind(*Context.Get());
        }

        Material.BindResources(*Context.Get());
        Mesh.BindResources(*Context.Get());

        STATS.UpdateDrawCallCount(Mesh.GetIndexCount(), Mesh.GetVertexCount());

        // 외부에서 indicesCount를 양수로 지정한 경우 해당 섹션 범위만 1회 드로우
        if (indicesCount > 0)
        {
            Context->DrawIndexed(static_cast<UINT>(indicesCount), static_cast<UINT>(startidx), 0);
            return;
        }

        // 전체를 그리도록 요청받은 경우 (-1)
        if (Mesh.HasIndices())
        {
            Context->DrawIndexed(Mesh.GetIndexCount(), 0, 0);
        }
        else
        {
            Context->Draw(Mesh.VertexCount, static_cast<UINT>(startidx));
        }
    }

private:
    template <typename TConstants>
    void UpdateBuffer(const TConstants& Constants, uint32 Slot = 0u) {
        static_assert(sizeof(TConstants) <= ConstantBufferSize);
        static_assert(sizeof(TConstants) % 16 == 0);

        TConstants ShaderConstants = Constants;
        if constexpr (requires { ShaderConstants.MVP; }) {
            static const FMatrix UnrealClipToD3DClip{
                FVector{ 0.0f, 0.0f, 1.0f }, FVector{ 1.0f, 0.0f, 0.0f },
                FVector{ 0.0f, 1.0f, 0.0f }, FVector{ 0.0f, 0.0f, 0.0f } };
            ShaderConstants.MVP *= UnrealClipToD3DClip;
        }

        if constexpr (requires { ShaderConstants.VP; }) {
            static const FMatrix UnrealClipToD3DClip{
                FVector{ 0.0f, 0.0f, 1.0f }, FVector{ 1.0f, 0.0f, 0.0f },
                FVector{ 0.0f, 1.0f, 0.0f }, FVector{ 0.0f, 0.0f, 0.0f } };
            ShaderConstants.VP *= UnrealClipToD3DClip;
        }

        D3D11_MAPPED_SUBRESOURCE Mapped{};
        if (FAILED(Context->Map(b0ConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD,
            0, &Mapped))) {
            return;
        }
        std::memcpy(Mapped.pData, &ShaderConstants, sizeof(TConstants));
        Context->Unmap(b0ConstantBuffer.Get(), 0);

        Context->VSSetConstantBuffers(Slot, 1u, b0ConstantBuffer.GetAddressOf());
        Context->PSSetConstantBuffers(Slot, 1u, b0ConstantBuffer.GetAddressOf());
    }
};
