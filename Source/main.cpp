#include "Editor/Application/FEditorApplication.h"
#include "Runtime/Core/Log.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/Engine/FRenderView.h"
#include "Runtime/Engine/FTimeManager.h"
#include "Runtime/Engine/UScene.h"
#include "Runtime/Engine/USceneManager.h"
#include "Runtime/Input/FInputManager.h"
#include "Runtime/Math/FVector2.h"
#include "Runtime/Rendering/FRenderResourceLibrary.h"
#include "Runtime/Rendering/FRenderer.h"
#include "ThirdParty/Imgui/imgui.h"
#include "ThirdParty/Imgui/imgui_internal.h"
#include <Windows.h>
#include <windowsx.h>

#include "ThirdParty/DirectXTK/Inc/DDSTextureLoader.h"
#include "ThirdParty/DirectXTK/Inc/WICTextureLoader.h"
#include <d3dcompiler.h>
#include <objbase.h>
#include "Runtime/Rendering/FObjDecoder.h"


extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg,
                                              WPARAM wParam, LPARAM lParam);

static bool bRequestNewScene = false;
static bool bRequestSaveScene = false;
static bool bRequestLoadScene = false;
static bool bRequestResize = false;
static UINT ResizeWidth = 0u;
static UINT ResizeHeight = 0u;

namespace {
constexpr LPCWSTR WindowName = L"My Engine";

HWND CreateWindowHandle(HINSTANCE Instance, HWND& OutSplashWnd);
bool ProcessWindowMessage();

LRESULT CALLBACK WindowCallback(HWND Window, UINT Message, WPARAM WParam,
                                LPARAM LParam);
} // namespace

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, 
    _In_ LPWSTR lpCmdLine, _In_ int nShowCmd) {
  struct FComScope {
    HRESULT Result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ~FComScope() { if (SUCCEEDED(Result)) CoUninitialize(); }
  } ComScope;
  HWND SplashWindow = nullptr;
  HWND Window = CreateWindowHandle(hInstance, SplashWindow);
  if (!Window) {
    return -1;
  }

  ShowWindow(Window, nShowCmd);

  FInputManager::Get();

  FRenderer Renderer;
  if (!Renderer.Initialize(Window)) {
    if (SplashWindow) DestroyWindow(SplashWindow);
    MessageBoxW(Window, L"Failed to initialize Direct3D.", L"Startup error", MB_OK | MB_ICONERROR);
    return -1;
  }
  FRenderView RenderView(Renderer);

  FRenderResourceLibrary &RenderResources = FRenderResourceLibrary::Get();
  if (!RenderResources.Initialize(Renderer)) {
    if (SplashWindow) DestroyWindow(SplashWindow);
    const auto& Logs = FLogManager::Get().GetLogs();
    const FString Detail = Logs.empty() ? "Resource initialization failed." : Logs.back();
    MessageBoxA(Window, Detail.c_str(), "Startup error", MB_OK | MB_ICONERROR);
    return -1;
  }

  // RTTI를 위한 UClass 초기화
  UClass::ResolveTypeBitsets();
  // 새씬 생성
  USceneManager SceneManager;


  SceneManager.SetScene(NewObject<UScene>());

  FEditorApplication &EditorApp = FEditorApplication::Get();
  {
    ID3D11Device *Device = nullptr;
    ID3D11DeviceContext *Context = nullptr;
    Renderer.GetDeviceAndContext_ImplDX11(Device, Context);
    EditorApp.Initialize_ImguiWin32DX11(Window, Device, Context);
  }

  EditorApp.Initialize_Runtime(&SceneManager, &RenderView);

  // 초기화가 끝났으니 로딩 화면을 닫고 메인 창을 띄운다
  if (SplashWindow) {
      DestroyWindow(SplashWindow);
      SplashWindow = nullptr;
  }
  ShowWindow(Window, nShowCmd);
  SetForegroundWindow(Window);


  bool bQuit = false;
  while (!bQuit) {
    FTimeManager::Get().Update();
    FTimeManager::Get().Resume();

    if (!ProcessWindowMessage()) {
      bQuit = true;
      break;
    }

    if (bRequestResize) {
      Renderer.OnWindowSize(ResizeWidth, ResizeHeight);
      EditorApp.OnWindowSize(ResizeWidth, ResizeHeight);
      bRequestResize = false;
    }

    FInputManager::Get().BeginFrame();
    EditorApp.Update(FTimeManager::Get().GetDeltaTime());

    Renderer.BeginFrame();
    EditorApp.Render();
    Renderer.SwapBuffer();

    //EditorApp.CollectGarbage();
  }

  EditorApp.Shutdown();
  SceneManager.Release();
  //EditorApp.CollectGarbage();
  Renderer.Shutdown();

  return 0;
}

namespace {

struct FWindowLayout { int X, Y, Width, Height; };  // Width/Height = 클라이언트 영역 기준

FWindowLayout GetWindowLayout() {
    constexpr int DesiredWidth = 1600;
    constexpr int DesiredHeight = 900;

    RECT Work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &Work, 0);

    // 메인 창의 캡션/테두리가 작업영역 밖으로 나가지 않도록 그만큼 안쪽으로 줄인다
    RECT Frame{ 0, 0, 0, 0 };
    AdjustWindowRectEx(&Frame, WS_OVERLAPPEDWINDOW, FALSE, 0);
    Work.left -= Frame.left;
    Work.top -= Frame.top;
    Work.right -= Frame.right;
    Work.bottom -= Frame.bottom;

    // 작업영역보다 크게 요청하면 작업영역에 맞춰 자른다
    const int MaxW = Work.right - Work.left;
    const int MaxH = Work.bottom - Work.top;
    const int W = (DesiredWidth < MaxW) ? DesiredWidth : MaxW;
    const int H = (DesiredHeight < MaxH) ? DesiredHeight : MaxH;

    // 남는 공간의 절반씩 → 중앙 배치
    return { Work.left + (MaxW - W) / 2, Work.top + (MaxH - H) / 2, W, H };
}

RECT ToWindowRect(const FWindowLayout& Layout, DWORD Style, DWORD ExStyle) {
    RECT R{ Layout.X, Layout.Y, Layout.X + Layout.Width, Layout.Y + Layout.Height };
    AdjustWindowRectEx(&R, Style, FALSE, ExStyle);   // 클라이언트 → 바깥 크기
    return R;
}



HWND ShowLoadingWindow(HINSTANCE hInstance)
{
    const auto ResourcesDir = GetResourcesDirectory();
    if (ResourcesDir.empty()) return nullptr;

    // DDS uses the GPU decoder; PNG/JPG use WIC. The splash is optional.
    const auto WindowLayout = GetWindowLayout();
    WNDCLASSW SplashClass{};
    SplashClass.lpfnWndProc = DefWindowProcW;
    SplashClass.hInstance = hInstance;
    SplashClass.lpszClassName = L"JungleSplash";
    RegisterClassW(&SplashClass);
    constexpr DWORD Style = WS_POPUP;
    constexpr DWORD ExStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
    const RECT Bounds = ToWindowRect(WindowLayout, Style, ExStyle);
    HWND Splash = CreateWindowExW(ExStyle, SplashClass.lpszClassName, L"", Style,
        Bounds.left, Bounds.top, Bounds.right - Bounds.left, Bounds.bottom - Bounds.top,
        nullptr, nullptr, hInstance, nullptr);
    if (!Splash) return nullptr;

    auto Fail = [&]() -> HWND { DestroyWindow(Splash); return nullptr; };
    try
    {
        DXGI_SWAP_CHAIN_DESC Desc{};
        Desc.BufferDesc.Width = WindowLayout.Width;
        Desc.BufferDesc.Height = WindowLayout.Height;
        Desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        Desc.SampleDesc.Count = 1;
        Desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        Desc.BufferCount = 1;
        Desc.OutputWindow = Splash;
        Desc.Windowed = TRUE;
        Microsoft::WRL::ComPtr<ID3D11Device> Device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> Context;
        Microsoft::WRL::ComPtr<IDXGISwapChain> SwapChain;
        if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE,
            nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &Desc, &SwapChain,
            &Device, nullptr, &Context))) return Fail();

        Microsoft::WRL::ComPtr<ID3D11Resource> Resource;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Image;
        for (const auto* Extension : { L".dds", L".png", L".jpg" })
        {
            const auto File = ResourcesDir / L"Textures" / (std::wstring(L"LoadingImage") + Extension);
            std::error_code Error;
            if (!std::filesystem::is_regular_file(File, Error)) continue;
            Resource.Reset();
            Image.Reset();
            const HRESULT Result = std::wstring_view(Extension) == L".dds"
                ? DirectX::CreateDDSTextureFromFile(Device.Get(), File.c_str(), &Resource, &Image)
                : DirectX::CreateWICTextureFromFile(Device.Get(), File.c_str(), &Resource, &Image);
            if (SUCCEEDED(Result)) break;
            UE_LOG_WARN("[Splash] Failed to load %s (HRESULT=0x%08lX)",
                File.string().c_str(), static_cast<unsigned long>(Result));
            Image.Reset();
        }
        if (!Image) return Fail();

        Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture;
        if (FAILED(Resource.As(&Texture))) return Fail();
        D3D11_TEXTURE2D_DESC ImageDesc{};
        Texture->GetDesc(&ImageDesc);
        Microsoft::WRL::ComPtr<ID3D11Texture2D> BackBuffer;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> Target;
        if (FAILED(SwapChain->GetBuffer(0, IID_PPV_ARGS(&BackBuffer)))
            || FAILED(Device->CreateRenderTargetView(BackBuffer.Get(), nullptr, &Target))) return Fail();
        Context->OMSetRenderTargets(1, Target.GetAddressOf(), nullptr);
        const float Black[] = { 0, 0, 0, 1 };
        Context->ClearRenderTargetView(Target.Get(), Black);
        D3D11_VIEWPORT Viewport{};
        Viewport.Width = static_cast<float>(WindowLayout.Width);
        Viewport.Height = static_cast<float>(WindowLayout.Height);
        Viewport.MaxDepth = 1;
        Context->RSSetViewports(1, &Viewport);
        const LONG Height = static_cast<LONG>(WindowLayout.Height * 0.3f);
        const LONG Width = static_cast<LONG>(static_cast<double>(Height) * ImageDesc.Width / ImageDesc.Height);
        const LONG Left = (WindowLayout.Width - Width) / 2;
        const LONG Top = (WindowLayout.Height - Height) / 2;
        const RECT Destination{ Left, Top, Left + Width, Top + Height };
        // Built-in splash shaders do not depend on the packaged Shader folder.
        constexpr char Shader[] = R"(
            struct VOut { float4 position : SV_Position; float2 uv : TEXCOORD0; };
            VOut VS(uint id : SV_VertexID) {
                VOut result;
                result.uv = float2((id << 1) & 2, id & 2);
                result.position = float4(result.uv * float2(2, -2) + float2(-1, 1), 0, 1);
                return result;
            }
            Texture2D image : register(t0);
            SamplerState imageSampler : register(s0);
            float4 PS(VOut input) : SV_Target { return image.Sample(imageSampler, input.uv); }
        )";
        Microsoft::WRL::ComPtr<ID3DBlob> VSCode, PSCode;
        if (FAILED(D3DCompile(Shader, sizeof(Shader), nullptr, nullptr, nullptr,
            "VS", "vs_4_0", 0, 0, &VSCode, nullptr))
            || FAILED(D3DCompile(Shader, sizeof(Shader), nullptr, nullptr, nullptr,
                "PS", "ps_4_0", 0, 0, &PSCode, nullptr))) return Fail();
        Microsoft::WRL::ComPtr<ID3D11VertexShader> VS;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> PS;
        if (FAILED(Device->CreateVertexShader(VSCode->GetBufferPointer(), VSCode->GetBufferSize(), nullptr, &VS))
            || FAILED(Device->CreatePixelShader(PSCode->GetBufferPointer(), PSCode->GetBufferSize(), nullptr, &PS))) return Fail();
        D3D11_SAMPLER_DESC SamplerDesc{};
        SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        SamplerDesc.AddressU = SamplerDesc.AddressV = SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        Microsoft::WRL::ComPtr<ID3D11SamplerState> Sampler;
        if (FAILED(Device->CreateSamplerState(&SamplerDesc, &Sampler))) return Fail();
        Viewport.TopLeftX = static_cast<float>(Destination.left);
        Viewport.TopLeftY = static_cast<float>(Destination.top);
        Viewport.Width = static_cast<float>(Width);
        Viewport.Height = static_cast<float>(Height);
        Context->RSSetViewports(1, &Viewport);
        Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        Context->VSSetShader(VS.Get(), nullptr, 0);
        Context->PSSetShader(PS.Get(), nullptr, 0);
        Context->PSSetSamplers(0, 1, Sampler.GetAddressOf());
        Context->PSSetShaderResources(0, 1, Image.GetAddressOf());
        Context->Draw(3, 0);
        ShowWindow(Splash, SW_SHOWNOACTIVATE);
        if (FAILED(SwapChain->Present(0, 0))) return Fail();
        return Splash;
    }
    catch (const std::exception& Error)
    {
        UE_LOG_WARN("[Splash] Skipped: %s", Error.what());
        return Fail();
    }
}

// TODO: Resizing 처리
HWND CreateWindowHandle(HINSTANCE Instance, HWND& OutSplashWnd) {
  WNDCLASS WindowClass{};
  WindowClass.lpfnWndProc = WindowCallback;

  WindowClass.hInstance = Instance;
  WindowClass.lpszClassName = L"MyEngine";

  if (!RegisterClass(&WindowClass) &&
      GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    return nullptr;
  }

  OutSplashWnd = ShowLoadingWindow(Instance);

  const FWindowLayout WindowLayout = GetWindowLayout();
  constexpr DWORD MainStyle = WS_OVERLAPPEDWINDOW;
  const RECT WindowRect = ToWindowRect(WindowLayout, MainStyle, 0);


  HWND Window = CreateWindowExW(0, WindowClass.lpszClassName, WindowName,
                                MainStyle, WindowRect.left, WindowRect.top,
                                WindowRect.right - WindowRect.left, 
                                WindowRect.bottom - WindowRect.top, 
                                nullptr, nullptr, Instance, nullptr);

  return Window;
}

// 닫아야되면 false 반환
bool ProcessWindowMessage() {
  MSG Message;
  while (PeekMessageW(&Message, nullptr, 0u, 0u, PM_REMOVE)) {
    if (Message.message == WM_QUIT) {
      return false;
    }

    TranslateMessage(&Message);
    DispatchMessageW(&Message);
  }

  return true;
}

LRESULT CALLBACK WindowCallback(HWND Window, UINT Message, WPARAM WParam,
                                LPARAM LParam) {
  if (LRESULT ImGuiResult = ImGui_ImplWin32_WndProcHandler(
          Window, Message, WParam, LParam)) // imgui의 프레임 스냅샷 상태를 갱신
    return ImGuiResult; // ImGuiResult != 0인 경우: 상태가 DefWindowProcW() 함수
                        // 동작을 오버라이드해야 하는 경우

  const FVector2 MousePos{static_cast<float>(GET_X_LPARAM(LParam)),
                          static_cast<float>(GET_Y_LPARAM(LParam))};
  const int16 WheelDelta = GET_WHEEL_DELTA_WPARAM(WParam);

  switch (Message) {
  case WM_DESTROY:
    PostQuitMessage(0);
    break;

  case WM_SIZE: {
    if (WParam != SIZE_MINIMIZED) {
      bRequestResize = true;
      ResizeWidth = LOWORD(LParam);
      ResizeHeight = HIWORD(LParam);
    }

    break;
  }

  case WM_LBUTTONDOWN:
    FInputManager::Get().OnMouseButtonDown(EMouseButton::Left, MousePos);
    SetCapture(Window);
    break;

  case WM_RBUTTONDOWN:
    FInputManager::Get().OnMouseButtonDown(EMouseButton::Right, MousePos);
    SetCapture(Window);
    break;

  case WM_MBUTTONDOWN:
    FInputManager::Get().OnMouseButtonDown(EMouseButton::Middle, MousePos);
    SetCapture(Window);
    break;

  case WM_LBUTTONUP:
    FInputManager::Get().OnMouseButtonUp(EMouseButton::Left, MousePos);
    ReleaseCapture();
    break;

  case WM_RBUTTONUP:
    FInputManager::Get().OnMouseButtonUp(EMouseButton::Right, MousePos);
    ReleaseCapture();
    break;

  case WM_MBUTTONUP:
    FInputManager::Get().OnMouseButtonUp(EMouseButton::Middle, MousePos);
    ReleaseCapture();
    break;

  case WM_MOUSEMOVE:
    FInputManager::Get().OnMouseMove(MousePos);
    break;

  case WM_MOUSEWHEEL:
    FInputManager::Get().OnMouseWheelScroll(WheelDelta);
    break;

  // case WM_CAPTURECHANGED:
  case WM_CANCELMODE:
  case WM_KILLFOCUS: {
    const FVector2 Last = FInputManager::Get().GetMousePosition();
    FInputManager::Get().OnMouseButtonUp(EMouseButton::Left, Last);
    FInputManager::Get().OnMouseButtonUp(EMouseButton::Right, Last);
    FInputManager::Get().OnMouseButtonUp(EMouseButton::Middle, Last);
    break;
  }
  // WM_CA
  break;
  case WM_KEYDOWN:
    switch (WParam) {
    case VK_F5:
      bRequestSaveScene = true;
      break;
    case VK_F6:
      bRequestLoadScene = true;
      break;
    case VK_F7:
      bRequestNewScene = true;
      break;
    }
    break;
  default:
    return DefWindowProc(Window, Message, WParam, LParam);
  }

  return 0;
}
} // namespace
