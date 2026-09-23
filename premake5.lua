-- premake5.lua
-- 사용법: premake5 vs2026   (VS2022라면 premake5 vs2022)

workspace "MyEngine"
    location "."
    configurations { "Debug", "Release", "ObjViewerDebug", "Analysis" }
    platforms { "x64", "Win32" }
    startproject "MyEngine"

    filter "platforms:x64"
        architecture "x86_64"
    filter "platforms:Win32"
        architecture "x86"
    filter {}

-- DirectXTK는 자체 vcxproj를 그대로 사용
externalproject "DirectXTK_Desktop_2026"
    location "Source/ThirdParty/DirectXTK"
    uuid "E0B52AE7-E160-4D32-BF3F-910B785E5A8E"
    kind "StaticLib"
    language "C++"
    configmap {
        ["ObjViewerDebug"] = "Debug",
        ["Analysis"]       = "Debug",
    }

project "MyEngine"
    location "."
    kind "WindowedApp"
    language "C++"
    cppdialect "C++20"
    characterset "Unicode"
    conformancemode "On"
    buildoptions { "/sdl" }
    multiprocessorcompile "On"
    warnings "Default"
    symbols "On"

    targetdir "Binaries/%{cfg.platform}/%{cfg.buildcfg}"
    objdir "Intermediate/%{prj.name}/%{cfg.platform}/%{cfg.buildcfg}"

    files {
        "Source/**.h",
        "Source/**.hpp",
        "Source/**.cpp",
        "Shader/**.hlsl",
        "Shader/**.hlsli",
        "Resources/MasterYi/MasterYi_HeadData.h",
    }
    removefiles { "Source/ThirdParty/DirectXTK/**" }

    includedirs {
        ".",
        "Source",
        "Source/ThirdParty/DirectXTK",
        "Source/ThirdParty/DirectXTK/Inc",
        "Source/ThirdParty/DirectXTK/Src",
    }

    defines { "NOMINMAX", "_CONSOLE" }
    buildoptions { "/utf-8", "/FS" }

    links {
        "user32", "d3d11", "dxgi", "d3dcompiler",
        "DirectXTK_Desktop_2026",
    }

    vsprops {
        EnableClangTidyCodeAnalysis = "true",
        ClangTidyChecks = "clang-analyzer-*",
        CodeAnalysisRuleSet = "CodeAnalysis.ruleset",
    }

    -- 서드파티 ImGui 경고 끄기
    filter "files:Source/ThirdParty/Imgui/**.cpp"
        warnings "Off"

    -- HLSL (Shader Model 5.0, 결과물은 $(OutDir)Shader\*.cso)
    filter "files:**.hlsl"
        shadermodel "5.0"
        shaderobjectfileoutput "$(OutDir)Shader\\%%(Filename).cso"
    filter "files:Shader/*PS.hlsl"
        shadertype "Pixel"
        shaderentry "MainPS"
    filter "files:Shader/*VS.hlsl"
        shadertype "Vertex"
        shaderentry "MainVS"
    filter "files:Shader/OutlinePostProcessPS.hlsl or Shader/RotationGizmoPS.hlsl or Shader/RotationGizmoVS.hlsl or Shader/ScreenQuadVS.hlsl"
        shaderentry "main"

    -- 구성별 설정
    filter "configurations:Debug or ObjViewerDebug or Analysis"
        defines { "_DEBUG" }
        runtime "Debug"
        optimize "Off"

    filter "configurations:Release"
        defines { "NDEBUG" }
        runtime "Release"
        optimize "Speed"
        functionlevellinking "On"
        intrinsics "On"
        linktimeoptimization "On"

    filter "configurations:ObjViewerDebug"
        defines { "IS_OBJ_VIEWER=1" }

    filter "configurations:Analysis"
        buildoptions { "/analyze" }
        vsprops { RunCodeAnalysis = "true" }

    filter "platforms:Win32"
        defines { "WIN32" }

    filter { "platforms:x64", "configurations:Debug or ObjViewerDebug" }
        forceincludes { "Runtime/Core/Log.h" }

    -- 빌드 전: png/jpg -> dds 변환
    filter { "platforms:x64", "configurations:Debug or ObjViewerDebug or Release" }
        prebuildmessage "Convert Png to dds"
        prebuildcommands { 'call "$(ProjectDir)ConvertTextures.bat"' }

    -- 빌드 후: 리소스 복사
    filter {}
        postbuildmessage "Copying resources to output directory..."
        postbuildcommands {
            'if exist "$(SolutionDir)Resources\\Textures" xcopy /Y /D /I /E "$(SolutionDir)Resources\\Textures\\" "$(OutDir)Textures\\"',
            'if exist "$(SolutionDir)Resources\\Edit" xcopy /Y /D /I /E "$(SolutionDir)Resources\\Edit\\" "$(OutDir)Edit\\"',
            'if exist "$(SolutionDir)Fonts" xcopy /Y /D /I /E "$(SolutionDir)Fonts\\" "$(OutDir)Fonts\\"',
            'xcopy /Y /D /I /E "$(ProjectDir)Resources" "$(OutDir)Resources"',
            'if errorlevel 1 exit /b 1',
        }
