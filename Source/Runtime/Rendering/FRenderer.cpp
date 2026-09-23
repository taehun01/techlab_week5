#include "FRenderer.h"
#include "FRenderResourceLibrary.h"

#include "FMaterial.h"
#include "FMesh.h"
#include "FRenderPipeline.h"
#include "Runtime/Core/Log.h"
#include "Runtime/Core/PointerTypes.h"
#include "Runtime/Engine/FCamera.h"
#include "Runtime/Rendering/FRenderQueue.h"
#include "Runtime/Rendering/FTexture.h"
#include "ShaderConstants.h"
#include "ThirdParty/DirectXTK/Inc/DDSTextureLoader.h"
#include "ThirdParty/DirectXTK/Inc/WICTextureLoader.h"
#include <cwctype>
#include "Vertices.h"
#include "FPreviewRenderTarget.h"
#include "Runtime/CoreUObject/UStaticMesh.h"
#include "Editor/Grid/FGrid.h"
#include <Windows.h>
#include <cmath>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>


bool FRenderer::Initialize(HWND Window) {
  if (!InitializeDeviceAndSwapChain(Window) ||
      !InitializeBackBufferAndDepthStencil() ||
      !InitializeEditorViewportRenderTarget() || !InitializeConstantBuffers()) {
    Shutdown();
    return false;
  }

  LineBatcher.Initialize(Device.Get()); // batch line

  return true;
}

void FRenderer::Shutdown() {
  if (Context) {
    Context->ClearState();
    Context->Flush();
  }

  LineBatcher.Shutdown();

  b0ConstantBuffer.Reset();
  FrameConstantBuffer.Reset();

  BackBufferRTV.Reset();
  DepthStencilView.Reset();
  DepthStencilBuffer.Reset();

  SwapChain.Reset();
  Context.Reset();
  Device.Reset();
}

void FRenderer::BeginFrame() {
  Context->RSSetViewports(1, &Viewport);
  BindEditorViewportRenderTargets();

  constexpr float ClearColor[] = {0.05f, 0.05f, 0.08f, 1.0f};
  Context->ClearRenderTargetView(EditorViewPortRTV.Get(), ClearColor);
  Context->ClearDepthStencilView(
      DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

void FRenderer::BindEditorViewportRenderTargets() {
  Context->OMSetRenderTargets(1, EditorViewPortRTV.GetAddressOf(),
                              DepthStencilView.Get());
}

void FRenderer::SetViewportUV(FVector2 TopLeftUV, FVector2 LengthUV) {
  // 유효성 검사
  if (Viewport.Width <= 0.0f || Viewport.Height <= 0.0f ||
      LengthUV.X <= 0.0f || LengthUV.Y <= 0.0f ||
      !std::isfinite(TopLeftUV.X) || !std::isfinite(TopLeftUV.Y) ||
      !std::isfinite(LengthUV.X) || !std::isfinite(LengthUV.Y)) {
    return;
  }

  // 뷰포트 변환
  D3D11_VIEWPORT RenderViewport = Viewport;
  RenderViewport.TopLeftX = TopLeftUV.X * Viewport.Width;
  RenderViewport.TopLeftY = TopLeftUV.Y * Viewport.Height;
  RenderViewport.Width = LengthUV.X * Viewport.Width;
  RenderViewport.Height = LengthUV.Y * Viewport.Height;
  Context->RSSetViewports(1, &RenderViewport);

  FFrameConstants Constants{
      FVector2{RenderViewport.Width, RenderViewport.Height}};
  Context->UpdateSubresource(FrameConstantBuffer.Get(), 0, nullptr, &Constants,
                             0, 0);
  Context->VSSetConstantBuffers(1, 1, FrameConstantBuffer.GetAddressOf());
  Context->PSSetConstantBuffers(1, 1, FrameConstantBuffer.GetAddressOf());
};

// void FRenderer::Draw(const FMesh &Mesh, const FMaterial &Material,
//                      const FObjectConstants &ObjectConstants) {
//   UpdateObjectConstants(ObjectConstants);
//
//   TSharedPtr<FRenderPipeline> Pipeline = Material.Pipeline;
//   if (CurrentRenderMode == EViewModeIndex::VMI_Wireframe) {
//     Pipeline = GetPipeline(EBuiltinPipeline::Simple_Wireframe);
//   }
//
//   if (Pipeline) {
//     Pipeline->Bind(*Context.Get());
//   }
//
//   Material.BindResources(*Context.Get());
//   Mesh.BindResources(*Context.Get());
//
//   Context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
//
//   if (Mesh.HasIndices()) {
//     Context->DrawIndexed(Mesh.IndexCount, 0, 0);
//   } else {
//     Context->Draw(Mesh.VertexCount, 0);
//   }
// }
//
// void FRenderer::DrawGrid(const FMesh &Mesh, const FMaterial &Material,
//                          const FGridConstants &GridConstants) {
//   UpdateGridConstants(GridConstants);
//   const auto &Pipeline = Material.Pipeline;
//
//   Pipeline->Bind(*Context.Get());
//   Material.BindResources(*Context.Get());
//   Mesh.BindResources(*Context.Get());
//
//   Context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
//
//   if (Mesh.HasIndices()) {
//     Context->DrawIndexed(Mesh.IndexCount, 0, 0);
//   } else {
//     Context->Draw(Mesh.VertexCount, 0);
//   }
// }

void FRenderer::ClearDepth() {
  Context->ClearDepthStencilView(
      DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

void FRenderer::SwapBuffer() { SwapChain->Present(1u, 0u); }

void FRenderer::OnWindowSize(UINT Width, UINT Height) {
  Context->OMSetRenderTargets(0, nullptr, nullptr);
  BackBufferRTV.Reset();
  DepthStencilView.Reset();
  DepthStencilSRV.Reset();
  DepthStencilBuffer.Reset();
  EditorViewPortRTV.Reset();
  EditorViewPortSRV.Reset();
  renderTexture.Reset();

  SwapChain->ResizeBuffers(0, Width, Height, DXGI_FORMAT_UNKNOWN, 0);
  Viewport.Width = static_cast<float>(Width);
  Viewport.Height = static_cast<float>(Height);

  InitializeBackBufferAndDepthStencil();
  InitializeEditorViewportRenderTarget();
}

TSharedPtr<FStaticMesh> FRenderer::CreateMesh(const FMeshDesc &Desc) {
  if (!Desc.VertexData || Desc.VertexCount == 0 || Desc.VertexDataSize == 0 ||
      Desc.VertexStride == 0) {
    return nullptr;
  }
  if (Desc.IndexCount > 0 && (!Desc.IndexData || Desc.IndexDataSize == 0)) {
    return nullptr;
  }

  auto Mesh = TSharedPtr<FStaticMesh>{new FStaticMesh()};
  
  D3D11_BUFFER_DESC VertexBufferDesc = {
      .ByteWidth = Desc.VertexDataSize,
      .Usage = D3D11_USAGE_DEFAULT,
      .BindFlags = D3D11_BIND_VERTEX_BUFFER,
  };

  D3D11_SUBRESOURCE_DATA VertexData = {
      .pSysMem = Desc.VertexData,
  };

  HRESULT Result =
      Device->CreateBuffer(&VertexBufferDesc, &VertexData, &Mesh->VertexBuffer);
  if (FAILED(Result)) {
    return nullptr;
  }
  Mesh->VertexCount = Desc.VertexCount;
  Mesh->VertexStride = Desc.VertexStride;

  if (Desc.IndexCount > 0 && Desc.IndexData) {
    D3D11_BUFFER_DESC IndexBufferDesc = {
        .ByteWidth = Desc.IndexDataSize,
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_INDEX_BUFFER,
    };

    D3D11_SUBRESOURCE_DATA IndexData = {
        .pSysMem = Desc.IndexData,
    };

    Result =
        Device->CreateBuffer(&IndexBufferDesc, &IndexData, &Mesh->IndexBuffer);
    if (FAILED(Result)) {
      return nullptr;
    }
  }
  Mesh->IndexCount = Desc.IndexCount;

  const auto *vertices = static_cast<const FVertexData *>(Desc.VertexData);

  Mesh->Positions.reserve(Desc.VertexCount);
  for (uint32 i = 0; i < Desc.VertexCount; ++i) {
    Mesh->Positions.push_back(
        FVector{vertices[i].x, vertices[i].y, vertices[i].z});
  }

  if (Desc.IndexCount > 0) {
    const auto *indices = static_cast<const uint32 *>(Desc.IndexData);
    Mesh->Indices.assign(indices, indices + Desc.IndexCount);
  }

  Mesh->Topology = Desc.bIsLine ? D3D11_PRIMITIVE_TOPOLOGY_LINELIST
                                : D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

  Mesh->LocalBounds = FAxisAlignedBoundingBox{*Mesh.get()};
  return Mesh;
}

TSharedPtr<FStaticMesh> FRenderer::CreateDynamicMesh(const FMeshDesc &Desc) {
  if (!Desc.VertexData || Desc.VertexCount == 0 || Desc.VertexDataSize == 0 ||
      Desc.VertexStride == 0) {
    return nullptr;
  }
  if (Desc.IndexCount > 0 && (!Desc.IndexData || Desc.IndexDataSize == 0)) {
    return nullptr;
  }

  auto Mesh = TSharedPtr<FStaticMesh>{new FStaticMesh()};
  D3D11_BUFFER_DESC VertexBufferDesc = {
      .ByteWidth = Desc.VertexDataSize,
      .Usage = D3D11_USAGE_DYNAMIC,
      .BindFlags = D3D11_BIND_VERTEX_BUFFER,
      .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
  };

  D3D11_SUBRESOURCE_DATA VertexData = {
      .pSysMem = Desc.VertexData,
  };

  HRESULT Result =
      Device->CreateBuffer(&VertexBufferDesc, &VertexData, &Mesh->VertexBuffer);
  if (FAILED(Result)) {
    return nullptr;
  }
  Mesh->VertexCount = Desc.VertexCount;
  Mesh->VertexStride = Desc.VertexStride;
  Mesh->VertexBufferSize = Desc.VertexDataSize;

  if (Desc.IndexCount > 0 && Desc.IndexData) {
    D3D11_BUFFER_DESC IndexBufferDesc = {
        .ByteWidth = Desc.IndexDataSize,
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_INDEX_BUFFER,
    };

    D3D11_SUBRESOURCE_DATA IndexData = {
        .pSysMem = Desc.IndexData,
    };

    Result =
        Device->CreateBuffer(&IndexBufferDesc, &IndexData, &Mesh->IndexBuffer);
    if (FAILED(Result)) {
      return nullptr;
    }
  }
  Mesh->IndexCount = Desc.IndexCount;
  Mesh->IndexBufferSize = Desc.IndexDataSize;

  const auto *vertices = static_cast<const FVertexData *>(Desc.VertexData);

  Mesh->Positions.reserve(Desc.VertexCount);
  for (uint32 i = 0; i < Desc.VertexCount; ++i) {
    Mesh->Positions.push_back(
        FVector{vertices[i].x, vertices[i].y, vertices[i].z});
  }

  if (Desc.IndexCount > 0) {
    const auto *indices = static_cast<const uint32 *>(Desc.IndexData);
    Mesh->Indices.assign(indices, indices + Desc.IndexCount);
  }

  Mesh->Topology = Desc.bIsLine ? D3D11_PRIMITIVE_TOPOLOGY_LINELIST
                                : D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

  Mesh->LocalBounds = FAxisAlignedBoundingBox{*Mesh.get()};
  return Mesh;
}

TSharedPtr<FMaterial> FRenderer::CreateMaterial(const FMaterialDesc &Desc) {
  TSharedPtr<FMaterial> Material{new FMaterial()};
  return Material;
}

void FRenderer::GetDeviceAndContext_ImplDX11(ID3D11Device *&DeviceOut,
                                             ID3D11DeviceContext *&ContextOut) {
  DeviceOut = Device.Get();
  ContextOut = Context.Get();
}

TSharedPtr<FRenderPipeline>
FRenderer::CreateRenderPipeline(const FRenderPipelineDesc &Desc,
                                EViewModeIndex RenderMode) {
  TSharedPtr<FRenderPipeline> Pipeline{new FRenderPipeline()};
  Pipeline->desc = Desc;

  Microsoft::WRL::ComPtr<ID3DBlob> Blob;
  HRESULT Result = D3DReadFileToBlob(Desc.VertexShaderFileName.c_str(), &Blob);
  if (FAILED(Result)) {
    return nullptr;
  }

  Result = Device->CreateVertexShader(Blob->GetBufferPointer(),
                                      Blob->GetBufferSize(), nullptr,
                                      &Pipeline->VertexShader);
  if (FAILED(Result)) {
    return nullptr;
  }

  if (Desc.bIsInstancing) {
    Result = Device->CreateInputLayout(
        FVertexInstanceLayouts::Layout, FVertexInstanceLayouts::NumElements,
        Blob->GetBufferPointer(), Blob->GetBufferSize(),
        &Pipeline->InputLayout);
  } else {
    Result = Device->CreateInputLayout(
        FVertexLayouts::Layout, FVertexLayouts::NumElements,
        Blob->GetBufferPointer(), Blob->GetBufferSize(),
        &Pipeline->InputLayout);
  }

  if (FAILED(Result)) {
    return nullptr;
  }

  Result = D3DReadFileToBlob(Desc.PixelShaderFileName.c_str(), &Blob);
  if (FAILED(Result)) {
    return nullptr;
  }

  Result =
      Device->CreatePixelShader(Blob->GetBufferPointer(), Blob->GetBufferSize(),
                                nullptr, &Pipeline->PixelShader);
  if (FAILED(Result)) {
    return nullptr;
  }

  D3D11_RASTERIZER_DESC RasterizerDesc{
      .FillMode = (RenderMode == EViewModeIndex::VMI_Wireframe)
                      ? D3D11_FILL_WIREFRAME
                      : D3D11_FILL_SOLID,
      .CullMode = Desc.CullMode,
      .FrontCounterClockwise = false,
  };

  Result = Device->CreateRasterizerState(&RasterizerDesc,
                                         &Pipeline->RasterizerState);
  if (FAILED(Result)) {
    return nullptr;
  }

  D3D11_DEPTH_STENCIL_DESC DepthStencilDesc{
      .DepthEnable = Desc.bEnableDepthTest,
      .DepthWriteMask = Desc.bEnableDepthWrite ? D3D11_DEPTH_WRITE_MASK_ALL
                                               : D3D11_DEPTH_WRITE_MASK_ZERO,
      .DepthFunc = D3D11_COMPARISON_LESS,
  };

  Result = Device->CreateDepthStencilState(&DepthStencilDesc,
                                           &Pipeline->DepthStencilState);
  if (FAILED(Result)) {
    return nullptr;
  }

  // 블렌드 상태 생성
  D3D11_BLEND_DESC BlendDesc{};
  BlendDesc.AlphaToCoverageEnable = false;
  BlendDesc.IndependentBlendEnable = false;
  auto &RenderTargetBlend = BlendDesc.RenderTarget[0];

  switch (Desc.BlendMode) {
  case EBlendMode::Opaque:
  case EBlendMode::Masked:
    RenderTargetBlend.BlendEnable = false;
    break;

  case EBlendMode::Translucent:
    RenderTargetBlend.BlendEnable = true;
    RenderTargetBlend.SrcBlend = D3D11_BLEND_SRC_ALPHA;
    RenderTargetBlend.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    RenderTargetBlend.BlendOp = D3D11_BLEND_OP_ADD;
    RenderTargetBlend.SrcBlendAlpha = D3D11_BLEND_ONE;
    RenderTargetBlend.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    RenderTargetBlend.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    break;

  case EBlendMode::Additive:
    RenderTargetBlend.BlendEnable = true;
    RenderTargetBlend.SrcBlend = D3D11_BLEND_ONE;
    RenderTargetBlend.DestBlend = D3D11_BLEND_ONE;
    RenderTargetBlend.BlendOp = D3D11_BLEND_OP_ADD;
    RenderTargetBlend.SrcBlendAlpha = D3D11_BLEND_ONE;
    RenderTargetBlend.DestBlendAlpha = D3D11_BLEND_ZERO;
    RenderTargetBlend.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    break;

  case EBlendMode::PremultipliedAlpha:
    RenderTargetBlend.BlendEnable = true;
    RenderTargetBlend.SrcBlend = D3D11_BLEND_ONE;
    RenderTargetBlend.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    RenderTargetBlend.BlendOp = D3D11_BLEND_OP_ADD;
    RenderTargetBlend.SrcBlendAlpha = D3D11_BLEND_ONE;
    RenderTargetBlend.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    RenderTargetBlend.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    break;
  }

  RenderTargetBlend.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

  Result = Device->CreateBlendState(&BlendDesc, &Pipeline->BlendState);
  if (FAILED(Result)) {
    return nullptr;
  }

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
    return nullptr;
  }

  return Pipeline;
}

TSharedPtr<FTexture> FRenderer::CreateTexture(const wchar_t *path) {
  auto Texture = TSharedPtr<FTexture>{new FTexture()};
  Microsoft::WRL::ComPtr<ID3D11Resource> TempResource;
  std::wstring Extension = std::filesystem::path(path).extension().wstring();
  std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::towlower);
  HRESULT hr = Extension == L".dds"
      ? DirectX::CreateDDSTextureFromFile(Device.Get(), path,
          TempResource.GetAddressOf(), Texture->TextureSRV.GetAddressOf())
      : DirectX::CreateWICTextureFromFile(Device.Get(), path,
          TempResource.GetAddressOf(), Texture->TextureSRV.GetAddressOf());
  if (FAILED(hr)) {
    UE_LOG_WARN("[Texture Loader] Failed: %s (HRESULT=0x%08lX)",
        std::filesystem::path(path).string().c_str(), static_cast<unsigned long>(hr));
    return nullptr;
  }

  hr = TempResource.As(&Texture->Texture2D);
  if (FAILED(hr)) {
    return nullptr;
  }

  D3D11_TEXTURE2D_DESC desc;
  Texture->Texture2D->GetDesc(&desc);
  Texture->Width = desc.Width;
  Texture->Height = desc.Height;
  
  return Texture;
}

TSharedPtr<FRenderPipeline> FRenderer::GetPipeline(const FName &Id) const {
  return FRenderResourceLibrary::Get().GetPipeline(Id);
}

bool FRenderer::InitializeDeviceAndSwapChain(HWND Window) {
  constexpr D3D_FEATURE_LEVEL FeatureLevels[] = {D3D_FEATURE_LEVEL_11_0};

  DXGI_SWAP_CHAIN_DESC SwapChainDesc{
      .BufferDesc =
          {
              .Width = 0u,
              .Height = 0u,
              .Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
          },
      .SampleDesc =
          {
              .Count = 1u,
          },
      .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
      .BufferCount = 2u,
      .OutputWindow = Window,
      .Windowed = true,
      .SwapEffect = DXGI_SWAP_EFFECT_DISCARD,
  };

  UINT CreateDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifndef NDEBUG
  CreateDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

  HRESULT Result = D3D11CreateDeviceAndSwapChain(
      nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, CreateDeviceFlags,
      FeatureLevels, ARRAYSIZE(FeatureLevels), D3D11_SDK_VERSION,
      &SwapChainDesc, &SwapChain, &Device, nullptr, &Context);
  if (FAILED(Result)) {
    return false;
  }

  RECT ClientRect{};
  GetClientRect(Window, &ClientRect);

  Viewport = {
      .TopLeftX = 0.0f,
      .TopLeftY = 0.0f,
      .Width = static_cast<float>(ClientRect.right - ClientRect.left),
      .Height = static_cast<float>(ClientRect.bottom - ClientRect.top),
      .MinDepth = 0.0f,
      .MaxDepth = 1.0f,
  };

  return true;
}

bool FRenderer::InitializeBackBufferAndDepthStencil() {
  Microsoft::WRL::ComPtr<ID3D11Texture2D> BackBuffer;
  HRESULT Result = SwapChain->GetBuffer(0, IID_PPV_ARGS(&BackBuffer));
  if (FAILED(Result)) {
    return false;
  }

  Result =
      Device->CreateRenderTargetView(BackBuffer.Get(), nullptr, &BackBufferRTV);
  if (FAILED(Result)) {
    return false;
  }

  const UINT Width = static_cast<UINT>(Viewport.Width);
  const UINT Height = static_cast<UINT>(Viewport.Height);

  D3D11_TEXTURE2D_DESC DepthStencilDesc = {
      .Width = Width,
      .Height = Height,
      .MipLevels = 1u,
      .ArraySize = 1u,
      .Format = DXGI_FORMAT_R24G8_TYPELESS,
      .SampleDesc =
          {
              .Count = 1u,
          },
      .Usage = D3D11_USAGE_DEFAULT,
      .BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE,
  };

  Result =
      Device->CreateTexture2D(&DepthStencilDesc, nullptr, &DepthStencilBuffer);
  if (FAILED(Result)) {
    return false;
  }

  D3D11_DEPTH_STENCIL_VIEW_DESC DsvDesc{
      .Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
      .ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D,
  };
  Result = Device->CreateDepthStencilView(DepthStencilBuffer.Get(), &DsvDesc,
                                          &DepthStencilView);
  if (FAILED(Result)) {
    return false;
  }

  D3D11_SHADER_RESOURCE_VIEW_DESC StencilSrvDesc{
      .Format = DXGI_FORMAT_X24_TYPELESS_G8_UINT,
      .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
      .Texture2D = {.MostDetailedMip = 0, .MipLevels = 1},
  };
  Result = Device->CreateShaderResourceView(DepthStencilBuffer.Get(),
                                            &StencilSrvDesc, &DepthStencilSRV);
  if (FAILED(Result)) {
    return false;
  }

  return true;
}

bool FRenderer::InitializeEditorViewportRenderTarget() {
  if (!Device) {
    return false;
  }

  const UINT Width = static_cast<UINT>(Viewport.Width);
  const UINT Height = static_cast<UINT>(Viewport.Height);
  if (Width == 0 || Height == 0) {
    return false;
  }

  D3D11_TEXTURE2D_DESC ColorTexDesc{
      .Width = Width,
      .Height = Height,
      .MipLevels = 1u,
      .ArraySize = 1u,
      .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
      .SampleDesc = {.Count = 1u},
      .Usage = D3D11_USAGE_DEFAULT,
      .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
  };

  HRESULT Result =
      Device->CreateTexture2D(&ColorTexDesc, nullptr, &renderTexture);
  if (FAILED(Result)) {
    return false;
  }

  Result = Device->CreateRenderTargetView(renderTexture.Get(), nullptr,
                                          &EditorViewPortRTV);
  if (FAILED(Result)) {
    return false;
  }

  Result = Device->CreateShaderResourceView(renderTexture.Get(), nullptr,
                                            &EditorViewPortSRV);
  if (FAILED(Result)) {
    return false;
  }

  return true;
}

bool FRenderer::InitializeConstantBuffers() {
  // b0를 쓰는 모든 상수 타입이 공유하는 버퍼.
  // 가장 큰 구조체보다 크게 잡아두고, 초과 여부는 UpdateBuffer의
  // static_assert가 잡는다.
  D3D11_BUFFER_DESC b0Desc = {
      .ByteWidth = ConstantBufferSize,
      .Usage = D3D11_USAGE_DYNAMIC,
      .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
      .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
  };

  HRESULT Result = Device->CreateBuffer(&b0Desc, nullptr, &b0ConstantBuffer);
  if (FAILED(Result)) {
    return false;
  }

  D3D11_BUFFER_DESC FrameConstantBufferDesc = {
      .ByteWidth = sizeof(FFrameConstants),
      .Usage = D3D11_USAGE_DEFAULT,
      .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
  };

  Result = Device->CreateBuffer(&FrameConstantBufferDesc, nullptr,
                                &FrameConstantBuffer);

  if (FAILED(Result)) {
    return false;
  }

  D3D11_BUFFER_DESC lightbufferDesc = {
      .ByteWidth = sizeof(FLightConstants),
      .Usage = D3D11_USAGE_DEFAULT,
      .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
  };

  Result =
      Device->CreateBuffer(&lightbufferDesc, nullptr, &LightConstantBuffer);

  if (FAILED(Result)) {
    return false;
  }

  return true;
}

void FRenderer::UpdateLightConstants(const FLightConstants &Constants,
                                     const EViewModeIndex InMode) {
  Context->UpdateSubresource(LightConstantBuffer.Get(), 0, nullptr, &Constants,
                             0, 0);
  Context->PSSetConstantBuffers(2, 1, LightConstantBuffer.GetAddressOf());
}

void FRenderer::AddTextInstanceArray(const TArray<FInstanceData> &Instances,
                                     const FName &MeshId,
                                     const FName &MaterialId) {
  // 빈 데이터 전달 시 조기 반환
  if (Instances.empty()) {
    return;
  }
  auto &ResLib = FRenderResourceLibrary::Get();
  // 머티리얼 리소스 존재 여부 확인
  if (!ResLib.GetMaterial(MaterialId)) {
    UE_LOG_WARN("[FRenderer] 유효하지 않은 머티리얼 ID 인스턴스 등록 시도");
    return;
  }
  // 메시 리소스 존재 여부 확인
  if (!ResLib.GetMesh(MeshId)) {
    UE_LOG_WARN("[FRenderer] 유효하지 않은 메시 ID 인스턴스 등록 시도");
    return;
  }

  auto &TargetArray = ResLib.GetInstancingArray(MaterialId, MeshId);
  TargetArray.reserve(TargetArray.size() + Instances.size());
  TargetArray.insert(TargetArray.end(), Instances.begin(), Instances.end());
}

void FRenderer::DrawInstances(const FCamera &Camera) {
  auto &ResLib = FRenderResourceLibrary::Get();

  FObjectConstants SC{};
  SC.MVP = Camera.CreateViewProjectionMatrix();

  // 배치 키(MaterialID, MeshID) 순회
  for (const auto &[BatchKey, InstanceData] : ResLib.AllInstancingArrayMap) {
    if (InstanceData.empty())
      continue;

    SC.DisableShading =
        CurrentRenderMode == EViewModeIndex::VMI_Unlit ||
        BatchKey.MaterialID == FName("Instance_Simple") ? 1.0f : 0.0f;
    UpdateBuffer(SC);

    const UINT InstanceCount = static_cast<UINT>(InstanceData.size());
    const UINT RequiredSize = InstanceCount * sizeof(FInstanceData);

    // 버퍼 크기 부족 시 동적 확장
    if (RequiredSize > TextInstanceBufferSize) {
      InstanceBuffer.Reset();
      D3D11_BUFFER_DESC Desc{};
      Desc.ByteWidth = RequiredSize;
      Desc.Usage = D3D11_USAGE_DYNAMIC;
      Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
      Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

      if (FAILED(Device->CreateBuffer(&Desc, nullptr, &InstanceBuffer)))
        continue;
      TextInstanceBufferSize = RequiredSize;
    }

    // 인스턴스 데이터 업로드
    D3D11_MAPPED_SUBRESOURCE MappedResource{};
    if (FAILED(Context->Map(InstanceBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
                            &MappedResource)))
      continue;
    std::memcpy(MappedResource.pData, InstanceData.data(), RequiredSize);
    Context->Unmap(InstanceBuffer.Get(), 0);

    // 머티리얼 및 파이프라인 바인딩
    auto Material = ResLib.GetMaterial(BatchKey.MaterialID);
    if (!Material)
      continue;

    TSharedPtr<FRenderPipeline> Pipeline = Material->GetPipeline();
    if (Pipeline) {
      Pipeline->Bind(*Context.Get());
    }
    Material->BindResources(*Context.Get());

    // 메시 조회 및 바인딩
    auto Mesh = ResLib.GetMesh(BatchKey.MeshID);
    if (!Mesh)
      continue;
    Mesh->BindResources(*Context.Get());

    // 슬롯 1에 인스턴스 버퍼 바인딩
    UINT Stride = sizeof(FInstanceData);
    UINT Offset = 0;
    Context->IASetVertexBuffers(1, 1, InstanceBuffer.GetAddressOf(), &Stride,
                                &Offset);

    // 인스턴스 렌더링 호출
    if (Mesh->HasIndices()) {
      Context->DrawIndexedInstanced(Mesh->GetIndexCount(), InstanceCount, 0, 0,
                                    0);
    } else {
      Context->DrawInstanced(Mesh->VertexCount, InstanceCount, 0, 0);
    }
  }
}

void FRenderer::DrawTextInstances(const FCamera &Camera, const FName &MeshId,
                                  const FName &MaterialId) {
  auto &ResLib = FRenderResourceLibrary::Get();

  // 상수 버퍼 업데이트
  FObjectConstants SC{};
  SC.MVP = Camera.CreateViewProjectionMatrix();
  UpdateBuffer(SC);

  TArray<FInstanceData> InstanceData =
      FRenderResourceLibrary::Get().GetInstancingArray(MaterialId, MeshId);

  if (InstanceData.empty())
    return;

  const UINT InstanceCount = static_cast<UINT>(InstanceData.size());
  const UINT RequiredSize = InstanceCount * sizeof(FInstanceData);

  // 버퍼 크기 부족 시 동적 확장
  if (RequiredSize > TextInstanceBufferSize) {
    InstanceBuffer.Reset();
    D3D11_BUFFER_DESC Desc{};
    Desc.ByteWidth = RequiredSize;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(Device->CreateBuffer(&Desc, nullptr, &InstanceBuffer)))
      return;
    TextInstanceBufferSize = RequiredSize;
  }

  // 인스턴스 데이터 업로드
  D3D11_MAPPED_SUBRESOURCE MappedResource{};
  if (FAILED(Context->Map(InstanceBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
                          &MappedResource)))
    return;
  std::memcpy(MappedResource.pData, InstanceData.data(), RequiredSize);
  Context->Unmap(InstanceBuffer.Get(), 0);

  // 머티리얼 및 파이프라인 바인딩
  auto Material = ResLib.GetMaterial(MaterialId);
  if (!Material)
    return;

  TSharedPtr<FRenderPipeline> Pipeline = Material->GetPipeline();
  if (Pipeline) {
    Pipeline->Bind(*Context.Get());
  }
  Material->BindResources(*Context.Get());

  // 메시 조회 및 바인딩
  auto Mesh = ResLib.GetMesh(MeshId);
  if (!Mesh)
    return;
  Mesh->BindResources(*Context.Get());

  // 슬롯 1에 인스턴스 버퍼 바인딩
  UINT Stride = sizeof(FInstanceData);
  UINT Offset = 0;
  Context->IASetVertexBuffers(1, 1, InstanceBuffer.GetAddressOf(), &Stride,
                              &Offset);

  // 인스턴스 렌더링 호출
  if (Mesh->HasIndices()) {
    Context->DrawIndexedInstanced(Mesh->GetIndexCount(), InstanceCount, 0, 0,
                                  0);
  } else {
    Context->DrawInstanced(Mesh->VertexCount, InstanceCount, 0, 0);
  }
}

void FRenderer::ClearTextInstances() {
  for (auto &[BatchKey, InstanceArray] :
       FRenderResourceLibrary::Get().AllInstancingArrayMap) {
    InstanceArray.clear();
  }

  FRenderResourceLibrary::Get().DestroyAllInstancingArray();
}

void FRenderer::BindBackBufferWithDepth() {
  Context->OMSetRenderTargets(1, BackBufferRTV.GetAddressOf(), DepthStencilView.Get());
}

void FRenderer::RenderOutline(FVector2 TopLeftUV, FVector2 LengthUV) {
  // 뷰포트 영역 설정
  D3D11_VIEWPORT RenderViewport = Viewport;
  RenderViewport.TopLeftX = TopLeftUV.X * Viewport.Width;
  RenderViewport.TopLeftY = TopLeftUV.Y * Viewport.Height;
  RenderViewport.Width = LengthUV.X * Viewport.Width;
  RenderViewport.Height = LengthUV.Y * Viewport.Height;

  Context->RSSetViewports(1, &RenderViewport);
  Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  Context->IASetInputLayout(nullptr);

  ID3D11Buffer *NullVB = nullptr;
  UINT Zero = 0;
  Context->IASetVertexBuffers(0, 1, &NullVB, &Zero, &Zero);

  Context->OMSetRenderTargets(1, BackBufferRTV.GetAddressOf(), nullptr);
  // 텍스처 바인딩
  ID3D11ShaderResourceView *SRVs[] = {EditorViewPortSRV.Get(),
                                      DepthStencilSRV.Get()};
  Context->PSSetShaderResources(0, 2, SRVs);

  if (auto Pipeline = FRenderResourceLibrary::Get().GetPipeline(FName("PostProcess"))) {
    Pipeline->Bind(*Context.Get());
    Context->Draw(3, 0);
  }

  // 슬롯 해제
  ID3D11ShaderResourceView *NullSRVs[] = {nullptr, nullptr};
  Context->PSSetShaderResources(0, 2, NullSRVs);

  // 깊이버퍼 복구
  BindBackBufferWithDepth();
}

void FRenderer::RenderMeshPreviewScene(FPreviewRenderTarget& RenderTarget, const FCamera& Camera, 
    UStaticMesh* TargetMesh, uint32 Width, uint32 Height, bool bDrawGrid, TSharedPtr<FMaterial> OverrideMaterial)
{
  if (!TargetMesh)
  {
    return;
  }

  const TSharedPtr<FStaticMesh> MeshAsset = TargetMesh->GetStaticMeshAsset();
  if (!MeshAsset)
  {
    return;
  }

  if (!Device || !Context)
  {
    return;
  }

  // 렌더타겟 크기 맞춤
  if (Width > 0 && Height > 0)
  {
    RenderTarget.Resize(Device.Get(), Width, Height);
  }
  if (!RenderTarget.IsValid() || RenderTarget.Width == 0 || RenderTarget.Height == 0)
  {
    return;
  }

  // 프리뷰 렌더타겟 바인딩
  ID3D11RenderTargetView* RTV = RenderTarget.RenderTargetView.Get();
  ID3D11DepthStencilView* DSV = RenderTarget.DepthStencilView.Get();
  Context->OMSetRenderTargets(1, &RTV, DSV);

  // 배경 및 깊이 버퍼 클리어
  const float ClearColor[4] = { 0.12f, 0.13f, 0.16f, 1.0f };
  Context->ClearRenderTargetView(RTV, ClearColor);
  Context->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

  // 뷰포트 설정
  D3D11_VIEWPORT D3DVP = {};
  D3DVP.TopLeftX = 0.0f;
  D3DVP.TopLeftY = 0.0f;
  D3DVP.Width = static_cast<float>(RenderTarget.Width);
  D3DVP.Height = static_cast<float>(RenderTarget.Height);
  D3DVP.MinDepth = 0.0f;
  D3DVP.MaxDepth = 1.0f;
  Context->RSSetViewports(1, &D3DVP);

  // 조명 상수 버퍼 설정 및 바인딩
  FLightConstants LightConstants;
  LightConstants.LightDirection = FVector(-0.577f, -0.577f, -0.577f);
  LightConstants.Intensity = 1.2f;
  LightConstants.LightColor = FVector(1.0f, 1.0f, 1.0f);
  LightConstants.AmbientIntensity = 0.4f;
  SetRenderMode(EViewModeIndex::VMI_Lit);
  UpdateLightConstants(LightConstants, EViewModeIndex::VMI_Lit);

  
  auto& Sections = MeshAsset->GetSections();

  FObjectConstants ObjConstants = {};
  ObjConstants.World = FMatrix::GetIdentity();
  ObjConstants.MVP = ObjConstants.World * Camera.CreateViewProjectionMatrix();
  ObjConstants.UVScale = FVector2(1.0f, 1.0f);
  ObjConstants.ColorOverride = FVector(1.0f, 1.0f, 1.0f);
  ObjConstants.ColorOverrideAmount = 0.0f;

  if (!Sections.empty())
  {
      for (int i = 0; i < TargetMesh->Materials.size(); i++)
      {
          // OverrideMaterial이 넘어왔다면(머티리얼 프리뷰 창일 때) 최우선 바인딩, 없으면 라이브러리에서 조회
          TSharedPtr<FMaterial> MaterialToDraw = OverrideMaterial;
          if (!MaterialToDraw)
          {
              MaterialToDraw = FRenderResourceLibrary::Get().GetMaterial(TargetMesh->Materials[i]);
              if (!MaterialToDraw)
              {
                  MaterialToDraw = FRenderResourceLibrary::Get().GetMaterial(FName("Simple"));
              }
          }

          if (MaterialToDraw && i < static_cast<int>(Sections.size()))
          {
              Draw(*MeshAsset, *MaterialToDraw, ObjConstants, Sections.at(i).FirstIndex, Sections.at(i).IndexCount, 0, false);
          }
      }
  }
  else
  {
      // 기본 구체/큐브 등 섹션이 없는 단일 메시의 경우
      TSharedPtr<FMaterial> MaterialToDraw = OverrideMaterial;
      if (!MaterialToDraw)
      {
          FName MatKey = (!TargetMesh->Materials.empty()) ? TargetMesh->Materials[0] : TargetMesh->MeshId;
          MaterialToDraw = FRenderResourceLibrary::Get().GetMaterial(MatKey);

          if (!MaterialToDraw)
          {
              MaterialToDraw = FRenderResourceLibrary::Get().GetMaterial(FName("Simple"));
          }
      }

      if (MaterialToDraw)
      {
          Draw(*MeshAsset, *MaterialToDraw, ObjConstants, 0, -1, 0, false);
      }
  }

  // 그리드 렌더링
  if (bDrawGrid)
  {
    FGrid Grid;
    Grid.DrawLine(*this, Camera);

    const float Extent = (TargetMesh->GetBounds().Max - TargetMesh->GetBounds().Min).Size();
    FGridLineConstants GridConstants = {};
    GridConstants.MVP = Camera.CreateViewProjectionMatrix();
    GridConstants.CameraPosition = Camera.Position;
    GridConstants.FadeStartDistance = std::max(5.0f, Extent * 0.5f);
    GridConstants.FadeEndDistance = std::max(100.0f, Extent * 10.0f);
    FlushLineBatch(GridConstants, FName("Grid"));
  }
}

void FRenderer::RenderMaterialPreviewScene(FPreviewRenderTarget& RenderTarget, const FCamera& Camera, TSharedPtr<FStaticMesh> Meshasset, 
    TSharedPtr<FMaterial> Material, uint32 Width, uint32 Height, bool bDrawGrid)
{
    if (!Meshasset || !Material)
    {
        return;
    }

    if (!Device || !Context)
    {
        return;
    }

    // 렌더타겟 크기 맞춤
    if (Width > 0 && Height > 0)
    {
        RenderTarget.Resize(Device.Get(), Width, Height);
    }
    if (!RenderTarget.IsValid() || RenderTarget.Width == 0 || RenderTarget.Height == 0)
    {
        return;
    }

    // 프리뷰 렌더타겟 바인딩
    ID3D11RenderTargetView* RTV = RenderTarget.RenderTargetView.Get();
    ID3D11DepthStencilView* DSV = RenderTarget.DepthStencilView.Get();
    Context->OMSetRenderTargets(1, &RTV, DSV);

    // 배경 및 깊이 버퍼 클리어
    const float ClearColor[4] = { 0.12f, 0.13f, 0.16f, 1.0f };
    Context->ClearRenderTargetView(RTV, ClearColor);
    Context->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    // 뷰포트 설정
    D3D11_VIEWPORT D3DVP = {};
    D3DVP.TopLeftX = 0.0f;
    D3DVP.TopLeftY = 0.0f;
    D3DVP.Width = static_cast<float>(RenderTarget.Width);
    D3DVP.Height = static_cast<float>(RenderTarget.Height);
    D3DVP.MinDepth = 0.0f;
    D3DVP.MaxDepth = 1.0f;
    Context->RSSetViewports(1, &D3DVP);

    // 조명 상수 버퍼 설정 및 바인딩
    FLightConstants LightConstants;
    LightConstants.LightDirection = FVector(-0.577f, -0.577f, -0.577f);
    LightConstants.Intensity = 1.2f;
    LightConstants.LightColor = FVector(1.0f, 1.0f, 1.0f);
    LightConstants.AmbientIntensity = 0.4f;
    SetRenderMode(EViewModeIndex::VMI_Lit);
    UpdateLightConstants(LightConstants, EViewModeIndex::VMI_Lit);


    FObjectConstants ObjConstants = {};
    ObjConstants.World = FMatrix::GetIdentity();
    ObjConstants.MVP = ObjConstants.World * Camera.CreateViewProjectionMatrix();
    ObjConstants.UVScale = FVector2(1.0f, 1.0f);
    ObjConstants.ColorOverride = FVector(1.0f, 1.0f, 1.0f);
    ObjConstants.ColorOverrideAmount = 0.0f;

    Draw(*Meshasset, *Material, ObjConstants, 0, -1, 0, false);

    // 그리드 렌더링
    if (bDrawGrid)
    {
        FGrid Grid;
        Grid.DrawLine(*this, Camera);

        const float Extent = (Meshasset->GetLocalBounds().Max - Meshasset->GetLocalBounds().Min).Size();
        FGridLineConstants GridConstants = {};
        GridConstants.MVP = Camera.CreateViewProjectionMatrix();
        GridConstants.CameraPosition = Camera.Position;
        GridConstants.FadeStartDistance = std::max(5.0f, Extent * 0.5f);
        GridConstants.FadeEndDistance = std::max(100.0f, Extent * 10.0f);
        FlushLineBatch(GridConstants, FName("Grid"));
    }

}

