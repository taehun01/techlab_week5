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
#include "Runtime/Rendering/FRenderState.h"

#include <Windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi1_5.h>
#include <filesystem>
#include <wrl/client.h>
#include "ThirdParty/tracy/tracy/Tracy.hpp"
#include "ThirdParty/tracy/tracy/TracyD3D11.hpp"
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

// premake5.lua가 있는 폴더를 프로젝트 루트로 본다. 찾지 못하면 실행 파일 폴더를 쓴다.
inline std::filesystem::path GetProjectRootDirectory()
{
    const std::filesystem::path ExeDir(GetExecutableDirectory());
    for (std::filesystem::path Dir = ExeDir; !Dir.empty(); Dir = Dir.parent_path())
    {
        std::error_code Error;
        if (std::filesystem::exists(Dir / L"premake5.lua", Error))
        {
            return Dir;
        }
        if (Dir == Dir.root_path())
        {
            break;
        }
    }
    return ExeDir;
}

// 씬 파일 기본 폴더(<프로젝트 루트>/Scenes). 없으면 만든다.
inline std::filesystem::path GetScenesDirectory()
{
    const std::filesystem::path Dir = GetProjectRootDirectory() / L"Scenes";
    std::error_code Error;
    std::filesystem::create_directories(Dir, Error);
    return Dir;
}

#include "Runtime/Engine/ShowFlags.h"

class FRenderer final {
public:
    bool Initialize(HWND Window);
    void Shutdown();
    void BeginFrame();
    void BindEditorViewportRenderTargets();
    void SetViewportUV(FVector2 TopLeftUV, FVector2 LengthUV);
    // b1의 ViewProj 설정 (엔진 좌표계 ViewProj를 넘긴다. D3D 클립 변환은 내부에서 곱한다).
    // World를 쓰는 셰이더(ExampleVS, TexturedUnlitVS, InstanceVS, RotationGizmoVS)는 이 값으로 화면 위치를 구한다.
    void SetViewProjection(const FMatrix& ViewProj);
    [[nodiscard]] const FMatrix& GetViewProjection() const { return CurrentViewProjection; }
    void ClearDepth();
    void SwapBuffer();
    void OnWindowSize(UINT Width, UINT Height);

    EViewModeIndex GetRenderMode() const { return CurrentRenderMode; }
    // 뷰 모드 설정. Unlit이면 b1의 ViewDisableShading도 켠다 (오브젝트 상수에는 뷰 모드 값을 두지 않는다).
    void SetRenderMode(EViewModeIndex InMode);

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

    void ResetRenderState() { CurrentRenderState.Reset(); }

    // Tracy GPU 구간 계측용 컨텍스트. TracyD3D11Zone(Renderer.GetGpuProfiler(), "이름")으로 쓴다.
    [[nodiscard]] TracyD3D11Ctx GetGpuProfiler() const { return GpuProfiler; }

    float GetViewportHeight() { return Viewport.Height; }

    // 오브젝트 상수 일괄 업로드: 드로우마다 b0를 Map/Unmap하는 대신
    // 큰 링 버퍼를 한 번만 Map해 Count개 슬롯을 채우고, 드로우 때는 슬롯 오프셋만 바인딩한다.
    // 슬롯은 컴포넌트(프리미티브) 단위라 같은 컴포넌트의 섹션들이 슬롯 하나를 공유한다.
    // 상수 버퍼 오프셋을 지원하지 않으면 false를 반환하므로 호출 측은 기존 Draw 경로를 쓴다.
    bool BeginObjectConstants(uint32 Count);
    void WriteObjectConstants(uint32 SlotIndex, const FObjectConstants& Constants);
    void EndObjectConstants();
    // bPersistent=true면 영구 상수 버퍼(FObjectConstantStore 슬롯), false면 이번 프레임 링 버퍼 슬롯을 바인딩한다
    void DrawWithObjectConstants(uint32 SlotIndex, bool bPersistent, const FStaticMesh& InMesh, const FMaterial& InMaterial,
        int32 startidx, int32 indicesCount);

    [[nodiscard]] bool SupportsConstantBufferOffset() const { return bSupportsConstantBufferOffset; }
    // 영구 상수 버퍼 사용 가능 여부 (상수 버퍼 오프셋 바인딩이 필요하고, 버퍼 생성에 실패한 적이 없어야 한다)
    [[nodiscard]] bool SupportsPersistentObjectConstants() const {
        return bSupportsConstantBufferOffset && !bPersistentObjectConstantsFailed;
    }
    // FObjectConstantStore에서 dirty 슬롯이 있는 청크만 영구 상수 버퍼에 올린다
    void UploadPersistentObjectConstants();

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
    bool bAllowTearing = false;
    D3D11_VIEWPORT Viewport{};

    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> BackBufferRTV;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> DepthStencilBuffer;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> DepthStencilView;

    static constexpr UINT ConstantBufferSize = 256u;
    Microsoft::WRL::ComPtr<ID3D11Buffer> b0ConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> FrameConstantBuffer;
    // b1 내용의 CPU 사본. 뷰포트 크기와 ViewProj를 따로 갱신해도 서로 덮어쓰지 않게 한다.
    FFrameConstants FrameConstants;
    FMatrix CurrentViewProjection = FMatrix::GetIdentity();
    void UploadFrameConstants();
    Microsoft::WRL::ComPtr<ID3D11Buffer> LightConstantBuffer;
    // 머티리얼 없이 그리는 경로(라인)용 b3 기본값 (MaterialDiffuse = 흰색)
    Microsoft::WRL::ComPtr<ID3D11Buffer> DefaultMaterialConstantBuffer;

    // 오브젝트 상수 링 버퍼. 슬롯 하나는 256바이트(상수 16개)로, VSSetConstantBuffers1 오프셋 단위와 같다.
    static constexpr UINT ObjectConstantSlotSize = 256u;
    static_assert(sizeof(FObjectConstants) <= ObjectConstantSlotSize);
    Microsoft::WRL::ComPtr<ID3D11DeviceContext1> Context1;
    Microsoft::WRL::ComPtr<ID3D11Buffer> ObjectConstantRing;
    uint32 ObjectConstantRingCapacity = 0u;
    uint8* MappedObjectConstants = nullptr;
    bool bSupportsConstantBufferOffset = false;

    // 영구 오브젝트 상수 버퍼. 64KB(256슬롯) 청크 여러 개로 나눈다:
    //  - 64KB는 D3D11 상수 버퍼 기본 한도라 어디서나 만들 수 있다 (더 큰 DEFAULT 상수 버퍼는 11.1 선택 기능)
    //  - dirty 슬롯이 있는 청크만 통째로 UpdateSubresource한다 (상수 버퍼 부분 갱신 기능에 의존하지 않는다)
    // 슬롯 s는 청크 s / 256의 (s % 256)번째 칸이다.
    static constexpr uint32 PersistentSlotsPerChunk = 256u;
    TArray<Microsoft::WRL::ComPtr<ID3D11Buffer>> PersistentObjectConstantChunks;
    // 마지막 청크가 덜 찼을 때 64KB를 채워 올리기 위한 임시 버퍼
    TArray<uint8> PersistentChunkScratch;
    bool bPersistentObjectConstantsFailed = false;

    // 파이프라인·머티리얼·메시를 (바뀐 경우에만) 바인딩하고 드로우한다. b0 바인딩은 호출 측 책임.
    void BindStateAndDraw(const FStaticMesh& InMesh, const FMaterial& InMaterial,
        int32 startidx, int32 indicesCount, bool bApplyViewMode);

    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> EditorViewPortRTV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> EditorViewPortSRV;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> renderTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> DepthStencilSRV;

    bool InitializeEditorViewportRenderTarget();

    Microsoft::WRL::ComPtr<ID3D11Buffer> InstanceBuffer;
    UINT TextInstanceBufferSize = 0;

    EViewModeIndex CurrentRenderMode = EViewModeIndex::VMI_Lit;
    FRenderState CurrentRenderState;
    TracyD3D11Ctx GpuProfiler = nullptr;
public:
    template <typename TConstants>
    void FlushLineBatch(
        const TConstants& Constants,
        const FName& PipelineId = FName("Simple_Line")
    ) {
        UpdateConstantBuffer(Constants);
        BindConstantBuffer();
        // 라인은 머티리얼 없이 그리므로 b3(MaterialDiffuse)에 흰색 기본값을 둔다.
        // ResetRenderState로 머티리얼 캐시를 비우므로 다음 머티리얼 드로우가 b3를 다시 바인딩한다.
        Context->PSSetConstantBuffers(3u, 1u, DefaultMaterialConstantBuffer.GetAddressOf());
        LineBatcher.Flush(*Context.Get(), GetPipeline(PipelineId));
        ResetRenderState();
    }

    // 엔진(언리얼) 클립 좌표계 → D3D 클립 좌표계 변환 행렬.
    // 오브젝트가 많은 경로는 ViewProj에 미리 곱해 두고 Draw에 bConstantsInD3DClip=true를 넘기면
    // 오브젝트마다 하던 4x4 행렬 곱셈을 생략할 수 있다.
    static const FMatrix& GetUnrealClipToD3DClip() {
        static const FMatrix UnrealClipToD3DClip{
            FVector{ 0.0f, 0.0f, 1.0f }, FVector{ 1.0f, 0.0f, 0.0f },
            FVector{ 0.0f, 1.0f, 0.0f }, FVector{ 0.0f, 0.0f, 0.0f } };
        return UnrealClipToD3DClip;
    }

    // bApplyViewMode=false면 뷰모드(와이어프레임) 오버라이드를 건너뛴다
    // bConstantsInD3DClip=true면 Constants의 MVP가 이미 D3D 클립 좌표계라 변환하지 않는다
    template <typename TConstants>
    void Draw(
        const FStaticMesh& InMesh,
        const FMaterial& InMaterial,
        const TConstants& Constants,
        int32 startidx = 0,
        int32 indicesCount = -1,
        uint32 Slot = 0,
        bool bApplyViewMode = true,
        bool bConstantsInD3DClip = false
    )
    {
        // 머티리얼 색(MaterialDiffuse)은 BindStateAndDraw의 머티리얼 바인딩이 b3로 넘긴다
        UpdateConstantBuffer(Constants, bConstantsInD3DClip);
        if (Slot != 0u)
        {
            // b0 이외 슬롯 요청은 캐시 대상이 아니므로 매번 바인딩
            BindConstantBuffer(Slot);
        }
        else if (!CurrentRenderState.bIsConstantBufferBind)
        {
            BindConstantBuffer();
            CurrentRenderState.bIsConstantBufferBind = true;
        }

        BindStateAndDraw(InMesh, InMaterial, startidx, indicesCount, bApplyViewMode);
    }

private:
    void BindConstantBuffer(uint32 Slot = 0u) {
        Context->VSSetConstantBuffers(Slot, 1u, b0ConstantBuffer.GetAddressOf());
        Context->PSSetConstantBuffers(Slot, 1u, b0ConstantBuffer.GetAddressOf());
    }

    template <typename TConstants>
    void UpdateConstantBuffer(const TConstants& Constants, bool bAlreadyInD3DClip = false) {
        static_assert(sizeof(TConstants) <= ConstantBufferSize);
        static_assert(sizeof(TConstants) % 16 == 0);

        TConstants ShaderConstants = Constants;
        if (!bAlreadyInD3DClip) {
            if constexpr (requires { ShaderConstants.MVP; }) {
                ShaderConstants.MVP *= GetUnrealClipToD3DClip();
            }

            if constexpr (requires { ShaderConstants.VP; }) {
                ShaderConstants.VP *= GetUnrealClipToD3DClip();
            }
        }

        D3D11_MAPPED_SUBRESOURCE Mapped{};
        if (FAILED(Context->Map(b0ConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD,
            0, &Mapped))) {
            return;
        }
        std::memcpy(Mapped.pData, &ShaderConstants, sizeof(TConstants));
        Context->Unmap(b0ConstantBuffer.Get(), 0);
    }
};
