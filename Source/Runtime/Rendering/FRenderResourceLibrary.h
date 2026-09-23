#pragma once

#include "FFont.h"
#include "FInstanceBatchKey.h"
#include "FMaterial.h"
#include "FMesh.h"
#include "FRenderPipeline.h"
#include "Runtime/Core/FString.h"
#include "Runtime/Core/FName.h"
#include "Runtime/Core/PointerTypes.h"
#include "Runtime/Core/TArray.h"
#include "Runtime/Core/TMap.h"
#include "Runtime/Engine/FDelegate.h"
#include <unordered_set>
#include "Vertices.h"

class FRenderer;
class FTexture;
class UStaticMesh;


struct FTextVertex {
    FVector Pos;
    float u, v;
};

struct FObjMaterialInfo
{
    // 모든 OBJ 머티리얼 이름은 전역적으로 유일하므로 이름을 등록 키로 사용한다.
    FString MaterialName; // Name -> MaterialName으로 통일

    FVector Ambient{ 0.2f, 0.2f, 0.2f };   // Ka 주변 색상
    FVector Diffuse{ 0.8f, 0.8f, 0.8f };   // Kd 확산 색상
    FVector Specular{ 0.0f, 0.0f, 0.0f };  // Ks 반사색
    FVector Emissive{ 0.0f, 0.0f, 0.0f };  // Ke 발광
    FVector TransmissionFilter{ 1.0f, 1.0f, 1.0f }; // Tf 투과 필터 색상
    float SpecularExponent = 0.0f;         // Ns 반사율
    float Opacity = 1.0f;                  // d  (Tr 는 1 - d) 투명성
    float OpticalDensity = 1.0f;           // Ni 굴절률
    int32 IlluminationModel = 0;           // illum 조명 모델

    FString DiffuseTextureName;         // map_Kd 디퓨즈 컬러 맵
    FString AmbientTextureName;         // map_Ka 주변 색상 맵
    FString SpecularTextureName;        // map_Ks 반사 색상 맵
    FString AlphaTextureName;           // map_d 알파 텍스처 맵
    FString NormalTextureName;          // map_bump / bump 범프 맵
    FString EmissiveTexture;            // map_Ke 발광 텍스처
    FString SpecularExponentTexture;    // map_Ns 반사광 하이라이트 구성 요소
    FString ReflectionTexture;          // refl 구형 반사 맵
    FString DisplacementTexture;        // disp 변위 맵
    FString DecalTexture;               // decal 스텐실 데칼 텍스처
};


class FRenderResourceLibrary final {
public:
    // 전역 싱글톤 접근자
    static FRenderResourceLibrary& Get();

    bool Initialize(FRenderer& Renderer);


  // 저장된 머티리얼 키를 넘겨주는 이벤트
  TMulticastDelegate<const FString&> OnMaterialSaved;



  // 파이프라인 보관 맵
  TMap<FName, TSharedPtr<FRenderPipeline>> AllPipelineMap;
  // 저수준 렌더 정적 메시 보관 맵
  TMap<FString, TSharedPtr<FStaticMesh>> AllFStaticMeshMap;
  // 게임 및 에디터용 UStaticMesh 에셋 보관 맵
  TMap<FString, UStaticMesh*> AllUStaticMeshMap;
  // 머티리얼 보관 맵 (FName 기반)
  TMap<FString, TSharedPtr<FMaterial>> AllMaterialMap;
  // 텍스쳐 보관 맵 (FName 기반)
  TMap<FName, TSharedPtr<FTexture>> AllTextureMap;
  // 폰트 보관 맵
  TMap<FName, TSharedPtr<FFont>> AllFontMap;

  // 현재 실행에서 처리한 MTL 파일 키
  TSet<FString> AllMaterialFileSet;

    // 에디터용 아이콘 텍스쳐 보관 맵
    TMap<FString, TSharedPtr<FTexture>> AllEditorTextureMap;

    // 스태틱 메시 썸네일 텍스처 보관 맵
    TMap<FName, TSharedPtr<FTexture>> AllMeshThumbnailMap;

  // 머터리얼 썸네일 텍스처 보관 맵
  TMap<FString, TSharedPtr<FTexture>> AllMaterialThumbnailMap;

  // 머터리얼 참조 메시 보관 맵
  TMap<FString, std::unordered_set<FString>> AllMaterialToMeshDependencyMap;





  // 전체 썸네일 맵 조회
  [[nodiscard]] const TMap<FName, TSharedPtr<FTexture>>& GetAllMeshThumbnailMap() const {
    return AllMeshThumbnailMap;
  }


  [[nodiscard]] const TMap<FString, TSharedPtr<FTexture>>& GetAllMaterialThumbnailMap() const {
      return AllMaterialThumbnailMap;
  }


  // 스태틱 메시 썸네일 조회
  [[nodiscard]] TSharedPtr<FTexture> GetMeshThumbnail(const FName& Id) const {
    auto it = AllMeshThumbnailMap.find(Id);
    if (it != AllMeshThumbnailMap.end())
      return it->second;
    return nullptr;
  }

  [[nodiscard]] TSharedPtr<FTexture> GetMaterialThumbnail(const FString& Id) const {
      auto it = AllMaterialThumbnailMap.find(Id);
      if (it != AllMaterialThumbnailMap.end())
          return it->second;
      return nullptr;
  }

  // 인스턴싱 배치 배열 맵
  TMap<FInstanceBatchKey, TArray<FInstanceData>> AllInstancingArrayMap;

    // 인스턴싱 배열 조회
    TArray<FInstanceData>& GetInstancingArray(const FName& MatId, const FName& MeshId) {
        return AllInstancingArrayMap[{MatId, MeshId}];
    }

    // 파이프라인 조회
    [[nodiscard]] TSharedPtr<FRenderPipeline> GetPipeline(const FName& Id) const {
        auto it = AllPipelineMap.find(Id);
        if (it != AllPipelineMap.end())
            return it->second;
        return nullptr;
    }

    // 머티리얼 조회TSharedPtr
    [[nodiscard]] TSharedPtr<FMaterial> GetMaterial(const FName& Id) const {
        auto it = AllMaterialMap.find(Id.ToString());
        if (it != AllMaterialMap.end())
            return it->second;
        return nullptr;
    }

    // 편집용 머티리얼 조회
    [[nodiscard]] TSharedPtr<FMaterial> GetEditMaterial(const FName& Id) const {
        auto it = AllMaterialMap.find(Id.ToString());
        if (it != AllMaterialMap.end())
            return it->second;
        return nullptr;
    }

    // 메쉬 조회
    TSharedPtr<FStaticMesh> GetMesh(const FName& ID) const {
        auto it = AllFStaticMeshMap.find(ID.ToString());
        if (it != AllFStaticMeshMap.end())
            return it->second;
        return nullptr;
    }

    // 메쉬 등록
    TSharedPtr<FStaticMesh> RegisterMesh(const FName& ID, TSharedPtr<FStaticMesh> inMesh) {
        inMesh->MeshId = ID;
        AllFStaticMeshMap[ID.ToString()] = inMesh;
        return inMesh;
    }


    TSharedPtr<FMaterial> CreateAndRegisterMaterialFromInfo(const FObjMaterialInfo& Info);
    UStaticMesh* CreateAndRegisterUStaticMesh(FName Key, TArray<FString>&& materials, TSharedPtr<FStaticMesh> fstaticmesh);

    // UStaticMesh 맵 조회
    [[nodiscard]] const TMap<FString, UStaticMesh*>& GetAllUStaticMeshMap() const {
        return AllUStaticMeshMap;
    }

    // UStaticMesh 맵 생성 함수
    bool CreateUStaticMeshMap();

    //Obj용 등록함수
    TSharedPtr<FStaticMesh> CreateStaticMesh(const FName& ID, const TArray<FVertexData>& Vertices, const TArray<uint32>& Indices);


  // material 참조하는 mesh 넣어주기
  void RegisterMeshMaterialDependency(FString MeshId, const FString& MaterialKey);

  // 머티리얼 키를 받아서 자기 자신 썸네일과 의존성이 걸린 메시 썸네일들을 한 번에 갱신
  void RefreshMaterialAndDependentThumbnails(const FString& InMaterialKey);


  void UpdateMeshMaterialDependency(const FString& MeshId, const FString& OldMatKey, const FString& NewMatKey);

  void UnregisterMeshMaterialDependency(const FString& MeshId, const FString& MaterialKey);

  // 개별 메쉬 접근자
  [[nodiscard]] TSharedPtr<FStaticMesh> GetCubeMesh() const {
    return GetMesh(FName("Cube"));
  }
  [[nodiscard]] TSharedPtr<FStaticMesh> GetCylinderMesh() const {
    return GetMesh(FName("Cylinder"));
  }
  [[nodiscard]] TSharedPtr<FStaticMesh> GetConeMesh() const {
    return GetMesh(FName("Cone"));
  }
  [[nodiscard]] TSharedPtr<FStaticMesh> GetSpotlightConeMesh() const {
    return GetMesh(FName("SpotlightCone"));
  }
  [[nodiscard]] TSharedPtr<FStaticMesh> GetArrowMesh() const {
    return GetMesh(FName("Arrow"));
  }
  [[nodiscard]] TSharedPtr<FStaticMesh> GetCircleMesh() const {
    return GetMesh(FName("Circle"));
  }
  [[nodiscard]] TSharedPtr<FStaticMesh> GetRotationGizmoMesh() const {
    return GetMesh(FName("RotGizmo"));
  }
  [[nodiscard]] TSharedPtr<FStaticMesh> GetSquareArrowMesh() const {
    return GetMesh(FName("SquareArrow"));
  }
  [[nodiscard]] TSharedPtr<FStaticMesh> GetGridMesh() const {
    return GetMesh(FName("Grid"));
  }
  [[nodiscard]] TSharedPtr<FStaticMesh> GetSphereMesh() const {
    return GetMesh(FName("Sphere"));
  }
  [[nodiscard]] TSharedPtr<FStaticMesh> GetLineMesh() const {
    return GetMesh(FName("Line"));
  }
  [[nodiscard]] TSharedPtr<FStaticMesh> GetPlaneMesh() const {
    return GetMesh(FName("Plane"));
  }
  [[nodiscard]] TSharedPtr<FStaticMesh> GetRectMesh() const {
    return GetMesh(FName("Rect"));
  }
  [[nodiscard]] TSharedPtr<FStaticMesh> GetTextMesh() const {
    return GetMesh(FName("TextMesh"));
  }
  [[nodiscard]] TSharedPtr<FStaticMesh> GetMasterYiMesh() const {
    return GetMesh(FName("MasterYi"));
  }

    // 머티리얼 등록
    TSharedPtr<FMaterial> RegisterMaterial(const FString& Id, TSharedPtr<FMaterial> inMaterial);

    void RegisterTexture(const FName& name, TSharedPtr<FTexture> texture) {
        AllTextureMap[name] = texture;
    }

    void RegisterEditTexture(const FString& name, TSharedPtr<FTexture> texture) {
        AllEditorTextureMap[name] = texture;
    }

    // 텍스처 조회
    [[nodiscard]] TSharedPtr<FTexture> GetTexture(const FName& name) const {
        auto it = AllTextureMap.find(name);
        if (it != AllTextureMap.end())
            return it->second;
        return nullptr;
    }

    [[nodiscard]] TSharedPtr<FTexture> GetEditTexture(const FString& name) const {
        auto it = AllEditorTextureMap.find(name);
        if (it != AllEditorTextureMap.end())
            return it->second;
        return nullptr;
    }


    [[nodiscard]] UStaticMesh* GetUStaticMesh(const FName& name) const
    {
        auto it = AllUStaticMeshMap.find(name.ToString());
        if (it != AllUStaticMeshMap.end())
            return it->second;
        return nullptr;
    }

    // 메쉬 전체 해제
    void DestroyAllMeshes() {
        AllFStaticMeshMap.clear();
        AllUStaticMeshMap.clear();
    }

    // 머티리얼 전체 해제
    void DestroyAllMaterials()
    {
        AllMaterialMap.clear();
        AllMaterialFileSet.clear();
    }

    // 파이프라인 전체 해제
    void DestroyAllPipelines() { AllPipelineMap.clear(); }

    void DestroyAllInstancingArray() { AllInstancingArrayMap.clear(); }

    // 전체 머티리얼 맵 조회
    const TMap<FString, TSharedPtr<FMaterial>>& GetAllMaterials() const {
        return AllMaterialMap;
    }

    // 렌더러 참조 조회
    FRenderer* GetRenderer() const { return RendererRef; }

    // 정점 배열 메쉬 캐싱 생성
    TSharedPtr<FStaticMesh> GetOrCreateMesh(const FName& ID,
        const TArray<FVertexData>& vertices);



    [[nodiscard]] TSharedPtr<FFont> GetFont(const FName& InName) const {
        auto it = AllFontMap.find(InName);
        if (it != AllFontMap.end())
            return it->second;
        return nullptr;
    }

  void UnregisterMaterial(const FString& InKey); //material 등록해제


  TSharedPtr<FStaticMesh> CreateAndRegisterStaticMesh(
      const FName& ID,
      const TArray<FVertexData>& Vertices,
      const TArray<uint32>& Indices
  );

  // 스태틱 메시 썸네일 일괄 생성
  bool CreateMeshThumbnails();
  // 머터리얼 썸네일 일괄 생성
  bool CreateMaterialThumbnails();

  void UpdateMaterialThumbnail(const FString& MatKey);
  void UpdateMeshThumbnail(const FString& MatKey);

  // 모든 obj 만드는 용도
  bool CreateObjMeshes();
private:
    bool InitializePipelines();
    bool CreateSolidWireframePipeline();
    bool CreateOutlinePipeline();
    bool CreatePostProcessPipeline();

    bool CreateCubeMesh();
    bool CreateCylinderMesh(float Height, uint32 SliceCount,
        float TopRadius, float BottomRadius);
    bool CreateConeMesh();
    bool CreateSpotlightConeMesh();
    bool CreateArrowMesh();
    bool CreateCircleMesh();
    bool CreateRotationGizmoMesh();
    bool CreateSquareArrowMesh();
    bool CreateGridMesh();
    bool CreateSphereMesh();
    bool CreateLineMesh();
    bool CreatePlaneMesh();
    bool CreateRectMesh();
    bool CreateMasterYiMesh();

    bool CreateInstancingArrayMap();

    // 텍스처 및 머티리얼 일괄 초기화
    bool CreateTextures();
    bool InitializeMaterials();
    bool CreateEditTextures();



    // 폰트 일괄 초기화
    bool CreateFonts();

    // Todo: Make as static const
    const char* OBJ_EXTENSION = ".obj";
    const char* MTL_EXTENSION = ".mtl";
    const char* BIN_EXTENSION = ".bin";

    FRenderer* RendererRef = nullptr;
};
