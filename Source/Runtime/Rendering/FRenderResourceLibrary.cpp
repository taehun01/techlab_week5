#include "FRenderResourceLibrary.h"
#include "Resources/MasterYi/MasterYi_HeadData.h"
#include "Vertices.h"


#include "FObjDecoder.h"
#include "FRenderer.h"
#include "FTexture.h"
#include "Runtime/Core/TArray.h"
#include "Runtime/Core/Log.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "Runtime/CoreUObject/UStaticMesh.h"
#include "Runtime/Engine/FRenderView.h"
#include "Runtime/Engine/FCamera.h"
#include "Runtime/Rendering/FPreviewRenderTarget.h"
#include "Runtime/Geometry/Sphere.h"
#include "Runtime/Math/FVector.h"
#include "Runtime/Rendering/FRenderer.h"
#include "Runtime/Core/FStatRegistry.h"
#include "Runtime/Core/Log.h"
#include <cmath>
#include <d3dcompiler.h>
#include <numbers>




#define STB_IMAGE_IMPLEMENTATION

#include "ThirdParty/stb/stb_image.h"

FRenderResourceLibrary& FRenderResourceLibrary::Get() {
    static FRenderResourceLibrary Instance;
    return Instance;
}

// 파이프라인 정보 엔트리
struct FPipelineEntry {
    FName Id;
    const wchar_t* VertexShader;
    const wchar_t* PixelShader;
    bool bDepthWrite = true;
    D3D11_CULL_MODE CullMode = D3D11_CULL_BACK;
    EBlendMode BlendMode = EBlendMode::Opaque;
    bool bIsInstancing = false;
};

// 기본 파이프라인 테이블
const FPipelineEntry pipelineTable[] = {
    {
        .Id = FName("Simple_Solid"),
        .VertexShader = L"ExampleVS.cso",
        .PixelShader = L"ExamplePS.cso",
        .BlendMode = EBlendMode::Opaque,
    },
    {
        .Id = FName("Simple_Line"),
        .VertexShader = L"ExampleVS.cso",
        .PixelShader = L"ExamplePS.cso",
        .BlendMode = EBlendMode::Opaque,
    },
    {
        .Id = FName("Grid"),
        .VertexShader = L"GridVS.cso",
        .PixelShader = L"GridPS.cso",
        .bDepthWrite = false,
        .CullMode = D3D11_CULL_NONE,
        .BlendMode = EBlendMode::Translucent,
    },
    {
        .Id = FName("Textured"),
        .VertexShader = L"ExampleVS.cso",
        .PixelShader = L"TexturedPS.cso",
        .BlendMode = EBlendMode::Opaque,
    },
    {
        .Id = FName("Billboard"),
        .VertexShader = L"ExampleVS.cso",
        .PixelShader = L"TexturedPS.cso",
        .bDepthWrite = true,
        //.CullMode = D3D11_CULL_NONE,
        .BlendMode = EBlendMode::Translucent,
    },
    {
        .Id = FName("RotationGizmo"),
        .VertexShader = L"RotationGizmoVS.cso",
        .PixelShader = L"RotationGizmoPS.cso",
        .BlendMode = EBlendMode::Opaque,
    },
    {
        .Id = FName("Spotlight"),
        .VertexShader = L"ExampleVS.cso",
        .PixelShader = L"SpotlightPS.cso",
        .bDepthWrite = false,
        .CullMode = D3D11_CULL_NONE,
        .BlendMode = EBlendMode::Additive,
    },
    {
        .Id = FName("Text"),
        .VertexShader = L"ExampleVS.cso",
        .PixelShader = L"MsdfTextPS.cso",
        .BlendMode = EBlendMode::Translucent,
    },
    {
        .Id = FName("Instance_Text"),
        .VertexShader = L"InstanceVS.cso",
        .PixelShader = L"MsdfTextPS.cso",
        .BlendMode = EBlendMode::Translucent,
        .bIsInstancing = true,
    },
    {
        .Id = FName("Instance_Simple"),
        .VertexShader = L"InstanceVS.cso",
        .PixelShader = L"ExamplePS.cso",
        .BlendMode = EBlendMode::Opaque,
        .bIsInstancing = true,
    },
    {
        .Id = FName("Instance_Textured"),
        .VertexShader = L"InstanceVS.cso",
        .PixelShader = L"TexturedPS.cso",
        .CullMode = D3D11_CULL_NONE,
        .BlendMode = EBlendMode::Translucent,
        .bIsInstancing = true,
    },
    {
        .Id = FName("Gizmo"),
        .VertexShader = L"ExampleVS.cso",
        .PixelShader = L"ExamplePS.cso",
        .BlendMode = EBlendMode::Opaque,
    },
    {
        .Id = FName("SelectedActor_Text"),
        .VertexShader = L"InstanceVS.cso",
        .PixelShader = L"MsdfTextPS.cso",
        .bDepthWrite = false,
        .BlendMode = EBlendMode::Translucent,
        .bIsInstancing = true,
    },
    {
        .Id = FName("Texture_Translucent"),
        .VertexShader = L"ExampleVS.cso",
        .PixelShader = L"TexturedPS.cso",
        .BlendMode = EBlendMode::Translucent,
    },
};

// 머티리얼 정보 엔트리
struct FMaterialEntry {
    FName Id;
    FName PipelineID;
    const char* TextureName = nullptr;
    const char* NormalTextureName = nullptr;
    const char* SpecularTextureName = nullptr;
};

// 기본 머티리얼 테이블
const FMaterialEntry materialTable[] = {
    {
        .Id = FName("Simple"),
        .PipelineID = FName("Simple_Solid"),
    },
    {
        .Id = FName("RotGizmo"),
        .PipelineID = FName("RotationGizmo"),
    },
    {
        .Id = FName("Spotlight"),
        .PipelineID = FName("Spotlight"),
    },
    {
        .Id = FName("Text"),
        .PipelineID = FName("Text"),
        .TextureName = "maplestorybold",
    },
    {
        .Id = FName("Textured"),
        .PipelineID = FName("Textured"),
        .TextureName = "texture",
    },
    {
        .Id = FName("Billboard"),
        .PipelineID = FName("Billboard"),
        .TextureName = "texture",
    },
    {
        .Id = FName("Instance_Text_Maple"),
        .PipelineID = FName("Instance_Text"),
        .TextureName = "maplestorybold",
    },
    {
        .Id = FName("Instance_Simple"),
        .PipelineID = FName("Instance_Simple"),
    },
    {
        .Id = FName("Instance_Textured"),
        .PipelineID = FName("Instance_Textured"),
        .TextureName = "masteryi_head",
    },
    {
        .Id = FName("Gizmo"),
        .PipelineID = FName("Gizmo"),
    },
    {
        .Id = FName("Outline"),
        .PipelineID = FName("Outline"),
    },
    {
        .Id = FName("SelectedActor_Text"),
        .PipelineID = FName("SelectedActor_Text"),
        .TextureName = "maplestorybold",
    },
        {
        .Id = FName("MasterYi"),
        .PipelineID = FName("Textured"),
        .TextureName = "masteryi_head",
    },
};

bool FRenderResourceLibrary::CreateSolidWireframePipeline() {
    if (!RendererRef) return false;
    FRenderer& Renderer = *RendererRef;

    const FWString Path = GetExecutableDirectory();
    const FWString VsPath = Path + L"/Shader/ExampleVS.cso";
    const FWString PsPath = Path + L"/Shader/ExamplePS.cso";

    if (!std::filesystem::exists(VsPath) || !std::filesystem::exists(PsPath)) {
        return false;
    }

    FRenderPipelineDesc Desc = {
        .VertexShaderFileName = VsPath,
        .PixelShaderFileName = PsPath,
        .bEnableDepthTest = true,
    };

    // 솔리드 파이프라인 생성 및 등록
    TSharedPtr<FRenderPipeline> SolidPipeline =
        Renderer.CreateRenderPipeline(Desc, EViewModeIndex::VMI_Lit);
    if (SolidPipeline) {
        AllPipelineMap[FName("Simple_Solid")] = SolidPipeline;
    }

    // 와이어프레임 파이프라인 생성 및 등록
    TSharedPtr<FRenderPipeline> WireframePipeline =
        Renderer.CreateRenderPipeline(Desc, EViewModeIndex::VMI_Wireframe);
    if (WireframePipeline) {
        AllPipelineMap[FName("Simple_Wireframe")] = WireframePipeline;
    }

    return SolidPipeline != nullptr && WireframePipeline != nullptr;
}

bool FRenderResourceLibrary::CreateOutlinePipeline() {
    if (!RendererRef) return false;
    FRenderer& Renderer = *RendererRef;

    ID3D11Device* Device = Renderer.GetDevice();
    if (!Device) {
        return false;
    }

    const FWString Path = GetExecutableDirectory();
    const FWString VsPath = Path + L"/Shader/ExampleVS.cso";
    const FWString PsPath = Path + L"/Shader/ExamplePS.cso";

    if (!std::filesystem::exists(VsPath) || !std::filesystem::exists(PsPath)) {
        return false;
    }

    auto Pipeline = std::make_shared<FRenderPipeline>();

    // 버텍스 셰이더 로드 및 생성
    Microsoft::WRL::ComPtr<ID3DBlob> Blob;
    HRESULT Result = D3DReadFileToBlob(VsPath.c_str(), &Blob);
    if (FAILED(Result)) {
        return false;
    }

    Result = Device->CreateVertexShader(Blob->GetBufferPointer(),
        Blob->GetBufferSize(), nullptr,
        &Pipeline->VertexShader);
    if (FAILED(Result)) {
        return false;
    }

    // 입력 레이아웃 생성
    Result = Device->CreateInputLayout(
        FVertexLayouts::Layout, FVertexLayouts::NumElements,
        Blob->GetBufferPointer(), Blob->GetBufferSize(), &Pipeline->InputLayout);
    if (FAILED(Result)) {
        return false;
    }

    // 픽셀 셰이더 로드 및 생성
    Result = D3DReadFileToBlob(PsPath.c_str(), &Blob);
    if (FAILED(Result)) {
        return false;
    }

    Result =
        Device->CreatePixelShader(Blob->GetBufferPointer(), Blob->GetBufferSize(),
            nullptr, &Pipeline->PixelShader);
    if (FAILED(Result)) {
        return false;
    }

    // 래스터라이저 상태 생성
    D3D11_RASTERIZER_DESC RasterizerDesc{
        .FillMode = D3D11_FILL_SOLID,
        .CullMode = D3D11_CULL_NONE,
        .FrontCounterClockwise = false,
    };
    Result = Device->CreateRasterizerState(&RasterizerDesc,
        &Pipeline->RasterizerState);
    if (FAILED(Result)) {
        return false;
    }

    // 스텐실 마스크 기록 설정
    D3D11_DEPTH_STENCIL_DESC DepthStencilDesc{};
    DepthStencilDesc.DepthEnable = FALSE;
    DepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    DepthStencilDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    DepthStencilDesc.StencilEnable = TRUE;
    DepthStencilDesc.StencilReadMask = 0xFF;
    DepthStencilDesc.StencilWriteMask = 0xFF;
    DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    DepthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    DepthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
    DepthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    DepthStencilDesc.BackFace = DepthStencilDesc.FrontFace;
    Result = Device->CreateDepthStencilState(&DepthStencilDesc,
        &Pipeline->DepthStencilState);
    if (FAILED(Result)) {
        return false;
    }

    // 블렌드 상태 생성
    D3D11_BLEND_DESC BlendDesc{};
    BlendDesc.RenderTarget[0].BlendEnable = FALSE;
    BlendDesc.RenderTarget[0].RenderTargetWriteMask = 0;
    Result = Device->CreateBlendState(&BlendDesc, &Pipeline->BlendState);
    if (FAILED(Result)) {
        return false;
    }

    // 샘플러 상태 생성
    D3D11_SAMPLER_DESC SamplerDesc{
        .Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
        .AddressU = D3D11_TEXTURE_ADDRESS_WRAP,
        .AddressV = D3D11_TEXTURE_ADDRESS_WRAP,
        .AddressW = D3D11_TEXTURE_ADDRESS_WRAP,
        .ComparisonFunc = D3D11_COMPARISON_NEVER,
        .MaxLOD = D3D11_FLOAT32_MAX,
    };
    Result = Device->CreateSamplerState(&SamplerDesc, &Pipeline->SamplerState);
    if (FAILED(Result)) {
        return false;
    }

    AllPipelineMap[FName("Outline")] = Pipeline;
    return true;
}

bool FRenderResourceLibrary::CreatePostProcessPipeline() {
    if (!RendererRef) return false;
    FRenderer& Renderer = *RendererRef;

    ID3D11Device* Device = Renderer.GetDevice();
    if (!Device) {
        return false;
    }

    const FWString Path = GetExecutableDirectory();
    const FWString VsPath = Path + L"/Shader/ScreenQuadVS.cso";
    const FWString PsPath = Path + L"/Shader/OutlinePostProcessPS.cso";

    if (!std::filesystem::exists(VsPath) || !std::filesystem::exists(PsPath)) {
        return false;
    }

    auto Pipeline = std::make_shared<FRenderPipeline>();

    // 버텍스 셰이더 로드 및 생성
    Microsoft::WRL::ComPtr<ID3DBlob> Blob;
    HRESULT Result = D3DReadFileToBlob(VsPath.c_str(), &Blob);
    if (FAILED(Result)) {
        return false;
    }

    Result = Device->CreateVertexShader(Blob->GetBufferPointer(),
        Blob->GetBufferSize(), nullptr,
        &Pipeline->VertexShader);
    if (FAILED(Result)) {
        return false;
    }

    // 픽셀 셰이더 로드 및 생성
    Result = D3DReadFileToBlob(PsPath.c_str(), &Blob);
    if (FAILED(Result)) {
        return false;
    }

    Result =
        Device->CreatePixelShader(Blob->GetBufferPointer(), Blob->GetBufferSize(),
            nullptr, &Pipeline->PixelShader);
    if (FAILED(Result)) {
        return false;
    }

    // 래스터라이저 상태 생성
    D3D11_RASTERIZER_DESC RasterizerDesc{
        .FillMode = D3D11_FILL_SOLID,
        .CullMode = D3D11_CULL_NONE,
        .FrontCounterClockwise = false,
    };
    Result = Device->CreateRasterizerState(&RasterizerDesc,
        &Pipeline->RasterizerState);
    if (FAILED(Result)) {
        return false;
    }

    // 깊이 스텐실 상태 생성
    D3D11_DEPTH_STENCIL_DESC DepthStencilDesc{
        .DepthEnable = FALSE,
        .DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO,
        .DepthFunc = D3D11_COMPARISON_ALWAYS,
        .StencilEnable = FALSE,
    };
    Result = Device->CreateDepthStencilState(&DepthStencilDesc,
        &Pipeline->DepthStencilState);
    if (FAILED(Result)) {
        return false;
    }

    // 블렌드 상태 생성
    D3D11_BLEND_DESC BlendDesc{};
    BlendDesc.RenderTarget[0].BlendEnable = FALSE;
    BlendDesc.RenderTarget[0].RenderTargetWriteMask =
        D3D11_COLOR_WRITE_ENABLE_ALL;
    Result = Device->CreateBlendState(&BlendDesc, &Pipeline->BlendState);
    if (FAILED(Result)) {
        return false;
    }

    // 샘플러 상태 생성
    D3D11_SAMPLER_DESC SamplerDesc{
        .Filter = D3D11_FILTER_MIN_MAG_MIP_POINT,
        .AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
        .AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
        .AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
        .ComparisonFunc = D3D11_COMPARISON_NEVER,
        .MaxLOD = D3D11_FLOAT32_MAX,
    };
    Result = Device->CreateSamplerState(&SamplerDesc, &Pipeline->SamplerState);
    if (FAILED(Result)) {
        return false;
    }

    AllPipelineMap[FName("PostProcess")] = Pipeline;
    return true;
}

bool FRenderResourceLibrary::InitializePipelines() {
    if (!RendererRef) return false;
    FRenderer& Renderer = *RendererRef;

    // 솔리드 및 와이어프레임 파이프라인 개별 생성
    const std::filesystem::path ShaderRoot = std::filesystem::path(GetExecutableDirectory()) / L"Shader";
    auto RequireShader = [&](const wchar_t* Name) {
        const auto File = ShaderRoot / Name;
        std::error_code Error;
        if (std::filesystem::is_regular_file(File, Error)) return true;
        UE_LOG_ERROR("[Shader Loader] Required file missing: %s", File.string().c_str());
        return false;
    };
    for (const auto& Entry : pipelineTable) {
        if (!RequireShader(Entry.VertexShader) || !RequireShader(Entry.PixelShader)) return false;
    }
    if (!RequireShader(L"ScreenQuadVS.cso") || !RequireShader(L"OutlinePostProcessPS.cso")) return false;
    if (!CreateSolidWireframePipeline() || !CreateOutlinePipeline() || !CreatePostProcessPipeline()) {
        UE_LOG_ERROR("[Shader Loader] Failed to initialize built-in pipelines.");
        return false;
    }

    const FWString Path = GetExecutableDirectory();

    for (const FPipelineEntry& Entry : pipelineTable) {
        if (AllPipelineMap.find(Entry.Id) != AllPipelineMap.end()) {
            continue;
        }

        const FWString VsPath = Path + L"/Shader/" + Entry.VertexShader;
        const FWString PsPath = Path + L"/Shader/" + Entry.PixelShader;

        if (!std::filesystem::exists(VsPath) || !std::filesystem::exists(PsPath)) {
            UE_LOG_ERROR("[Shader Loader] Required shader missing: %s / %s",
                std::filesystem::path(VsPath).string().c_str(), std::filesystem::path(PsPath).string().c_str());
            return false;
        }

        FRenderPipelineDesc PipelineDesc = {
            .VertexShaderFileName = VsPath,
            .PixelShaderFileName = PsPath,
            .bEnableDepthTest = true,
            .bEnableDepthWrite = Entry.bDepthWrite,
            .CullMode = Entry.CullMode,
            .BlendMode = Entry.BlendMode,
            .bIsInstancing = Entry.bIsInstancing,
        };

        TSharedPtr<FRenderPipeline> Pipeline =
            Renderer.CreateRenderPipeline(PipelineDesc, EViewModeIndex::VMI_Lit);
        if (!Pipeline) {
            UE_LOG_ERROR("[Shader Loader] Pipeline creation failed: %s / %s",
                std::filesystem::path(VsPath).string().c_str(), std::filesystem::path(PsPath).string().c_str());
            return false;
        }

        AllPipelineMap[Entry.Id] = Pipeline;
    }
    return true;
}

bool FRenderResourceLibrary::Initialize(FRenderer& Renderer) {
    RendererRef = &Renderer;
    if (!InitializePipelines() // 파이프라인 먼저 생성
        || !CreateCubeMesh() ||
        !CreateCylinderMesh(1.0f, 24u, 1.0f, 1.0f) ||
        !CreateConeMesh() || !CreateSpotlightConeMesh() ||
        !CreateArrowMesh() || !CreateCircleMesh() ||
        !CreateRotationGizmoMesh() || !CreateSquareArrowMesh() ||
        !CreateGridMesh() || !CreateSphereMesh() ||
        !CreateLineMesh() || !CreatePlaneMesh() ||
        !CreateRectMesh() || !CreateMasterYiMesh() ||
        !CreateTextures() || !InitializeMaterials() ||
        !CreateInstancingArrayMap() || !CreateEditTextures() ||
        !CreateFonts() || !CreateObjMeshes()) {
        return false;
    }

  CreateUStaticMeshMap();
  CreateMeshThumbnails(); // 메시 썸네일 일괄 생성
  CreateMaterialThumbnails(); // 머터리얼 썸네일 일괄 생성


  //머터리얼 변경 후 저장시 발동할 함수
  FRenderResourceLibrary::Get().OnMaterialSaved.Add([](const FString& SavedMatKey)
      {
          FRenderResourceLibrary::Get().RefreshMaterialAndDependentThumbnails(SavedMatKey);
      });

  return true;
}


TSharedPtr<FMaterial> FRenderResourceLibrary::CreateAndRegisterMaterialFromInfo(const FObjMaterialInfo& Info) {
    // 모든 OBJ 머티리얼 이름은 전역적으로 유일하므로 이름을 등록 키로 사용한다.
    FString MaterialKey = Info.MaterialName;
    //std::transform(MaterialKey.begin(), MaterialKey.end(), MaterialKey.begin(), ::tolower);

    if (auto ExistingMtl = GetMaterial(MaterialKey))
    {
        return ExistingMtl;
    }

    auto Material = std::make_shared<FMaterial>();

    // 텍스처 조회
    auto DiffuseTex = (!Info.DiffuseTextureName.empty() && Info.DiffuseTextureName != "None")
        ? GetTexture(FName(Info.DiffuseTextureName)) : nullptr;

    auto NormalTex = (!Info.NormalTextureName.empty() && Info.NormalTextureName != "None")
        ? GetTexture(FName(Info.NormalTextureName)) : nullptr;

    auto SpecularTex = (!Info.SpecularTextureName.empty() && Info.SpecularTextureName != "None")
        ? GetTexture(FName(Info.SpecularTextureName)) : nullptr;

    // 파이프라인 및 텍스처 설정
    FName PipelineName = FName("Simple_Solid");
    if (DiffuseTex)
    {
        const bool bIsTranslucent = (Info.Opacity < 0.99f || !Info.AlphaTextureName.empty() ||
            Info.IlluminationModel == 4 || Info.IlluminationModel == 6 || Info.IlluminationModel == 7);

        PipelineName = bIsTranslucent ? FName("Texture_Translucent") : FName("Textured");
        Material->SetDiffuseMap(DiffuseTex);
    }

    if (NormalTex)
    {
        Material->SetNormalMap(NormalTex);
    }

    if (SpecularTex)
    {
        Material->SetSpecularMap(SpecularTex);
    }

    Material->SetPipeLine(GetPipeline(PipelineName));


    // 라이브러리에 등록 후 반환
    // 여러 메시와 섹션은 이 등록 키를 공유한다.
    return RegisterMaterial(MaterialKey, Material);
}

UStaticMesh* FRenderResourceLibrary::CreateAndRegisterUStaticMesh(FName Key, TArray<FString>&& materials,
    TSharedPtr<FStaticMesh> fstaticmesh)
{
    auto it = AllUStaticMeshMap.find(Key.ToString());
    if (it != AllUStaticMeshMap.end() && it->second != nullptr)
    {
        return it->second;
    }

    UStaticMesh* MeshPtr = NewObject<UStaticMesh>();
    if (!MeshPtr)
    {
        return nullptr;
    }

    MeshPtr->Materials = std::move(materials);

    MeshPtr->MeshId = Key;
    MeshPtr->SetStaticMeshAsset(fstaticmesh);

    for (int i = 0; i < MeshPtr->Materials.size(); i++)
    {
        RegisterMeshMaterialDependency(Key.ToString(), MeshPtr->Materials[i]);
    }


    AllUStaticMeshMap[Key.ToString()] = MeshPtr;

    return MeshPtr;
}

bool FRenderResourceLibrary::CreateUStaticMeshMap() {
    UE_LOG("[UStaticMeshMap] 생성 시작 (등록된 FStaticMesh 개수: %zu)", AllFStaticMeshMap.size());

    for (const auto& [Key, Mesh] : AllFStaticMeshMap) {
        if (!Mesh) continue;

        STATS.AddStaticMeshByte(static_cast<uint32>(Mesh->GetIndices().size() * sizeof(int32)), static_cast<uint32>(Mesh->GetVertexCount() * sizeof(FVertexData)));

        //  이미 OBJ 파싱 단계 등에서 등록된 에셋은 건너뜀
        if (AllUStaticMeshMap.find(Key) != AllUStaticMeshMap.end()) {
            continue;
        }

        // 기본 도형 메시(Cube, Sphere 등) UStaticMesh 생성
        UStaticMesh* StaticMeshObj = NewObject<UStaticMesh>(Key);
        StaticMeshObj->MeshId = Key;
        StaticMeshObj->SetStaticMeshAsset(Mesh);

        // 외부 생성 함수를 호출하지 않고 머티리얼 슬롯만 지정
        if (Key == "Spotlight")
        {
            StaticMeshObj->Materials.push_back("Spotlight");
            RegisterMeshMaterialDependency("Spotlight", "Spotlight");
        }
        else if (Key == "MasterYi")
        {
            StaticMeshObj->Materials.push_back("MasterYi");
            RegisterMeshMaterialDependency("MasterYi", "MasterYi");
        }
        else
        {
            StaticMeshObj->Materials.push_back("Simple_Solid");
            RegisterMeshMaterialDependency(Key, "Simple_Solid");
        }

        AllUStaticMeshMap[Key] = StaticMeshObj;
    }


    auto SphereAsset = GetSphereMesh();
    if (SphereAsset)
    {
        CreateAndRegisterUStaticMesh(FName("Sphere_Mat"), { "Simple" }, SphereAsset);
        RegisterMeshMaterialDependency("Sphere_Mat", "Simple");
    }





    UE_LOG("[UStaticMeshMap] 생성 완료 (총 %zu 개)", AllUStaticMeshMap.size());
    return true;
}

bool FRenderResourceLibrary::CreateCubeMesh() {
    if (!RendererRef) return false;
    FRenderer& Renderer = *RendererRef;

    FMeshDesc MeshDesc{
        .VertexData = CubeVertices,
        .VertexDataSize = static_cast<uint32>(sizeof(CubeVertices)),
        .VertexStride = static_cast<uint32>(sizeof(FVertexData)),
        .VertexCount = static_cast<uint32>(std::size(CubeVertices)),
        .IndexData = CubeIndices,
        .IndexDataSize = static_cast<uint32>(sizeof(CubeIndices)),
        .IndexCount = static_cast<uint32>(std::size(CubeIndices)),
    };

    RegisterMesh(FName("Cube"), Renderer.CreateMesh(MeshDesc));
    return AllFStaticMeshMap["Cube"] != nullptr;
}

bool FRenderResourceLibrary::CreateCylinderMesh(float Height, uint32 SliceCount,
    float TopRadius,
    float BottomRadius) {
    if (!RendererRef) return false;
    FRenderer& Renderer = *RendererRef;

    constexpr float TAU = std::numbers::pi_v<float> *2.0f;
    const float DTheta = TAU / static_cast<float>(SliceCount);

    TArray<FVertexData> Vertices;
    TArray<uint32> Indices;

    Vertices.reserve(SliceCount * 4 + 2);
    Indices.reserve(SliceCount * 12);

    const float HalfH = Height * 0.5f;

    const uint32 TopCenterIndex = static_cast<uint32>(Vertices.size());
    Vertices.push_back({ 0.0f, 0.0f, HalfH, 0.0f, 0.0f, 1.0f, 1.0f, 0.5f, 0.5f,
                        0.0f, 0.0f, 1.0f });

    const uint32 TopRingStart = static_cast<uint32>(Vertices.size());
    for (uint32 i = 0; i < SliceCount; ++i) {
        const float Theta = static_cast<float>(i) * DTheta;
        Vertices.push_back({ TopRadius * std::cos(Theta),
                            TopRadius * std::sin(Theta), HalfH, 0.0f, 0.0f, 1.0f,
                            1.0f, 0.5f + 0.5f * std::cos(Theta),
                            0.5f + 0.5f * std::sin(Theta), 0.0f, 0.0f, 1.0f });
    }

    const uint32 BottomCenterIndex = static_cast<uint32>(Vertices.size());
    Vertices.push_back({ 0.0f, 0.0f, -HalfH, 0.0f, 0.0f, 1.0f, 1.0f, 0.5f, 0.5f,
                        0.0f, 0.0f, -1.0f });

    const uint32 BottomRingStart = static_cast<uint32>(Vertices.size());
    for (uint32 i = 0; i < SliceCount; ++i) {
        const float Theta = static_cast<float>(i) * DTheta;
        Vertices.push_back({ BottomRadius * std::cos(Theta),
                            BottomRadius * std::sin(Theta), -HalfH, 0.0f, 0.0f,
                            1.0f, 1.0f, 0.5f + 0.5f * std::cos(Theta),
                            0.5f + 0.5f * std::sin(Theta), 0.0f, 0.0f, -1.0f });
    }

    const uint32 SideTopStart = static_cast<uint32>(Vertices.size());
    for (uint32 i = 0; i < SliceCount; ++i) {
        const float Theta = static_cast<float>(i) * DTheta;
        Vertices.push_back({ TopRadius * std::cos(Theta),
                            TopRadius * std::sin(Theta), HalfH, 0.0f, 0.0f, 1.0f,
                            1.0f,
                            static_cast<float>(i) / static_cast<float>(SliceCount),
                            0.0f, std::cos(Theta), std::sin(Theta), 0.0f });
    }

    const uint32 SideBottomStart = static_cast<uint32>(Vertices.size());
    for (uint32 i = 0; i < SliceCount; ++i) {
        const float Theta = static_cast<float>(i) * DTheta;
        Vertices.push_back({ BottomRadius * std::cos(Theta),
                            BottomRadius * std::sin(Theta), -HalfH, 0.0f, 0.0f,
                            1.0f, 1.0f,
                            static_cast<float>(i) / static_cast<float>(SliceCount),
                            1.0f, std::cos(Theta), std::sin(Theta), 0.0f });
    }

    for (uint32 i = 0; i < SliceCount; ++i) {
        const uint32 Next = (i + 1) % SliceCount;
        Indices.push_back(TopCenterIndex);
        Indices.push_back(TopRingStart + i);
        Indices.push_back(TopRingStart + Next);
    }

    for (uint32 i = 0; i < SliceCount; ++i) {
        const uint32 Next = (i + 1) % SliceCount;
        Indices.push_back(BottomCenterIndex);
        Indices.push_back(BottomRingStart + Next);
        Indices.push_back(BottomRingStart + i);
    }

    for (uint32 i = 0; i < SliceCount; ++i) {
        const uint32 Next = (i + 1) % SliceCount;

        const uint32 TL = SideTopStart + i;
        const uint32 TR = SideTopStart + Next;
        const uint32 BL = SideBottomStart + i;
        const uint32 BR = SideBottomStart + Next;

        Indices.push_back(BL);
        Indices.push_back(BR);
        Indices.push_back(TL);

        Indices.push_back(BR);
        Indices.push_back(TR);
        Indices.push_back(TL);
    }

    FMeshDesc MeshDesc{
        .VertexData = Vertices.data(),
        .VertexDataSize =
            static_cast<uint32>(sizeof(FVertexData) * Vertices.size()),
        .VertexStride = static_cast<uint32>(sizeof(FVertexData)),
        .VertexCount = static_cast<uint32>(Vertices.size()),
        .IndexData = Indices.data(),
        .IndexDataSize = static_cast<uint32>(sizeof(uint32) * Indices.size()),
        .IndexCount = static_cast<uint32>(Indices.size()),
    };

    RegisterMesh(FName("Cylinder"), Renderer.CreateMesh(MeshDesc));
    return AllFStaticMeshMap["Cylinder"] != nullptr;
}


bool FRenderResourceLibrary::CreateConeMesh() {
    if (!RendererRef) return false;
    FRenderer& Renderer = *RendererRef;

    constexpr float BottomRadius = 0.5f;
    constexpr float Height = 1.0f;
    constexpr uint32 SliceCount = 48;
    constexpr float TAU = std::numbers::pi_v<float> *2.0f;
    const float DTheta = TAU / static_cast<float>(SliceCount);

    TArray<FVertexData> Vertices;
    TArray<uint32> Indices;

    const float HalfH = Height * 0.5f;
    const float SlantLen =
        std::sqrt(Height * Height + BottomRadius * BottomRadius);
    const float NormalFactor = Height / SlantLen;
    const float NormalZ = BottomRadius / SlantLen;

    // 옆면 정점 생성
    for (uint32 i = 0; i < SliceCount; ++i) {
        const float Theta = static_cast<float>(i) * DTheta;
        const float NextTheta = static_cast<float>(i + 1) * DTheta;
        const float MidTheta = (Theta + NextTheta) * 0.5f;

        const float ApexNx = NormalFactor * std::cos(MidTheta);
        const float ApexNy = NormalFactor * std::sin(MidTheta);

        const uint32 ApexIdx = static_cast<uint32>(Vertices.size());
        Vertices.push_back({ 0.0f, 0.0f, HalfH, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.0f,
                            ApexNx, ApexNy, NormalZ });

        const float BaseNx1 = NormalFactor * std::cos(Theta);
        const float BaseNy1 = NormalFactor * std::sin(Theta);
        const uint32 BaseIdx1 = static_cast<uint32>(Vertices.size());
        Vertices.push_back({ BottomRadius * std::cos(Theta),
                            BottomRadius * std::sin(Theta), -HalfH, 1.0f, 1.0f,
                            1.0f, 1.0f,
                            static_cast<float>(i) / static_cast<float>(SliceCount),
                            1.0f, BaseNx1, BaseNy1, NormalZ });

        const float BaseNx2 = NormalFactor * std::cos(NextTheta);
        const float BaseNy2 = NormalFactor * std::sin(NextTheta);
        const uint32 BaseIdx2 = static_cast<uint32>(Vertices.size());
        Vertices.push_back(
            { BottomRadius * std::cos(NextTheta), BottomRadius * std::sin(NextTheta),
             -HalfH, 1.0f, 1.0f, 1.0f, 1.0f,
             static_cast<float>(i + 1) / static_cast<float>(SliceCount), 1.0f,
             BaseNx2, BaseNy2, NormalZ });

        Indices.push_back(ApexIdx);
        Indices.push_back(BaseIdx2);
        Indices.push_back(BaseIdx1);
    }

    // 밑면 뚜껑 정점 생성
    const uint32 BottomCenterIndex = static_cast<uint32>(Vertices.size());
    Vertices.push_back({ 0.0f, 0.0f, -HalfH, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.2f,
                        0.0f, 0.0f, -1.0f });

    const uint32 BottomRingStart = static_cast<uint32>(Vertices.size());
    for (uint32 i = 0; i < SliceCount; ++i) {
        const float Theta = static_cast<float>(i) * DTheta;
        const float U = static_cast<float>(i) / static_cast<float>(SliceCount);
        Vertices.push_back({ BottomRadius * std::cos(Theta),
                            BottomRadius * std::sin(Theta), -HalfH, 1.0f, 1.0f,
                            1.0f, 1.0f, U, 1.0f, 0.0f, 0.0f, -1.0f });
    }

    for (uint32 i = 0; i < SliceCount; ++i) {
        const uint32 Next = (i + 1) % SliceCount;
        Indices.push_back(BottomCenterIndex);
        Indices.push_back(BottomRingStart + Next);
        Indices.push_back(BottomRingStart + i);
    }

    FMeshDesc MeshDesc{
        .VertexData = Vertices.data(),
        .VertexDataSize =
            static_cast<uint32>(sizeof(FVertexData) * Vertices.size()),
        .VertexStride = static_cast<uint32>(sizeof(FVertexData)),
        .VertexCount = static_cast<uint32>(Vertices.size()),
        .IndexData = Indices.data(),
        .IndexDataSize = static_cast<uint32>(sizeof(uint32) * Indices.size()),
        .IndexCount = static_cast<uint32>(Indices.size()),
    };

    RegisterMesh(FName("Cone"), Renderer.CreateMesh(MeshDesc));
    return AllFStaticMeshMap["Cone"] != nullptr;
}

// 스포트라이트 전용 열린 원뿔 메쉬 생성
bool FRenderResourceLibrary::CreateSpotlightConeMesh() {
    if (!RendererRef) return false;
    FRenderer& Renderer = *RendererRef;

    constexpr float BottomRadius = 0.5f;
    constexpr float Height = 1.0f;
    constexpr uint32 SliceCount = 48;
    constexpr float TAU = std::numbers::pi_v<float> *2.0f;
    const float DTheta = TAU / static_cast<float>(SliceCount);

    TArray<FVertexData> Vertices;
    TArray<uint32> Indices;

    const float HalfH = Height * 0.5f;
    const float SlantLen =
        std::sqrt(Height * Height + BottomRadius * BottomRadius);
    const float NormalFactor = Height / SlantLen;
    const float NormalZ = BottomRadius / SlantLen;

    // 옆면 정점만 생성하고 밑면 뚜껑은 생성하지 않음
    for (uint32 i = 0; i < SliceCount; ++i) {
        const float Theta = static_cast<float>(i) * DTheta;
        const float NextTheta = static_cast<float>(i + 1) * DTheta;
        const float MidTheta = (Theta + NextTheta) * 0.5f;

        const float ApexNx = NormalFactor * std::cos(MidTheta);
        const float ApexNy = NormalFactor * std::sin(MidTheta);

        const uint32 ApexIdx = static_cast<uint32>(Vertices.size());
        Vertices.push_back({ 0.0f, 0.0f, HalfH, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.0f,
                            ApexNx, ApexNy, NormalZ });

        const float BaseNx1 = NormalFactor * std::cos(Theta);
        const float BaseNy1 = NormalFactor * std::sin(Theta);
        const uint32 BaseIdx1 = static_cast<uint32>(Vertices.size());
        Vertices.push_back({ BottomRadius * std::cos(Theta),
                            BottomRadius * std::sin(Theta), -HalfH, 1.0f, 1.0f,
                            1.0f, 1.0f,
                            static_cast<float>(i) / static_cast<float>(SliceCount),
                            1.0f, BaseNx1, BaseNy1, NormalZ });

        const float BaseNx2 = NormalFactor * std::cos(NextTheta);
        const float BaseNy2 = NormalFactor * std::sin(NextTheta);
        const uint32 BaseIdx2 = static_cast<uint32>(Vertices.size());
        Vertices.push_back(
            { BottomRadius * std::cos(NextTheta), BottomRadius * std::sin(NextTheta),
             -HalfH, 1.0f, 1.0f, 1.0f, 1.0f,
             static_cast<float>(i + 1) / static_cast<float>(SliceCount), 1.0f,
             BaseNx2, BaseNy2, NormalZ });

        Indices.push_back(ApexIdx);
        Indices.push_back(BaseIdx2);
        Indices.push_back(BaseIdx1);
    }

    FMeshDesc MeshDesc{
        .VertexData = Vertices.data(),
        .VertexDataSize =
            static_cast<uint32>(sizeof(FVertexData) * Vertices.size()),
        .VertexStride = static_cast<uint32>(sizeof(FVertexData)),
        .VertexCount = static_cast<uint32>(Vertices.size()),
        .IndexData = Indices.data(),
        .IndexDataSize = static_cast<uint32>(sizeof(uint32) * Indices.size()),
        .IndexCount = static_cast<uint32>(Indices.size()),
    };

    RegisterMesh(FName("SpotlightCone"), Renderer.CreateMesh(MeshDesc));
    return AllFStaticMeshMap["SpotlightCone"] != nullptr;
}

bool FRenderResourceLibrary::CreateArrowMesh() {
    if (!RendererRef) return false;
    FRenderer& Renderer = *RendererRef;

    constexpr uint32 SliceCount = 16u;
    constexpr float ShaftLength = 0.75f;
    constexpr float ShaftRadius = 0.025f;
    constexpr float HeadRadius = 0.075f;
    constexpr float HeadLength = 0.25f;
    constexpr float DTheta =
        2.0f * std::numbers::pi_v<float> / static_cast<float>(SliceCount);

    TArray<FVertexData> Vertices;
    TArray<uint32> Indices;

    Vertices.reserve(SliceCount * 5 + 3);
    Indices.reserve(SliceCount * 18);

    const uint32 ShaftBottomCenter = static_cast<uint32>(Vertices.size());
    Vertices.push_back({ 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.5f,
                        -1.0f, 0.0f, 0.0f });

    const uint32 ShaftBottomRing = static_cast<uint32>(Vertices.size());
    for (uint32 i = 0; i < SliceCount; ++i) {
        const float Theta = static_cast<float>(i) * DTheta;
        Vertices.push_back({ 0.0f, ShaftRadius * std::cos(Theta),
                            ShaftRadius * std::sin(Theta), 1.0f, 0.0f, 0.0f, 1.0f,
                            0.0f, 0.0f, -1.0f, 0.0f, 0.0f });
    }

    const uint32 ShaftSideBottom = static_cast<uint32>(Vertices.size());
    for (uint32 i = 0; i < SliceCount; ++i) {
        const float Theta = static_cast<float>(i) * DTheta;
        Vertices.push_back({ 0.0f, ShaftRadius * std::cos(Theta),
                            ShaftRadius * std::sin(Theta), 1.0f, 0.0f, 0.0f, 1.0f,
                            0.0f, 0.0f, 0.0f, std::cos(Theta), std::sin(Theta) });
    }

    const uint32 ShaftSideTop = static_cast<uint32>(Vertices.size());
    for (uint32 i = 0; i < SliceCount; ++i) {
        const float Theta = static_cast<float>(i) * DTheta;
        Vertices.push_back({ ShaftLength, ShaftRadius * std::cos(Theta),
                            ShaftRadius * std::sin(Theta), 1.0f, 0.0f, 0.0f, 1.0f,
                            1.0f, 0.0f, 0.0f, std::cos(Theta), std::sin(Theta) });
    }

    const uint32 HeadBaseCenter = static_cast<uint32>(Vertices.size());
    Vertices.push_back({ ShaftLength, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.5f,
                        0.5f, -1.0f, 0.0f, 0.0f });

    const uint32 HeadBaseRing = static_cast<uint32>(Vertices.size());
    for (uint32 i = 0; i < SliceCount; ++i) {
        const float Theta = static_cast<float>(i) * DTheta;
        Vertices.push_back({ ShaftLength, HeadRadius * std::cos(Theta),
                            HeadRadius * std::sin(Theta), 1.0f, 0.0f, 0.0f, 1.0f,
                            0.0f, 0.0f, -1.0f, 0.0f, 0.0f });
    }

    const uint32 HeadSideBase = static_cast<uint32>(Vertices.size());
    for (uint32 i = 0; i < SliceCount; ++i) {
        const float Theta = static_cast<float>(i) * DTheta;
        Vertices.push_back({ ShaftLength, HeadRadius * std::cos(Theta),
                            HeadRadius * std::sin(Theta), 1.0f, 0.0f, 0.0f, 1.0f,
                            0.0f, 0.0f, 0.0f, std::cos(Theta), std::sin(Theta) });
    }

    const uint32 HeadTip = static_cast<uint32>(Vertices.size());
    Vertices.push_back({ ShaftLength + HeadLength, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                        1.0f, 1.0f, 0.5f, 1.0f, 0.0f, 0.0f });

    for (uint32 i = 0; i < SliceCount; ++i) {
        const uint32 Next = (i + 1) % SliceCount;
        Indices.push_back(ShaftBottomCenter);
        Indices.push_back(ShaftBottomRing + Next);
        Indices.push_back(ShaftBottomRing + i);
    }

    for (uint32 i = 0; i < SliceCount; ++i) {
        const uint32 Next = (i + 1) % SliceCount;
        const uint32 BL = ShaftSideBottom + i;
        const uint32 BR = ShaftSideBottom + Next;
        const uint32 TL = ShaftSideTop + i;
        const uint32 TR = ShaftSideTop + Next;

        Indices.push_back(BL);
        Indices.push_back(TL);
        Indices.push_back(BR);

        Indices.push_back(BR);
        Indices.push_back(TL);
        Indices.push_back(TR);
    }

    for (uint32 i = 0; i < SliceCount; ++i) {
        const uint32 Next = (i + 1) % SliceCount;
        Indices.push_back(HeadBaseCenter);
        Indices.push_back(HeadBaseRing + Next);
        Indices.push_back(HeadBaseRing + i);
    }

    for (uint32 i = 0; i < SliceCount; ++i) {
        const uint32 Next = (i + 1) % SliceCount;
        Indices.push_back(HeadSideBase + i);
        Indices.push_back(HeadTip);
        Indices.push_back(HeadSideBase + Next);
    }

    FMeshDesc MeshDesc{
        .VertexData = Vertices.data(),
        .VertexDataSize =
            static_cast<uint32>(sizeof(FVertexData) * Vertices.size()),
        .VertexStride = static_cast<uint32>(sizeof(FVertexData)),
        .VertexCount = static_cast<uint32>(Vertices.size()),
        .IndexData = Indices.data(),
        .IndexDataSize = static_cast<uint32>(sizeof(uint32) * Indices.size()),
        .IndexCount = static_cast<uint32>(Indices.size()),
    };

    RegisterMesh(FName("Arrow"), Renderer.CreateMesh(MeshDesc));
    return AllFStaticMeshMap["Arrow"] != nullptr;
}

bool FRenderResourceLibrary::CreateCircleMesh() {
    if (!RendererRef) return false;
    FRenderer& Renderer = *RendererRef;

    constexpr uint32 SliceCount = 32u;
    constexpr float Radius = 1.0f;
    constexpr float Width = 0.07f;

    TArray<FVertexData> Vertices;
    TArray<uint32> Indices;

    Vertices.reserve(SliceCount * 2);
    Indices.reserve(SliceCount * 6);

    for (uint32 i = 0; i < SliceCount; ++i) {
        float Theta = 2.0f * std::numbers::pi_v<float> *static_cast<float>(i) /
            static_cast<float>(SliceCount);
        float InnerRadius = Radius - Width * 0.5f;
        float OuterRadius = Radius + Width * 0.5f;

        Vertices.push_back({ 0.0f, InnerRadius * std::cos(Theta),
                            InnerRadius * std::sin(Theta), 1.0f, 0.0f, 0.0f, 1.0f,
                            0.0f, 0.0f, 1.0f, 0.0f, 0.0f });
        Vertices.push_back({ 0.0f, OuterRadius * std::cos(Theta),
                            OuterRadius * std::sin(Theta), 1.0f, 0.0f, 0.0f, 1.0f,
                            1.0f, 1.0f, 1.0f, 0.0f, 0.0f });

        uint32 InnerCurrent = 2 * i;
        uint32 OuterCurrent = 2 * i + 1;
        uint32 InnerNext = (2 * (i + 1)) % (SliceCount * 2);
        uint32 OuterNext = (2 * (i + 1) + 1) % (SliceCount * 2);

        Indices.push_back(InnerCurrent);
        Indices.push_back(OuterCurrent);
        Indices.push_back(InnerNext);

        Indices.push_back(InnerNext);
        Indices.push_back(OuterCurrent);
        Indices.push_back(OuterNext);

        Indices.push_back(OuterCurrent);
        Indices.push_back(InnerCurrent);
        Indices.push_back(InnerNext);

        Indices.push_back(OuterCurrent);
        Indices.push_back(InnerNext);
        Indices.push_back(OuterNext);
    }

    const FMeshDesc Desc{
        .VertexData = Vertices.data(),
        .VertexDataSize =
            static_cast<uint32>(sizeof(FVertexData) * Vertices.size()),
        .VertexStride = static_cast<uint32>(sizeof(FVertexData)),
        .VertexCount = static_cast<uint32>(Vertices.size()),
        .IndexData = Indices.data(),
        .IndexDataSize = static_cast<uint32>(sizeof(uint32) * Indices.size()),
        .IndexCount = static_cast<uint32>(Indices.size()),
    };

    RegisterMesh(FName("Circle"), Renderer.CreateMesh(Desc));
    return AllFStaticMeshMap["Circle"] != nullptr;
}

bool FRenderResourceLibrary::CreateRotationGizmoMesh() {
    if (!RendererRef) return false;
    FRenderer& Renderer = *RendererRef;

    constexpr uint32 SliceCount = 32u;
    constexpr float Radius = 1.0f;

    TArray<FVertexData> Vertices;
    TArray<uint32> Indices;

    Vertices.reserve(SliceCount * 2);
    Indices.reserve(SliceCount * 6);

    for (uint32 i = 0; i < SliceCount; ++i) {
        float Theta = 2.0f * std::numbers::pi_v<float> *static_cast<float>(i) /
            static_cast<float>(SliceCount);

        Vertices.push_back({ 0.0f, Radius * std::cos(Theta),
                            Radius * std::sin(Theta), -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                            0.0f, -1.0f, 0.0f, 0.0f });
        Vertices.push_back({ 0.0f, Radius * std::cos(Theta),
                            Radius * std::sin(Theta), 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
                            0.0f, 1.0f, 0.0f, 0.0f });

        if (i == 0)
            continue;

        Indices.push_back(2u * i - 2u);
        Indices.push_back(2u * i + 1u);
        Indices.push_back(2u * i - 1u);

        Indices.push_back(2u * i - 2u);
        Indices.push_back(2u * i);
        Indices.push_back(2u * i + 1u);

        Indices.push_back(2u * i - 2u);
        Indices.push_back(2u * i - 1u);
        Indices.push_back(2u * i + 1u);

        Indices.push_back(2u * i - 2u);
        Indices.push_back(2u * i);
        Indices.push_back(2u * i - 1u);

        Indices.push_back(2u * i - 2u);
        Indices.push_back(2u * i + 1u);
        Indices.push_back(2u * i);

        Indices.push_back(2u * i - 2u);
        Indices.push_back(2u * i);
        Indices.push_back(2u * i + 1u);
    }
    Indices[0] = 2u * SliceCount - 2u;
    Indices[1] = 2u * SliceCount - 1u;
    Indices[3] = 2u * SliceCount - 2u;
    Indices[5] = 2u * SliceCount - 1u;
    Indices[6] = 2u * SliceCount - 2u;
    Indices[9] = 2u * SliceCount - 2u;

    const FMeshDesc Desc{
        .VertexData = Vertices.data(),
        .VertexDataSize =
            static_cast<uint32>(sizeof(FVertexData) * Vertices.size()),
        .VertexStride = static_cast<uint32>(sizeof(FVertexData)),
        .VertexCount = static_cast<uint32>(Vertices.size()),
        .IndexData = Indices.data(),
        .IndexDataSize = static_cast<uint32>(sizeof(uint32) * Indices.size()),
        .IndexCount = static_cast<uint32>(Indices.size()),
    };

    RegisterMesh(FName("RotGizmo"), Renderer.CreateMesh(Desc));
    return AllFStaticMeshMap["RotGizmo"] != nullptr;
}

bool FRenderResourceLibrary::CreateSquareArrowMesh() {
    if (!RendererRef) return false;
    FRenderer& Renderer = *RendererRef;

    constexpr float ShaftLength = 0.85f;
    constexpr float ShaftRadius = 0.025f;
    constexpr float ArrowLength = 1.0f;
    constexpr float TipSize = ArrowLength - ShaftLength;

    TArray<FVertexData> Vertices;
    TArray<uint32> Indices;

    Vertices.reserve(16u);
    Indices.reserve(72u);

    for (const auto& v : ColoredCubeVertices) {
        float ScaledX = v.x * ShaftLength;
        float ScaledY = v.y * ShaftRadius;
        float ScaledZ = v.z * ShaftRadius;
        Vertices.push_back({ ScaledX + ShaftLength * 0.5f, ScaledY, ScaledZ, v.r,
                            v.g, v.b, v.a, v.u, v.v, v.nx, v.ny, v.nz });
    }

    for (const auto& Index : ColoredCubeIndices) {
        Indices.push_back(Index);
    }

    for (const auto& v : ColoredCubeVertices) {
        float ScaledX = v.x * TipSize;
        float ScaledY = v.y * TipSize;
        float ScaledZ = v.z * TipSize;
        Vertices.push_back({ ScaledX + TipSize * 0.5f + ShaftLength, ScaledY,
                            ScaledZ, v.r, v.g, v.b, v.a, v.u, v.v, v.nx, v.ny,
                            v.nz });
    }

    for (const auto& Index : ColoredCubeIndices) {
        Indices.push_back(Index + 8u);
    }

    const FMeshDesc Desc{
        .VertexData = Vertices.data(),
        .VertexDataSize =
            static_cast<uint32>(sizeof(FVertexData) * Vertices.size()),
        .VertexStride = static_cast<uint32>(sizeof(FVertexData)),
        .VertexCount = static_cast<uint32>(Vertices.size()),
        .IndexData = Indices.data(),
        .IndexDataSize = static_cast<uint32>(sizeof(uint32) * Indices.size()),
        .IndexCount = static_cast<uint32>(Indices.size()),
    };

    RegisterMesh(FName("SquareArrow"), Renderer.CreateMesh(Desc));
    return AllFStaticMeshMap["SquareArrow"] != nullptr;
}

bool FRenderResourceLibrary::CreateGridMesh() {
    if (!RendererRef) return false;
    FRenderer& Renderer = *RendererRef;

    constexpr float HalfW = 10.0f;
    constexpr float HalfH = 10.0f;

    const TArray<FVertexData> Vertices = {
        {-HalfW, -HalfH, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
         1.0f},
        {HalfW, -HalfH, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f,
         1.0f},
        {HalfW, HalfH, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f,
         1.0f},
        {-HalfW, HalfH, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f,
         1.0f},
    };
    const TArray<uint32> Indices = { 0, 1, 2, 0, 2, 3 };

    FMeshDesc MeshDesc{
        .VertexData = Vertices.data(),
        .VertexDataSize =
            static_cast<uint32>(sizeof(FVertexData) * Vertices.size()),
        .VertexStride = sizeof(FVertexData),
        .VertexCount = static_cast<uint32>(Vertices.size()),
        .IndexData = Indices.data(),
        .IndexDataSize = static_cast<uint32>(sizeof(uint32) * Indices.size()),
        .IndexCount = static_cast<uint32>(Indices.size()),
    };

    RegisterMesh(FName("Grid"), Renderer.CreateMesh(MeshDesc));
    return AllFStaticMeshMap["Grid"] != nullptr;
}

bool FRenderResourceLibrary::CreateSphereMesh() {
    if (!RendererRef) return false;
    FRenderer& Renderer = *RendererRef;

    auto Vertices = CreateSphereVertices(0.5f, 20, 20, false);

    FMeshDesc MeshDesc{
        .VertexData = Vertices.data(),
        .VertexDataSize =
            static_cast<uint32>(sizeof(FVertexData) * Vertices.size()),
        .VertexStride = sizeof(FVertexData),
        .VertexCount = static_cast<uint32>(Vertices.size()),
    };

    RegisterMesh(FName("Sphere"), Renderer.CreateMesh(MeshDesc));
    return AllFStaticMeshMap["Sphere"] != nullptr;
}



bool FRenderResourceLibrary::CreateLineMesh() {
    if (!RendererRef) return false;
    FRenderer& Renderer = *RendererRef;

    FMeshDesc Desc{ .VertexData = LineVertices,
                   .VertexDataSize = static_cast<uint32>(sizeof(LineVertices)),
                   .VertexStride = sizeof(FVertexData),
                   .VertexCount = static_cast<uint32>(std::size(LineVertices)),
                   .bIsLine = true };

    RegisterMesh(FName("Line"), Renderer.CreateMesh(Desc));
    return AllFStaticMeshMap["Line"] != nullptr;
}

bool FRenderResourceLibrary::CreatePlaneMesh() {
    if (!RendererRef) return false;
    FRenderer& Renderer = *RendererRef;

    FMeshDesc Desc{
        .VertexData = PlaneVertices,
        .VertexDataSize = static_cast<uint32>(sizeof(PlaneVertices)),
        .VertexStride = sizeof(FVertexData),
        .VertexCount = static_cast<uint32>(std::size(PlaneVertices)),
    };

    RegisterMesh(FName("Plane"), Renderer.CreateMesh(Desc));
    return AllFStaticMeshMap["Plane"] != nullptr;
}

bool FRenderResourceLibrary::CreateRectMesh() {
    if (!RendererRef) return false;
    FRenderer& Renderer = *RendererRef;

    // 사각형 정점 배열
    const TArray<FVertexData> Vertices = {
        {0.0f, -0.5f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.5f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.5f, -0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, -0.5f, -0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f,
         0.0f},
    };

    // 양면 인덱스 배열
    const TArray<uint32> Indices = { 0, 1, 2, 0, 2, 3 };

    FMeshDesc MeshDesc{
        .VertexData = Vertices.data(),
        .VertexDataSize =
            static_cast<uint32>(sizeof(FVertexData) * Vertices.size()),
        .VertexStride = sizeof(FVertexData),
        .VertexCount = static_cast<uint32>(Vertices.size()),
        .IndexData = Indices.data(),
        .IndexDataSize = static_cast<uint32>(sizeof(uint32) * Indices.size()),
        .IndexCount = static_cast<uint32>(Indices.size()),
    };

    RegisterMesh(FName("Rect"), Renderer.CreateMesh(MeshDesc));
    return AllFStaticMeshMap["Rect"] != nullptr;
}

bool FRenderResourceLibrary::CreateMasterYiMesh() {
    if (!RendererRef) return false;
    FRenderer& Renderer = *RendererRef;
    FMeshDesc MeshDesc{
        .VertexData = MasterYiHeadVertices,
        .VertexDataSize = static_cast<uint32>(sizeof(MasterYiHeadVertices)),
        .VertexStride = static_cast<uint32>(sizeof(FVertexData)),
        .VertexCount = MasterYiHeadVertexCount,
        .IndexData = MasterYiHeadIndices,
        .IndexDataSize = static_cast<uint32>(sizeof(MasterYiHeadIndices)),
        .IndexCount = MasterYiHeadIndexCount,
    };

    TSharedPtr<FStaticMesh> YiMesh = Renderer.CreateMesh(MeshDesc);
    if (YiMesh) {
        // 기본 텍스처 등록
        YiMesh->DefaultTextureId = "MasterYi_Head";
        RegisterMesh(FName("MasterYi"), YiMesh);
    }
    return AllFStaticMeshMap["MasterYi"] != nullptr;
}

bool FRenderResourceLibrary::CreateInstancingArrayMap() {
    AllInstancingArrayMap.clear();
    // 기본 배치 키 등록
    AllInstancingArrayMap[{FName("Instance_Text"), FName("Rect")}] = {};
    AllInstancingArrayMap[{FName("Instance_Simple"), FName("Cube")}] = {};
    AllInstancingArrayMap[{FName("Instance_Textured"), FName("MasterYi")}] = {};
    AllInstancingArrayMap[{FName("SelectedActor_Text"), FName("Rect")}] = {};

    return true;
}

bool FRenderResourceLibrary::InitializeMaterials() {
    for (const auto& Entry : materialTable) {
        TSharedPtr<FMaterial> Material = std::make_shared<FMaterial>();

        TSharedPtr<FRenderPipeline> Pipeline = GetPipeline(Entry.PipelineID);
        if (Pipeline) {
            Material->SetPipeLine(Pipeline);
        }

        if (Entry.TextureName) {
            Material->SetDiffuseMap(GetTexture(Entry.TextureName));
        }
        if (Entry.NormalTextureName) {
            Material->SetNormalMap(GetTexture(Entry.NormalTextureName));
        }
        if (Entry.SpecularTextureName) {
            Material->SetSpecularMap(GetTexture(Entry.SpecularTextureName));
        }

        RegisterMaterial(Entry.Id.ToString(), Material);
    }
    return true;
}

bool FRenderResourceLibrary::CreateEditTextures() {
    if (!RendererRef) return false;
    FRenderer& Renderer = *RendererRef;

    const std::filesystem::path ExeDir(GetExecutableDirectory());
    const auto ResourcesDir = GetResourcesDirectory();
    TArray<std::filesystem::path> SearchRoots;
    if (!ResourcesDir.empty()) SearchRoots.push_back(ResourcesDir / L"Edit");
    SearchRoots.push_back(ExeDir / L"Edit");

    for (const auto& Root : SearchRoots) {
        std::error_code Ec;
        if (!std::filesystem::exists(Root, Ec)) {
            continue;
        }

        for (const auto& Entry :
            std::filesystem::recursive_directory_iterator(Root, Ec)) {
            if (!Entry.is_regular_file(Ec))
                continue;

            FWString Ext = Entry.path().extension().wstring();
            std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
            if (Ext != L".dds" && Ext != L".jpg" && Ext != L".jpeg")
                continue;

            // 확장자 제거
            FString KeyWide = Entry.path().stem().string();
            std::transform(KeyWide.begin(), KeyWide.end(), KeyWide.begin(),
                ::tolower);

            // 이미 로드된 텍스처 건너뜀
            if (AllEditorTextureMap.find(KeyWide) != AllEditorTextureMap.end()) {
                continue;
            }

            TSharedPtr<FTexture> Texture =
                Renderer.CreateTexture(Entry.path().wstring().c_str());

            if (!Texture)
                continue;

            RegisterEditTexture(KeyWide, Texture);
        }
    }

    return true;
}

TSharedPtr<FMaterial>
FRenderResourceLibrary::RegisterMaterial(const FString& Id, TSharedPtr<FMaterial> inMaterial) {
    if (AllMaterialMap[Id])
        return AllMaterialMap[Id];

    if (inMaterial) {
        inMaterial->MaterialId = Id;
    }
    AllMaterialMap[Id] = inMaterial;
    return inMaterial;
}

bool FRenderResourceLibrary::CreateTextures() {
    if (!RendererRef) return false;
    FRenderer& Renderer = *RendererRef;

    const std::filesystem::path ExeDir(GetExecutableDirectory());
    const auto ResourcesDir = GetResourcesDirectory();
    TArray<std::filesystem::path> SearchRoots;
    if (!ResourcesDir.empty()) SearchRoots.push_back(ResourcesDir / L"Textures");
    SearchRoots.push_back(ExeDir / L"Textures");

    for (const auto& Root : SearchRoots) {
        std::error_code Ec;
        if (!std::filesystem::exists(Root, Ec)) {
            continue;
        }

        for (const auto& Entry :
            std::filesystem::recursive_directory_iterator(Root, Ec)) {
            if (!Entry.is_regular_file(Ec))
                continue;

            FWString Ext = Entry.path().extension().wstring();
            std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
            if (Ext != L".dds" && Ext != L".jpg" && Ext != L".jpeg" && Ext != L".png")
                continue;

            // 확장자 제거
            FString KeyWide = Entry.path().stem().string();
            std::transform(KeyWide.begin(), KeyWide.end(), KeyWide.begin(),
                ::tolower);
            FName TextureKey(KeyWide);

            // 이미 로드된 텍스처 건너뜀
            if (AllTextureMap.find(TextureKey) != AllTextureMap.end()) {
                continue;
            }

            TSharedPtr<FTexture> Texture =
                Renderer.CreateTexture(Entry.path().wstring().c_str());

            if (!Texture)
                continue;

            FString temp = TextureKey.ToString();

            RegisterTexture(temp, Texture);
        }
    }

    return true;
}

// MTL 파일별 Material 캐시와 메시별 OBJ 캐시를 로딩한다.
bool FRenderResourceLibrary::CreateObjMeshes() 
{
    if (!RendererRef)
    { 
        return false;
    }

    // Todo: Make as static constant, move to header
    FRenderer& Renderer = *RendererRef;
    const std::filesystem::path ExeDir(GetExecutableDirectory());
    const auto ResourcesDir = GetResourcesDirectory();
    if (ResourcesDir.empty())
    {
        UE_LOG_WARN("[Asset Loader] Resources directory not found: %s", ExeDir.string().c_str());
        return true;
    }
    const std::filesystem::path AssetRoot = ResourcesDir / L"Assets";
    std::error_code Error;
    if (!std::filesystem::exists(AssetRoot, Error)
        || Error
        || !std::filesystem::is_directory(AssetRoot, Error)
        || Error)
    {
        UE_LOG_WARN("[OBJ Loader] Assets 폴더를 찾을 수 없습니다: %s", AssetRoot.string().c_str());
        return true;
    }
    const std::filesystem::path BinaryDirectory = AssetRoot / L"Bins";
    std::filesystem::create_directories(BinaryDirectory, Error);
    if (Error)
    {
        UE_LOG_WARN("[Asset Loader] Bins 폴더 생성 실패: %s", BinaryDirectory.string().c_str());
    }

    TArray<std::filesystem::path> ObjFiles;
    FObjDecoder Decoder;

    std::filesystem::recursive_directory_iterator Entries(AssetRoot, Error);
    if (Error)
    {
        UE_LOG_WARN("[OBJ Loader] Assets 폴더 탐색 실패: %s", AssetRoot.string().c_str());
        return true;
    }

    for (const auto& Entry : Entries)
    {
        if (Entry.is_regular_file() == false)
        {
            continue;
        }

        const FString FileExtension = Entry.path().extension().string();
        if (FileExtension == OBJ_EXTENSION)
        {
            ObjFiles.push_back(Entry.path());

            continue;
        }

        if (FileExtension == MTL_EXTENSION)
        {
            const FString MaterialFileKey = Entry.path().stem().string() + MTL_EXTENSION;
            if (AllMaterialFileSet.find(MaterialFileKey) != AllMaterialFileSet.end())
            {
                continue;
            }

            const FString BinaryPath = (BinaryDirectory / (MaterialFileKey + BIN_EXTENSION)).string();
            
            TArray<FObjMaterialInfo> Materials;
            if (Decoder.LoadMaterials(Entry.path().string(), BinaryPath, Materials) == false)
            {
                UE_LOG_WARN("[Material Loader] 로딩 실패: %s", Entry.path().string().c_str());

                continue;
            }

            for (const FObjMaterialInfo& Material : Materials)
            {
                // 전역 Material Map에 이미 있으면 재등록하지 않는다.
                if (AllMaterialMap.find(Material.MaterialName) != AllMaterialMap.end())
                {
                    continue;
                }

                CreateAndRegisterMaterialFromInfo(Material);
            }

            AllMaterialFileSet.insert(MaterialFileKey);
        }

    }

    for (const std::filesystem::path& ObjFile : ObjFiles)
    {
        // 파일명을 MeshID(FName)로 사용
        const std::string StemName = ObjFile.stem().string();
        FName MeshKey(StemName);

        if (AllFStaticMeshMap.find(StemName) != AllFStaticMeshMap.end())
        {
            continue;
        }

        const FString ObjPath = std::filesystem::absolute(ObjFile).string();

        // OBJ 캐시는 Resources/Assets/Bins/<메시 이름>.bin으로 저장한다.
        const FString CacheName = StemName + BIN_EXTENSION;
        const FString BinaryPath = (AssetRoot / "Bins" / CacheName).string();

        FObjModelData ModelData;
        if (Decoder.LoadObj(ObjPath, BinaryPath, ModelData) == false)
        {
            UE_LOG_WARN("[OBJ Loader] 로딩 실패: %s", ObjPath.c_str());
            
            continue;
        }

        FMeshDesc Desc
        {
            .VertexData = ModelData.Vertices.data(),
            .VertexDataSize = static_cast<uint32>(sizeof(FVertexData) *
                                                  ModelData.Vertices.size()),
            .VertexStride = static_cast<uint32>(sizeof(FVertexData)),
            .VertexCount = static_cast<uint32>(ModelData.Vertices.size()),

            .IndexData = ModelData.Indices.data(),
            .IndexDataSize =
                static_cast<uint32>(sizeof(uint32) * ModelData.Indices.size()),
            .IndexCount = static_cast<uint32>(ModelData.Indices.size()),
            .bIsLine = false
        };

        TSharedPtr<FStaticMesh> StaticMesh = Renderer.CreateMesh(Desc);
        if (!StaticMesh)
        {
            UE_LOG_WARN("[OBJ Loader] GPU 메시 생성 실패: %s", ObjPath.c_str());
            continue;
        }
        
        StaticMesh->PathFileName = ObjFile.string();
        StaticMesh->MeshId = MeshKey;
        StaticMesh->Sections = std::move(ModelData.Sections);

        RegisterMesh(MeshKey, StaticMesh);
        UE_LOG("[OBJ Loader] 로드 완료: %s (정점: %u, 인덱스: %u, 섹션: %zu)",
            StemName.c_str(), ModelData.Vertices.size(),
            ModelData.Indices.size(), StaticMesh->Sections.size());

        TArray<FString> MaterialStrings;
        MaterialStrings.reserve(StaticMesh->Sections.size());
        for (const auto& Section : StaticMesh->Sections)
        {
            MaterialStrings.push_back(Section.MaterialName);
        }

        CreateAndRegisterUStaticMesh(MeshKey, std::move(MaterialStrings), StaticMesh);
    }

    return true;
}

TSharedPtr<FStaticMesh>
FRenderResourceLibrary::CreateStaticMesh(const FName& ID,
    const TArray<FVertexData>& Vertices,
    const TArray<uint32>& Indices) {
    if (!RendererRef || Vertices.empty()) return nullptr;

    FMeshDesc Desc{
        .VertexData = Vertices.data(),
        .VertexDataSize = static_cast<uint32>(sizeof(FVertexData) * Vertices.size()),
        .VertexStride = static_cast<uint32>(sizeof(FVertexData)),
        .VertexCount = static_cast<uint32>(Vertices.size()),
        .IndexData = Indices.empty() ? nullptr : Indices.data(),
        .IndexDataSize = static_cast<uint32>(sizeof(uint32) * Indices.size()),
        .IndexCount = static_cast<uint32>(Indices.size()),
    };

    TSharedPtr<FStaticMesh> NewMesh = RendererRef->CreateMesh(Desc);
    if (NewMesh) {
        RegisterMesh(ID, NewMesh);
    }
    return NewMesh;
}

TSharedPtr<FStaticMesh>
FRenderResourceLibrary::GetOrCreateMesh(const FName& ID,
    const TArray<FVertexData>& vertices) {
    auto it = AllFStaticMeshMap.find(ID.ToString());
    if (it != AllFStaticMeshMap.end())
        return it->second;

    FMeshDesc Desc{ .VertexData = vertices.data(),
                   .VertexDataSize =
                       static_cast<uint32>(sizeof(FVertexData) * vertices.size()),
                   .VertexStride = static_cast<uint32>(sizeof(FVertexData)),
                   .VertexCount = static_cast<uint32>(vertices.size()) };
    TSharedPtr<FStaticMesh> newMesh =
        RendererRef ? RendererRef->CreateMesh(Desc) : nullptr;
    if (newMesh) {
        RegisterMesh(ID, newMesh);
    }
    return newMesh;
}

bool FRenderResourceLibrary::CreateFonts() {
    const std::filesystem::path ExeDir(GetExecutableDirectory());
    const auto ResourcesDir = GetResourcesDirectory();
    TArray<std::filesystem::path> SearchRoots = { ExeDir / L"Fonts" };
    if (!ResourcesDir.empty()) {
        SearchRoots.push_back(ResourcesDir / L"Fonts");
        SearchRoots.push_back(ResourcesDir / L"Textures" / L"Fonts");
        SearchRoots.push_back(ResourcesDir.parent_path() / L"Fonts");
    }
    SearchRoots.push_back(ExeDir / L"Textures" / L"Fonts");

    for (const auto& Root : SearchRoots) {
        std::error_code Ec;
        if (!std::filesystem::exists(Root, Ec)) {
            continue;
        }

        for (const auto& Entry :
            std::filesystem::recursive_directory_iterator(Root, Ec)) {
            if (!Entry.is_regular_file(Ec))
                continue;

            FWString Ext = Entry.path().extension().wstring();
            std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
            if (Ext != L".json")
                continue;

            TSharedPtr<FFont> Font = MakeShared<FFont>();

            FWString Path = Entry.path().wstring();
            Font->Deserialize(Path);

            // 확장자 제거
            FString KeyWide = Entry.path().stem().string();
            std::transform(KeyWide.begin(), KeyWide.end(), KeyWide.begin(),
                ::tolower);
            FName TextureKey(KeyWide);
            Font->SetTexture(AllTextureMap[TextureKey]);

            // 이미 로드된 폰트 건너뜀
            if (AllFontMap.find(TextureKey) != AllFontMap.end()) {
                continue;
            }

            AllFontMap[TextureKey] = Font;
        }
    }

    return true;
}

void FRenderResourceLibrary::UnregisterMaterial(const FString& InKey)
{
    AllMaterialMap.erase(InKey);
}

bool FRenderResourceLibrary::CreateMeshThumbnails() {
    if (!RendererRef) {
        return false;
    }
    FRenderer& Renderer = *RendererRef;

    FPreviewRenderTarget ThumbnailRT;
    ID3D11Device* Device = Renderer.GetDevice();
    ID3D11DeviceContext* Context = Renderer.GetContext();
    if (!Device || !Context) {
        return false;
    }

    ThumbnailRT.Resize(Device, 128, 128);

    for (const auto& [Key, Mesh] : AllUStaticMeshMap) {
        if (!Mesh) continue;

        if (AllMeshThumbnailMap[Key]) continue; // 이미 있으면 건너뜀

        auto MeshAsset = Mesh->GetStaticMeshAsset();
        if (!MeshAsset) continue;

        const FAxisAlignedBoundingBox& Bounds = Mesh->GetBounds();
        const FVector Center = (Bounds.Min + Bounds.Max) * 0.5f;
        const float Extent = (Bounds.Max - Bounds.Min).Size();
        const float Distance = (Extent > 0.1f) ? Extent * 1.5f : 5.0f;

        FCamera Cam;
        Cam.Projection.ProjectionType = EProjectionType::Perspective;
        Cam.Projection.FOV = 45.0f;
        Cam.Projection.Aspect = 1.0f;
        Cam.Pitch = -20.0f;
        Cam.Yaw = 45.0f;

        const FMatrix Rot = FMatrix::MakeRotation(FVector(0.0f, Cam.Pitch, Cam.Yaw));
        const FVector Forward{ Rot.M[0][0], Rot.M[0][1], Rot.M[0][2] };
        Cam.Position = Center - Forward * Distance;

        Renderer.RenderMeshPreviewScene(ThumbnailRT, Cam, Mesh, 128, 128, false);

        D3D11_TEXTURE2D_DESC TexDesc = {};
        TexDesc.Width = 128;
        TexDesc.Height = 128;
        TexDesc.MipLevels = 1;
        TexDesc.ArraySize = 1;
        TexDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        TexDesc.SampleDesc.Count = 1;
        TexDesc.Usage = D3D11_USAGE_DEFAULT;
        TexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> SnapshotTex;
        if (SUCCEEDED(Device->CreateTexture2D(&TexDesc, nullptr, &SnapshotTex))) {
            Context->CopyResource(SnapshotTex.Get(), ThumbnailRT.ColorTexture.Get());

            D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
            SRVDesc.Format = TexDesc.Format;
            SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            SRVDesc.Texture2D.MipLevels = 1;

            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SnapshotSRV;
            if (SUCCEEDED(Device->CreateShaderResourceView(SnapshotTex.Get(), &SRVDesc, &SnapshotSRV))) {
                auto ThumbTexture = std::shared_ptr<FTexture>(new FTexture());
                ThumbTexture->Width = 128;
                ThumbTexture->Height = 128;
                ThumbTexture->Texture2D = SnapshotTex;
                ThumbTexture->TextureSRV = SnapshotSRV;
                AllMeshThumbnailMap[Key] = ThumbTexture;
            }
        }
    }

  Renderer.BindBackBufferWithDepth();
  return true;
}

bool FRenderResourceLibrary::CreateMaterialThumbnails()
{
    if (!RendererRef) return false;
    FRenderer& Renderer = *RendererRef;

    ID3D11Device* Device = Renderer.GetDevice();
    ID3D11DeviceContext* Context = Renderer.GetContext();
    if (!Device || !Context) return false;

    // 공용 렌더 타깃 준비 (클래스 멤버 또는 static 변수)
    static FPreviewRenderTarget SharedThumbnailRT;
    if (!SharedThumbnailRT.ColorTexture)
    {
        SharedThumbnailRT.Resize(Device, 128, 128);
    }

    auto PreviewMesh = GetSphereMesh();
    if (!PreviewMesh) return false;

    const FVector Center = { 0.0f, 0.0f, 0.0f };
    const float Distance = 2.3f;

    FCamera Cam;
    Cam.Projection.ProjectionType = EProjectionType::Perspective;
    Cam.Projection.FOV = 45.0f;
    Cam.Projection.Aspect = 1.0f;
    Cam.Pitch = -15.0f;
    Cam.Yaw = 45.0f;

    const FMatrix Rot = FMatrix::MakeRotation(FVector(0.0f, Cam.Pitch, Cam.Yaw));
    const FVector Forward{ Rot.M[0][0], Rot.M[0][1], Rot.M[0][2] };
    Cam.Position = Center - Forward * Distance;

    // 텍스처 생성 스펙 구조체 (루프 밖에서 1회만 정의)
    D3D11_TEXTURE2D_DESC TexDesc = {};
    TexDesc.Width = 128;
    TexDesc.Height = 128;
    TexDesc.MipLevels = 1;
    TexDesc.ArraySize = 1;
    TexDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    TexDesc.SampleDesc.Count = 1;
    TexDesc.Usage = D3D11_USAGE_DEFAULT;
    TexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
    SRVDesc.Format = TexDesc.Format;
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MipLevels = 1;

    for (const auto& [Key, Mat] : AllMaterialMap)
    {
        if (!Mat) continue;

        if (AllMaterialThumbnailMap[Key]) continue; // 이미 있으면 건너뜀

        // 1. 공용 도화지에 구체 렌더링
        Renderer.RenderMaterialPreviewScene(SharedThumbnailRT, Cam, PreviewMesh, Mat, 128, 128, false);

        // 2. 머티리얼별 개별 Texture2D/SRV 생성 및 결과 복사
        Microsoft::WRL::ComPtr<ID3D11Texture2D> SnapshotTex;
        if (SUCCEEDED(Device->CreateTexture2D(&TexDesc, nullptr, &SnapshotTex)))
        {
            Context->CopyResource(SnapshotTex.Get(), SharedThumbnailRT.ColorTexture.Get());

            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SnapshotSRV;
            if (SUCCEEDED(Device->CreateShaderResourceView(SnapshotTex.Get(), &SRVDesc, &SnapshotSRV)))
            {
                auto ThumbTexture = std::make_shared<FTexture>();
                ThumbTexture->Width = 128;
                ThumbTexture->Height = 128;
                ThumbTexture->Texture2D = SnapshotTex;
                ThumbTexture->TextureSRV = SnapshotSRV;
                AllMaterialThumbnailMap[Key] = ThumbTexture;
            }
        }
    }

    Renderer.BindBackBufferWithDepth();
    return true;
}


void FRenderResourceLibrary::UpdateMaterialThumbnail(const FString& MatKey)
{
    if (!RendererRef) return;
    FRenderer& Renderer = *RendererRef;

    ID3D11Device* Device = Renderer.GetDevice();
    ID3D11DeviceContext* Context = Renderer.GetContext();
    if (!Device || !Context) return;

    auto MatIt = AllMaterialMap.find(MatKey);
    if (MatIt == AllMaterialMap.end() || !MatIt->second) return;

    auto PreviewMesh = GetSphereMesh();
    if (!PreviewMesh) return;

    // static 또는 클래스 멤버 변수: FPreviewRenderTarget SharedThumbnailRT;
    static FPreviewRenderTarget SharedThumbnailRT;
    if (!SharedThumbnailRT.ColorTexture)
    {
        SharedThumbnailRT.Resize(Device, 128, 128);
    }

    // 카메라 설정
    const FVector Center = { 0.0f, 0.0f, 0.0f };
    const float Distance = 2.3f;

    FCamera Cam;
    Cam.Projection.ProjectionType = EProjectionType::Perspective;
    Cam.Projection.FOV = 45.0f;
    Cam.Projection.Aspect = 1.0f;
    Cam.Pitch = -15.0f;
    Cam.Yaw = 45.0f;

    const FMatrix Rot = FMatrix::MakeRotation(FVector(0.0f, Cam.Pitch, Cam.Yaw));
    const FVector Forward{ Rot.M[0][0], Rot.M[0][1], Rot.M[0][2] };
    Cam.Position = Center - Forward * Distance;

    // 렌더링 수행
    Renderer.RenderMaterialPreviewScene(SharedThumbnailRT, Cam, PreviewMesh, MatIt->second, 128, 128, false);

    // 기존 텍스처가 있으면 VRAM 버퍼만 복사(CopyResource), 없을 때만 신규 생성
    auto ThumbIt = AllMaterialThumbnailMap.find(MatKey);
    if (ThumbIt != AllMaterialThumbnailMap.end() && ThumbIt->second && ThumbIt->second->Texture2D)
    {
        Context->CopyResource(ThumbIt->second->Texture2D.Get(), SharedThumbnailRT.ColorTexture.Get());
    }
    else
    {
        D3D11_TEXTURE2D_DESC TexDesc = {};
        TexDesc.Width = 128;
        TexDesc.Height = 128;
        TexDesc.MipLevels = 1;
        TexDesc.ArraySize = 1;
        TexDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        TexDesc.SampleDesc.Count = 1;
        TexDesc.Usage = D3D11_USAGE_DEFAULT;
        TexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> SnapshotTex;
        if (SUCCEEDED(Device->CreateTexture2D(&TexDesc, nullptr, &SnapshotTex)))
        {
            Context->CopyResource(SnapshotTex.Get(), SharedThumbnailRT.ColorTexture.Get());

            D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
            SRVDesc.Format = TexDesc.Format;
            SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            SRVDesc.Texture2D.MipLevels = 1;

            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SnapshotSRV;
            if (SUCCEEDED(Device->CreateShaderResourceView(SnapshotTex.Get(), &SRVDesc, &SnapshotSRV)))
            {
                auto ThumbTexture = std::make_shared<FTexture>();
                ThumbTexture->Width = 128;
                ThumbTexture->Height = 128;
                ThumbTexture->Texture2D = SnapshotTex;
                ThumbTexture->TextureSRV = SnapshotSRV;
                AllMaterialThumbnailMap[MatKey] = ThumbTexture;
            }
        }
    }

    Renderer.BindBackBufferWithDepth();
}


void FRenderResourceLibrary::UpdateMeshThumbnail(const FString& MeshKey)
{
    if (!RendererRef) return;
    FRenderer& Renderer = *RendererRef;

    ID3D11Device* Device = Renderer.GetDevice();
    ID3D11DeviceContext* Context = Renderer.GetContext();
    if (!Device || !Context) return;

    // 해당 메시가 존재하는지 단일 조회
    auto MeshIt = AllUStaticMeshMap.find(MeshKey);
    if (MeshIt == AllUStaticMeshMap.end() || !MeshIt->second) return;

    UStaticMesh* MeshPtr = MeshIt->second;


    static FPreviewRenderTarget SharedThumbnailRT;
    if (!SharedThumbnailRT.ColorTexture)
    {
        SharedThumbnailRT.Resize(Device, 128, 128);
    }

    const FAxisAlignedBoundingBox& Bounds = MeshPtr->GetBounds();
    const FVector Center = (Bounds.Min + Bounds.Max) * 0.5f;
    const float Extent = (Bounds.Max - Bounds.Min).Size();
    const float Distance = (Extent > 0.1f) ? (Extent * 1.5f) : 2.5f;

    FCamera Cam;
    Cam.Projection.ProjectionType = EProjectionType::Perspective;
    Cam.Projection.FOV = 45.0f;
    Cam.Projection.Aspect = 1.0f;
    Cam.Pitch = -20.0f;
    Cam.Yaw = 45.0f;

    const FMatrix Rot = FMatrix::MakeRotation(FVector(0.0f, Cam.Pitch, Cam.Yaw));
    const FVector Forward{ Rot.M[0][0], Rot.M[0][1], Rot.M[0][2] };
    Cam.Position = Center - Forward * Distance;

    Renderer.RenderMeshPreviewScene(SharedThumbnailRT, Cam, MeshPtr, 128, 128, false);

    auto ThumbIt = AllMeshThumbnailMap.find(MeshKey);
    if (ThumbIt != AllMeshThumbnailMap.end() && ThumbIt->second && ThumbIt->second->Texture2D)
    {
        Context->CopyResource(ThumbIt->second->Texture2D.Get(), SharedThumbnailRT.ColorTexture.Get());
    }

    Renderer.BindBackBufferWithDepth();
}


void FRenderResourceLibrary::RegisterMeshMaterialDependency(FString MeshId, const FString& MaterialKey)
{
    if (!MaterialKey.empty())
    {
        AllMaterialToMeshDependencyMap[MaterialKey].insert(MeshId);
    }
}

void FRenderResourceLibrary::UnregisterMeshMaterialDependency(const FString& MeshId, const FString& MaterialKey)
{
    if (MaterialKey.empty()) return;

    auto It = AllMaterialToMeshDependencyMap.find(MaterialKey);
    if (It != AllMaterialToMeshDependencyMap.end())
    {
        It->second.erase(MeshId);
        if (It->second.empty())
        {
            AllMaterialToMeshDependencyMap.erase(It);
        }
    }
}


void FRenderResourceLibrary::RefreshMaterialAndDependentThumbnails(const FString& InMaterialKey)
{
    UpdateMaterialThumbnail(InMaterialKey);

    // 이 머티리얼을 참조하는 메시들 썸네일 갱신
    auto It = AllMaterialToMeshDependencyMap.find(InMaterialKey);
    if (It != AllMaterialToMeshDependencyMap.end())
    {
        const auto& MeshSet = It->second;
        for (const FString& MeshKey : MeshSet)
        {
            UpdateMeshThumbnail(MeshKey);
        }
    }
    
}

void FRenderResourceLibrary::UpdateMeshMaterialDependency(const FString& MeshId, const FString& OldMatKey, const FString& NewMatKey)
{
    if (OldMatKey == NewMatKey) return;

    UnregisterMeshMaterialDependency(MeshId, OldMatKey);
    RegisterMeshMaterialDependency(MeshId, NewMatKey);
}
