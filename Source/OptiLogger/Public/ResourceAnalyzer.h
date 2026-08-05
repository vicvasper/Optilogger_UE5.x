// ResourceAnalyzer.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/Texture.h"
#include "GameFramework/Actor.h"
#include "Components/LightComponent.h"
#include "ResourceAnalyzer.generated.h"

class APostProcessVolume;
class UAnimSequence;
class UMaterial;
class USkeletalMesh;
class USoundWave;
class UStaticMesh;
class UTexture2D;

// Estructura de datos para el análisis de Static Meshes
USTRUCT(BlueprintType)
struct FStaticMeshAnalysisData
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    FString AssetName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    int32 VertexCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    int32 TriangleCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    int32 LODCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    TArray<int32> LODVertexCounts;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    TArray<int32> LODTriangleCounts;

    // Every other member of these structs carries an initialiser; FVector and FLinearColor
    // default-construct uninitialised in Unreal, so omitting one here meant reading garbage
    // for any asset an analysis pass skipped or failed on.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    FVector BoundingBoxSize = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    float EstimatedMemoryUsageMB = 0.0f;
};

// Estructura de datos para el análisis de Skeletal Meshes
USTRUCT(BlueprintType)
struct FSkeletalMeshAnalysisData
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    FString AssetName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    int32 BoneCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    int32 VertexCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    int32 LODCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    FVector BoundingBoxSize = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    float EstimatedMemoryUsageMB = 0.0f;
};

// Estructura de datos para el análisis de Texturas
USTRUCT(BlueprintType)
struct FTextureAnalysisData
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    FString AssetName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    int32 Width = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    int32 Height = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    FString CompressionFormat;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    int32 MipLevels = 0;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    bool bIsVirtualTexture = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    float EstimatedMemoryUsageMB = 0.0f;
};

// Estructura de datos para el análisis de Materiales
USTRUCT(BlueprintType)
struct FMaterialAnalysisData
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    FString AssetName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    int32 TextureReferences = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    TArray<FString> ReferencedTextures;

    // --- Aquí añadimos los tres campos con UPROPERTY ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    int32 NumVertexShaderInstructions = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    int32 NumPixelShaderInstructions = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    int32 TotalShaderInstructions = 0;
    // -----------------------------------------------------

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    FString ComplexityLevel;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    bool bIsTranslucent = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    bool bIsMasked = false;
};

// Estructura de datos para el análisis de Animaciones
USTRUCT(BlueprintType)
struct FAnimationAnalysisData
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    FString AssetName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    float Duration = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    float FrameRate = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    int32 KeyframeCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    FString CompressionScheme;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    float EstimatedMemoryUsageMB = 0.0f;
};

// Estructura de datos para el análisis de Audio
USTRUCT(BlueprintType)
struct FAudioAnalysisData
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    FString AssetName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    float Duration = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    int32 SampleRate = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    int32 NumChannels = 0;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    FString CompressionFormat;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    int32 BitDepth = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    float EstimatedMemoryUsageMB = 0.0f;
};

// Estructura de datos para el análisis de Luces
USTRUCT(BlueprintType)
struct FLightingAnalysisData
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    FString LightName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    FString LightType;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    FString Mobility;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    float Intensity = 0.0f;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    FLinearColor LightColor = FLinearColor::Black;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    float AttenuationRadius = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    bool bCastShadows = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    bool bHasLightFunction = false;
};

// Estructura de datos para el análisis de Post-Procesado
USTRUCT(BlueprintType)
struct FPostProcessAnalysisData
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    FString VolumeName;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    bool bIsUnbound = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    float Priority = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    float BlendRadius = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    float BlendWeight = 0.0f;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
    TArray<FString> ActiveEffects;
};

// A hand-written OPTILOGGERRUNTIME_API macro used to live here, defined as __declspec of
// dllexport or dllimport. It was removed: UnrealBuildTool generates the <MODULE>_API macro for
// every module - which is where OPTILOGGER_API below comes from - so this one shadowed nothing,
// was referenced by no declaration, named a module that does not exist, and would not have
// compiled outside MSVC.

UCLASS()
class OPTILOGGER_API UResourceAnalyzer : public UObject
{
    GENERATED_BODY()

public:
    UResourceAnalyzer();

    void Initialize();
    void AnalyzeCurrentLevel(bool bFilterVisible = false);
    bool ExportAnalysisToJSON(const FString& FilePath = "");
    void ClearAnalysisResults();

    UWorld* GetAnalysisWorld() const;

    // Funciones de análisis individuales (ahora públicas para acceso granular si es necesario)
    void AnalyzeStaticMeshes(bool bFilterVisible = false);
    void AnalyzeSkeletalMeshes(bool bFilterVisible = false);
    void AnalyzeTextures(bool bFilterVisible = false);
    void AnalyzeMaterials(bool bFilterVisible = false);
    void AnalyzeAnimations(bool bFilterVisible = false);
    void AnalyzeAudio(bool bFilterVisible = false);
    void AnalyzeLighting(bool bFilterVisible = false);
    void AnalyzePostProcessEffects(bool bFilterVisible = false);

    // Getters para los resultados
    const TArray<FStaticMeshAnalysisData>& GetStaticMeshAnalysisResults() const { return StaticMeshAnalysisResults; }
    const TArray<FSkeletalMeshAnalysisData>& GetSkeletalMeshAnalysisResults() const { return SkeletalMeshAnalysisResults; }
    const TArray<FTextureAnalysisData>& GetTextureAnalysisResults() const { return TextureAnalysisResults; }
    const TArray<FMaterialAnalysisData>& GetMaterialAnalysisResults() const { return MaterialAnalysisResults; }
    const TArray<FAnimationAnalysisData>& GetAnimationAnalysisResults() const { return AnimationAnalysisResults; }
    const TArray<FAudioAnalysisData>& GetAudioAnalysisResults() const { return AudioAnalysisResults; }
    const TArray<FLightingAnalysisData>& GetLightingAnalysisResults() const { return LightingAnalysisResults; }
    const TArray<FPostProcessAnalysisData>& GetPostProcessAnalysisResults() const { return PostProcessAnalysisResults; }

private:
    // Per-asset analysis. Const: these read the asset and return a value, touching no state
    // on the analyzer itself.
    FStaticMeshAnalysisData AnalyzeStaticMeshAsset(UStaticMesh* StaticMesh) const;
    FSkeletalMeshAnalysisData AnalyzeSkeletalMeshAsset(USkeletalMesh* SkeletalMesh) const;
    FTextureAnalysisData AnalyzeTextureAsset(UTexture2D* Texture) const;
    FMaterialAnalysisData AnalyzeMaterialAsset(UMaterial* Material) const;
    FAnimationAnalysisData AnalyzeAnimationAsset(UAnimSequence* Animation) const;
    FAudioAnalysisData AnalyzeAudioAsset(USoundWave* SoundWave) const;
    FLightingAnalysisData AnalyzeLightComponent(ULightComponent* LightComponent) const;
    FPostProcessAnalysisData AnalyzePostProcessVolume(APostProcessVolume* PostProcessVolume) const;

    // Memory estimation. Static: pure functions of their arguments. See the constants block
    // in ResourceAnalyzer.cpp for what each figure assumes.
    static float EstimateTextureMemoryUsage(int32 Width, int32 Height, TextureCompressionSettings CompressionSettings, int32 MipLevels);
    static float EstimateStaticMeshMemoryUsage(int32 VertexCount, int32 TriangleCount);
    static float EstimateSkeletalMeshMemoryUsage(int32 VertexCount, int32 BoneCount);

    // JSON conversion
    TSharedPtr<FJsonObject> StaticMeshDataToJson(const FStaticMeshAnalysisData& Data) const;
    TSharedPtr<FJsonObject> SkeletalMeshDataToJson(const FSkeletalMeshAnalysisData& Data) const;
    TSharedPtr<FJsonObject> TextureDataToJson(const FTextureAnalysisData& Data) const;
    TSharedPtr<FJsonObject> MaterialDataToJson(const FMaterialAnalysisData& Data) const;
    TSharedPtr<FJsonObject> AnimationDataToJson(const FAnimationAnalysisData& Data) const;
    TSharedPtr<FJsonObject> AudioDataToJson(const FAudioAnalysisData& Data) const;
    TSharedPtr<FJsonObject> LightingDataToJson(const FLightingAnalysisData& Data) const;
    TSharedPtr<FJsonObject> PostProcessDataToJson(const FPostProcessAnalysisData& Data) const;

    // Enum-to-string helpers
    static FString GetCompressionFormatString(TextureCompressionSettings CompressionSettings);
    static FString GetMobilityString(EComponentMobility::Type Mobility);
    static FString GetLightTypeString(ULightComponent* LightComponent);

    /** True if any corner of the actor's bounds projects onto the active view. Ignores occlusion. */
    bool IsActorVisibleToCamera(AActor* Actor) const;

    // Almacenamiento de resultados
    TArray<FStaticMeshAnalysisData> StaticMeshAnalysisResults;
    TArray<FSkeletalMeshAnalysisData> SkeletalMeshAnalysisResults;
    TArray<FTextureAnalysisData> TextureAnalysisResults;
    TArray<FMaterialAnalysisData> MaterialAnalysisResults;
    TArray<FAnimationAnalysisData> AnimationAnalysisResults;
    TArray<FAudioAnalysisData> AudioAnalysisResults;
    TArray<FLightingAnalysisData> LightingAnalysisResults;
    TArray<FPostProcessAnalysisData> PostProcessAnalysisResults;
};