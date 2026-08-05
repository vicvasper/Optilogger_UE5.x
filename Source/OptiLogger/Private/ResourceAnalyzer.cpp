#include "ResourceAnalyzer.h"
#include "OptiLogger.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/LightComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Animation/AnimSequence.h"
#include "Sound/SoundWave.h"
#include "Engine/Light.h"
#include "Engine/PostProcessVolume.h"
#include "Animation/Skeleton.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "StaticMeshResources.h"
#include "Editor.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Animation/AnimCompress.h"
#include "Animation/AnimBoneCompressionSettings.h"
#include "Materials/MaterialInterface.h"
#include "RHI.h"
#include "Animation/AnimBlueprint.h" // Necesario para la nueva lógica de animaciones
#if WITH_EDITOR
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h" // Incluido por si acaso, aunque ya debería estarlo.
#include "LevelEditorViewport.h"
#endif
#include "Kismet/GameplayStatics.h"

class UAnimCompress;

UResourceAnalyzer::UResourceAnalyzer()
{
}

void UResourceAnalyzer::Initialize()
{
    ClearAnalysisResults();
    UE_LOG(LogOptiLogger, Log, TEXT("Optilogger: ResourceAnalyzer initialized successfully"));
}

void UResourceAnalyzer::AnalyzeCurrentLevel(bool bFilterVisible)
{
    UE_LOG(LogOptiLogger, Log, TEXT("Optilogger: Starting comprehensive level analysis. Filter Visible: %s"), bFilterVisible ? TEXT("True") : TEXT("False"));
    ClearAnalysisResults();

    UWorld* CurrentWorld = GetAnalysisWorld();
    if (!CurrentWorld)
    {
        UE_LOG(LogOptiLogger, Error, TEXT("Optilogger: FATAL - No valid world found for analysis. Cannot proceed."));
        return;
    }

    AnalyzeStaticMeshes(bFilterVisible);
    AnalyzeSkeletalMeshes(bFilterVisible);
    AnalyzeTextures(bFilterVisible);
    AnalyzeMaterials(bFilterVisible);
    AnalyzeAnimations(bFilterVisible);
    AnalyzeAudio(bFilterVisible);
    AnalyzeLighting(bFilterVisible);
    AnalyzePostProcessEffects(bFilterVisible);

    UE_LOG(LogOptiLogger, Log, TEXT("Optilogger: Level analysis finished. Check results. If empty, review previous logs."));
}

// Fragmento para ResourceAnalyzer.cpp (reemplaza la definición de GetAnalysisWorld())
UWorld* UResourceAnalyzer::GetAnalysisWorld() const
{
#if WITH_EDITOR
    if (GEditor)
    {
        if (GEditor->PlayWorld)
        {
            return GEditor->PlayWorld;
        }
        if (UWorld* EditorWorld = GEditor->GetEditorWorldContext().World())
        {
            return EditorWorld;
        }
    }
    UE_LOG(LogOptiLogger, Error, TEXT("Optilogger: GEditor is invalid or cannot provide a world."));
    return nullptr;
#else
    if (GEngine)
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::Game)
            {
                UE_LOG(LogOptiLogger, Log, TEXT("Optilogger: Using Game world for analysis"));
                return Context.World();
            }
        }
    }
    return nullptr;
#endif
}

void UResourceAnalyzer::ClearAnalysisResults()
{
    StaticMeshAnalysisResults.Empty();
    SkeletalMeshAnalysisResults.Empty();
    TextureAnalysisResults.Empty();
    MaterialAnalysisResults.Empty();
    AnimationAnalysisResults.Empty();
    AudioAnalysisResults.Empty();
    LightingAnalysisResults.Empty();
    PostProcessAnalysisResults.Empty();
}

// LÓGICA DE ANÁLISIS REFACTORIZADA

void UResourceAnalyzer::AnalyzeStaticMeshes(bool bFilterVisible)
{

    StaticMeshAnalysisResults.Empty();
    UWorld* World = GetAnalysisWorld(); if (!World) return;

    TSet<UStaticMesh*> FoundMeshes;
    int32 ActorCount = 0;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        ActorCount++;
        AActor* Actor = *It;
        if (!Actor) continue;

        if (bFilterVisible && !IsActorVisibleToCamera(Actor)) continue;
        
        TArray<UStaticMeshComponent*> Components;
        Actor->GetComponents<UStaticMeshComponent>(Components);
        for (UStaticMeshComponent* Comp : Components)
        {
            if (Comp && Comp->GetStaticMesh())
            {
                FoundMeshes.Add(Comp->GetStaticMesh());
            }
        }
    }

    if (ActorCount == 0) {
        UE_LOG(LogOptiLogger, Warning, TEXT("Optilogger: Found 0 actors in the world. Analysis will be empty."));
    }

    for (UStaticMesh* Mesh : FoundMeshes)
    {
        StaticMeshAnalysisResults.Add(AnalyzeStaticMeshAsset(Mesh));
    }
}

void UResourceAnalyzer::AnalyzeSkeletalMeshes(bool bFilterVisible)
{

    SkeletalMeshAnalysisResults.Empty();

    UWorld* World = GetAnalysisWorld();
    if (!World)
    {
        UE_LOG(LogOptiLogger, Error, TEXT("Optilogger: No valid world found for skeletal mesh analysis."));
        return;
    }

    TSet<USkeletalMesh*> FoundMeshes;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (bFilterVisible && !IsActorVisibleToCamera(*It)) continue;
        TArray<USkeletalMeshComponent*> Components;
        It->GetComponents<USkeletalMeshComponent>(Components);
        for (USkeletalMeshComponent* Component : Components)
        {
            if (Component && Component->GetSkeletalMeshAsset())
            {
                FoundMeshes.Add(Component->GetSkeletalMeshAsset());
            }
        }
    }

    for (USkeletalMesh* Mesh : FoundMeshes)
    {
        SkeletalMeshAnalysisResults.Add(AnalyzeSkeletalMeshAsset(Mesh));
    }
}

void UResourceAnalyzer::AnalyzeTextures(bool bFilterVisible)
{

    TextureAnalysisResults.Empty();

    UWorld* World = GetAnalysisWorld(); if (!World) { UE_LOG(LogOptiLogger, Error, TEXT("No valid world for texture analysis.")); return; }

    TSet<UTexture2D*> Found;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (bFilterVisible && !IsActorVisibleToCamera(*It)) continue;
        TArray<UMeshComponent*> Metro;
        It->GetComponents<UMeshComponent>(Metro);
        for (auto* MeshComp : Metro)
        {
            for (int32 i=0; i<MeshComp->GetNumMaterials(); ++i)
            {
                if (auto* Mat = MeshComp->GetMaterial(i))
                {
                    TArray<UTexture*> Textures;
                    Mat->GetUsedTextures(Textures, EMaterialQualityLevel::High, true, ERHIFeatureLevel::SM5, true);
                    for (auto* T : Textures)
                        if (auto* T2 = Cast<UTexture2D>(T)) Found.Add(T2);
                }
            }
        }
    }
    for (auto* Tex : Found)
        TextureAnalysisResults.Add(AnalyzeTextureAsset(Tex));
}

void UResourceAnalyzer::AnalyzeMaterials(bool bFilterVisible)
{

    MaterialAnalysisResults.Empty();

    UWorld* World = GetAnalysisWorld();
    if (!World)
    {
        UE_LOG(LogOptiLogger, Error, TEXT("Optilogger: No valid world found for material analysis."));
        return;
    }

    TSet<UMaterial*> FoundMaterials;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (bFilterVisible && !IsActorVisibleToCamera(*It)) continue;
        TArray<UMeshComponent*> Components;
        It->GetComponents<UMeshComponent>(Components);
        for (UMeshComponent* Component : Components)
        {
            if (Component)
            {
                TArray<UMaterialInterface*> Materials = Component->GetMaterials();
                for (UMaterialInterface* MatInterface : Materials)
                {
                    if (UMaterial* Mat = MatInterface ? MatInterface->GetBaseMaterial() : nullptr)
                    {
                        FoundMaterials.Add(Mat);
                    }
                }
            }
        }
    }

    for (UMaterial* Material : FoundMaterials)
    {
        MaterialAnalysisResults.Add(AnalyzeMaterialAsset(Material));
    }
}

// ***** FUNCIÓN CORREGIDA *****
void UResourceAnalyzer::AnalyzeAnimations(bool bFilterVisible)
{

    AnimationAnalysisResults.Empty();

    UWorld* World = GetAnalysisWorld();
    if (!World) { UE_LOG(LogOptiLogger, Error, TEXT("No valid world for animation analysis.")); return; }

    TSet<UAnimSequence*> FoundAnims;
    TSet<USkeleton*> SkeletonsInUse;

    // 1. Recolectar todos los esqueletos de los Skeletal Meshes presentes en el nivel.
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (bFilterVisible && !IsActorVisibleToCamera(*It)) continue;
        TArray<USkeletalMeshComponent*> Comps;
        It->GetComponents<USkeletalMeshComponent>(Comps);
        for (USkeletalMeshComponent* Comp : Comps)
        {
            if (!Comp) continue;

            // Añadir esqueleto del mesh principal
            if (USkeletalMesh* Mesh = Comp->GetSkeletalMeshAsset())
            {
                if (USkeleton* Skeleton = Mesh->GetSkeleton())
                {
                    SkeletonsInUse.Add(Skeleton);
                }
            }

            // Añadir animaciones directas (caso simple)
            if (UAnimSequence* Seq = Cast<UAnimSequence>(Comp->AnimationData.AnimToPlay.Get()))
            {
                FoundAnims.Add(Seq);
            }
        }
    }

    // 2. Usar el Asset Registry para encontrar TODAS las secuencias de animación que usan esos esqueletos.
    // Esto detecta animaciones usadas por Animation Blueprints de forma indirecta.
    if (SkeletonsInUse.Num() > 0)
    {
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        TArray<FAssetData> AssetData;
        
        FARFilter Filter;
        Filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
        
        AssetRegistryModule.Get().GetAssets(Filter, AssetData);

        for (const FAssetData& Asset : AssetData)
        {
            // La obtención del esqueleto desde FAssetData es más compleja y puede ser lenta.
            // Una forma más directa es cargar el asset si es necesario.
            if (UAnimSequence* AnimSeq = Cast<UAnimSequence>(Asset.GetAsset()))
            {
                if (AnimSeq->GetSkeleton() && SkeletonsInUse.Contains(AnimSeq->GetSkeleton()))
                {
                    FoundAnims.Add(AnimSeq);
                }
            }
        }
    }

    // 3. Analizar todas las animaciones encontradas.
    for (UAnimSequence* Seq : FoundAnims)
    {
        AnimationAnalysisResults.Add(AnalyzeAnimationAsset(Seq));
    }
}

void UResourceAnalyzer::AnalyzeAudio(bool bFilterVisible)
{

    AudioAnalysisResults.Empty();

    UWorld* World = GetAnalysisWorld();
    if (!World) { UE_LOG(LogOptiLogger, Error, TEXT("No valid world for audio analysis.")); return; }

    TSet<USoundWave*> Found;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (bFilterVisible && !IsActorVisibleToCamera(*It)) continue;
        TArray<UAudioComponent*> Comps;
        It->GetComponents<UAudioComponent>(Comps);
        for (UAudioComponent* Comp : Comps)
        {
            if (USoundBase* SB = Comp->Sound)
            {
                if (USoundWave* SW = Cast<USoundWave>(SB))
                {
                    Found.Add(SW);
                }
            }
        }
    }
    for (USoundWave* SW : Found)
    {
        AudioAnalysisResults.Add(AnalyzeAudioAsset(SW));
    }
}

void UResourceAnalyzer::AnalyzeLighting(bool bFilterVisible)
{

    LightingAnalysisResults.Empty();

    UWorld* World = GetAnalysisWorld(); if (!World) { UE_LOG(LogOptiLogger, Error, TEXT("No valid world for lighting analysis.")); return; }

    for (TActorIterator<ALight> It(World); It; ++It)
    {
        if (bFilterVisible && !IsActorVisibleToCamera(*It)) continue;
        if (auto* LC = It->GetLightComponent())
            LightingAnalysisResults.Add(AnalyzeLightComponent(LC));
    }
}

void UResourceAnalyzer::AnalyzePostProcessEffects(bool bFilterVisible)
{

    PostProcessAnalysisResults.Empty();

    UWorld* World = GetAnalysisWorld(); if (!World) { UE_LOG(LogOptiLogger, Error, TEXT("No valid world for post-process analysis.")); return; }

    for (TActorIterator<APostProcessVolume> It(World); It; ++It)
    {
        if (bFilterVisible && !IsActorVisibleToCamera(*It)) continue;
        PostProcessAnalysisResults.Add(AnalyzePostProcessVolume(*It));
    }
}

bool UResourceAnalyzer::IsActorVisibleToCamera(AActor* Actor) const
{
    if (!Actor) return false;

    UWorld* World = GetAnalysisWorld();
    if (!World) return false;

    FVector CamLoc;
    FRotator CamRot;
    float FOV = 90.0f; // Default FOV, can be adjusted
    FVector2D ViewportSize;

    // Get camera info
    APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
    if (PC)
    {
        PC->GetPlayerViewPoint(CamLoc, CamRot);
        if (GEngine && GEngine->GameViewport)
        {
            GEngine->GameViewport->GetViewportSize(ViewportSize);
        }
        FOV = PC->PlayerCameraManager->GetFOVAngle();
    }
    else
    {
#if WITH_EDITOR
        if (GEditor && GEditor->GetActiveViewport())
        {
            FEditorViewportClient* ViewportClient = static_cast<FEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient());
            if (ViewportClient)
            {
                CamLoc = ViewportClient->GetViewLocation();
                CamRot = ViewportClient->GetViewRotation();
                FOV = ViewportClient->FOVAngle;
                ViewportSize = FVector2D(ViewportClient->Viewport->GetSizeXY());
            }
            else
            {
                return false;
            }
        }
        else
#endif
        {
            return false;
        }
    }

    if (ViewportSize.X <= 0 || ViewportSize.Y <= 0) {
        ViewportSize = FVector2D(1920, 1080); // Fallback
    }

    // Get actor bounds
    FBoxSphereBounds Bounds = Actor->GetComponentsBoundingBox(true);
    FVector Origin = Bounds.Origin;
    FVector Extent = Bounds.BoxExtent;

    // Get 8 corners of the bounding box
    TArray<FVector> Corners;
    Corners.Add(Origin + FVector( Extent.X,  Extent.Y,  Extent.Z));
    Corners.Add(Origin + FVector( Extent.X,  Extent.Y, -Extent.Z));
    Corners.Add(Origin + FVector( Extent.X, -Extent.Y,  Extent.Z));
    Corners.Add(Origin + FVector( Extent.X, -Extent.Y, -Extent.Z));
    Corners.Add(Origin + FVector(-Extent.X,  Extent.Y,  Extent.Z));
    Corners.Add(Origin + FVector(-Extent.X,  Extent.Y, -Extent.Z));
    Corners.Add(Origin + FVector(-Extent.X, -Extent.Y,  Extent.Z));
    Corners.Add(Origin + FVector(-Extent.X, -Extent.Y, -Extent.Z));

    // Check if any corner is in front and projects on screen
    for (const FVector& Corner : Corners)
    {
        // Check if in front (dot product > 0)
        FVector ToCorner = Corner - CamLoc;
        ToCorner.Normalize();
        if (FVector::DotProduct(ToCorner, CamRot.Vector()) <= 0) continue; // Behind camera

        FVector2D ScreenPos;
        if (PC)
        {
            if (UGameplayStatics::ProjectWorldToScreen(PC, Corner, ScreenPos))
            {
                float NormX = ScreenPos.X / ViewportSize.X;
                float NormY = ScreenPos.Y / ViewportSize.Y;
                if (NormX >= 0.0f && NormX <= 1.0f && NormY >= 0.0f && NormY <= 1.0f)
                {
                    return true;
                }
            }
        }
        else
        {
            // Manual projection for Editor
            FMatrix ViewMatrix = FRotationTranslationMatrix(CamRot, CamLoc).Inverse();
            FMatrix ProjMatrix = FPerspectiveMatrix(FMath::DegreesToRadians(FOV), ViewportSize.X / ViewportSize.Y, 0.1f, 10000.0f);

            FVector4 ClipSpace = ProjMatrix.TransformFVector4(ViewMatrix.TransformPosition(Corner));
            if (ClipSpace.W > 0.0f) // In front
            {
                float NormX = (ClipSpace.X / ClipSpace.W + 1.0f) / 2.0f;
                float NormY = 1.0f - (ClipSpace.Y / ClipSpace.W + 1.0f) / 2.0f; // Flip Y
                if (NormX >= 0.0f && NormX <= 1.0f && NormY >= 0.0f && NormY <= 1.0f)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

FStaticMeshAnalysisData UResourceAnalyzer::AnalyzeStaticMeshAsset(UStaticMesh* StaticMesh)
{
    FStaticMeshAnalysisData AnalysisData;
    
    if (!StaticMesh)
    {
        return AnalysisData;
    }
    
    AnalysisData.AssetName = StaticMesh->GetName();
    
    if (StaticMesh->GetRenderData() && StaticMesh->GetRenderData()->LODResources.Num() > 0)
    {
        const FStaticMeshLODResources& LODResource = StaticMesh->GetRenderData()->LODResources[0];
        AnalysisData.VertexCount = LODResource.GetNumVertices();
        AnalysisData.TriangleCount = LODResource.GetNumTriangles();
        
        AnalysisData.LODCount = StaticMesh->GetRenderData()->LODResources.Num();
        for (int32 LODIndex = 0; LODIndex < AnalysisData.LODCount; LODIndex++)
        {
            const FStaticMeshLODResources& CurrentLOD = StaticMesh->GetRenderData()->LODResources[LODIndex];
            AnalysisData.LODVertexCounts.Add(CurrentLOD.GetNumVertices());
            AnalysisData.LODTriangleCounts.Add(CurrentLOD.GetNumTriangles());
        }
    }
    
    FBoxSphereBounds Bounds = StaticMesh->GetBounds();
    AnalysisData.BoundingBoxSize = Bounds.BoxExtent * 2.0f;
    
    AnalysisData.EstimatedMemoryUsageMB = EstimateStaticMeshMemoryUsage(AnalysisData.VertexCount, AnalysisData.TriangleCount);
    
    return AnalysisData;
}

FSkeletalMeshAnalysisData UResourceAnalyzer::AnalyzeSkeletalMeshAsset(USkeletalMesh* SkeletalMesh)
{
    FSkeletalMeshAnalysisData AnalysisData;
    
    if (!SkeletalMesh)
    {
        return AnalysisData;
    }
    
    AnalysisData.AssetName = SkeletalMesh->GetName();
    
    if (USkeleton* Skeleton = SkeletalMesh->GetSkeleton())
    {
        AnalysisData.BoneCount = Skeleton->GetReferenceSkeleton().GetNum();
    }
    
    if (SkeletalMesh->GetResourceForRendering() && SkeletalMesh->GetResourceForRendering()->LODRenderData.Num() > 0)
    {
        const FSkeletalMeshLODRenderData& LODData = SkeletalMesh->GetResourceForRendering()->LODRenderData[0];
        AnalysisData.VertexCount = LODData.GetNumVertices();
        AnalysisData.LODCount = SkeletalMesh->GetResourceForRendering()->LODRenderData.Num();
    }
    
    FBoxSphereBounds Bounds = SkeletalMesh->GetBounds();
    AnalysisData.BoundingBoxSize = Bounds.BoxExtent * 2.0f;
    
    AnalysisData.EstimatedMemoryUsageMB = EstimateSkeletalMeshMemoryUsage(AnalysisData.VertexCount, AnalysisData.BoneCount);
    
    return AnalysisData;
}

FTextureAnalysisData UResourceAnalyzer::AnalyzeTextureAsset(UTexture2D* Texture)
{
    FTextureAnalysisData AnalysisData;
    
    if (!Texture)
    {
        return AnalysisData;
    }
    
    AnalysisData.AssetName = Texture->GetName();
    AnalysisData.Width = Texture->GetSizeX();
    AnalysisData.Height = Texture->GetSizeY();
    AnalysisData.CompressionFormat = GetCompressionFormatString(Texture->CompressionSettings);
    AnalysisData.MipLevels = Texture->GetNumMips();
    AnalysisData.bIsVirtualTexture = Texture->VirtualTextureStreaming;
    
    AnalysisData.EstimatedMemoryUsageMB = EstimateTextureMemoryUsage(
        AnalysisData.Width, 
        AnalysisData.Height, 
        Texture->CompressionSettings, 
        AnalysisData.MipLevels
    );
    
    return AnalysisData;
}

// ***** FUNCIÓN CORREGIDA *****
FMaterialAnalysisData UResourceAnalyzer::AnalyzeMaterialAsset(UMaterial* Material)
{
    FMaterialAnalysisData AnalysisData;
    if (!Material)
    {
        return AnalysisData;
    }

    // Datos básicos del material
    AnalysisData.AssetName = Material->GetName();
    AnalysisData.bIsTranslucent = Material->GetBlendMode() != BLEND_Opaque;
    AnalysisData.bIsMasked = Material->GetBlendMode() == BLEND_Masked;

    // Obtener las texturas referenciadas
    TArray<UTexture*> UsedTextures;
    Material->GetUsedTextures(UsedTextures, EMaterialQualityLevel::Num, true, GMaxRHIFeatureLevel, true);
    
    AnalysisData.TextureReferences = UsedTextures.Num();
    AnalysisData.ReferencedTextures.Empty(); // Limpiamos por si acaso
    for (UTexture* Tex : UsedTextures)
    {
        if (Tex)
        {
            AnalysisData.ReferencedTextures.Add(Tex->GetName());
        }
    }

#if WITH_EDITOR
    // --- Obtenemos las estadísticas de instrucciones usando el método preferido ---
    const FMaterialStatistics Stats = UMaterialEditingLibrary::GetStatistics(Material);

    AnalysisData.NumVertexShaderInstructions = Stats.NumVertexShaderInstructions;
    AnalysisData.NumPixelShaderInstructions  = Stats.NumPixelShaderInstructions;
    AnalysisData.TotalShaderInstructions =
        AnalysisData.NumVertexShaderInstructions +
        AnalysisData.NumPixelShaderInstructions;
#else
    // En builds de runtime (no-editor), la información no está disponible.
    AnalysisData.NumVertexShaderInstructions = 0;
    AnalysisData.NumPixelShaderInstructions  = 0;
    AnalysisData.TotalShaderInstructions     = 0;
#endif

    // Calcular el nivel de complejidad basado en las instrucciones
    if (AnalysisData.TotalShaderInstructions < 100)
    {
        AnalysisData.ComplexityLevel = TEXT("Low");
    }
    else if (AnalysisData.TotalShaderInstructions < 300)
    {
        AnalysisData.ComplexityLevel = TEXT("Medium");
    }
    else
    {
        AnalysisData.ComplexityLevel = TEXT("High");
    }

    return AnalysisData;
}

FAnimationAnalysisData UResourceAnalyzer::AnalyzeAnimationAsset(UAnimSequence* Animation)
{
    FAnimationAnalysisData AnalysisData;
    
    if (!Animation)
    {
        return AnalysisData;
    }
    
    AnalysisData.AssetName       = Animation->GetName();
    AnalysisData.Duration        = Animation->GetPlayLength();
    AnalysisData.FrameRate       = Animation->GetSamplingFrameRate().AsDecimal();
    AnalysisData.KeyframeCount   = FMath::RoundToInt(AnalysisData.Duration * AnalysisData.FrameRate);
    
    if (UAnimBoneCompressionSettings* BoneSettings = Animation->BoneCompressionSettings)
    {
        AnalysisData.CompressionScheme = BoneSettings->GetClass()->GetName();
    }
    else
    {
        AnalysisData.CompressionScheme = TEXT("None");
    }
    
    AnalysisData.EstimatedMemoryUsageMB = (AnalysisData.KeyframeCount * 100) / (1024.0f * 1024.0f);
    
    return AnalysisData;
}

FAudioAnalysisData UResourceAnalyzer::AnalyzeAudioAsset(USoundWave* SoundWave)
{
    FAudioAnalysisData AnalysisData;
    
    if (!SoundWave)
    {
        return AnalysisData;
    }
    
    AnalysisData.AssetName = SoundWave->GetName();
    AnalysisData.Duration = SoundWave->Duration;
    AnalysisData.SampleRate = SoundWave->GetSampleRateForCurrentPlatform();
    AnalysisData.NumChannels = SoundWave->NumChannels;
    
    if (SoundWave->IsStreaming())
    {
        AnalysisData.CompressionFormat = TEXT("Streaming (OGG Vorbis)");
    }
    else
    {
        AnalysisData.CompressionFormat = TEXT("PCM");
    }
    
    AnalysisData.BitDepth = 16;
    
    float UncompressedSize = AnalysisData.Duration * AnalysisData.SampleRate * AnalysisData.NumChannels * (AnalysisData.BitDepth / 8.0f);
    AnalysisData.EstimatedMemoryUsageMB = UncompressedSize / (1024.0f * 1024.0f);
    
    return AnalysisData;
}

FLightingAnalysisData UResourceAnalyzer::AnalyzeLightComponent(ULightComponent* LightComponent)
{
    FLightingAnalysisData AnalysisData;
    
    if (!LightComponent)
    {
        return AnalysisData;
    }
    
    AnalysisData.LightName = LightComponent->GetOwner() ? LightComponent->GetOwner()->GetName() : TEXT("Unknown");
    AnalysisData.LightType = GetLightTypeString(LightComponent);
    AnalysisData.Mobility = GetMobilityString(LightComponent->Mobility);
    AnalysisData.Intensity = LightComponent->Intensity;
    AnalysisData.LightColor = LightComponent->LightColor;
    AnalysisData.bCastShadows = LightComponent->CastShadows;
    AnalysisData.bHasLightFunction = LightComponent->LightFunctionMaterial != nullptr;
    
    if (UPointLightComponent* PointLight = Cast<UPointLightComponent>(LightComponent))
    {
        AnalysisData.AttenuationRadius = PointLight->AttenuationRadius;
    }
    else if (USpotLightComponent* SpotLight = Cast<USpotLightComponent>(LightComponent))
    {
        AnalysisData.AttenuationRadius = SpotLight->AttenuationRadius;
    }
    
    return AnalysisData;
}

FPostProcessAnalysisData UResourceAnalyzer::AnalyzePostProcessVolume(APostProcessVolume* PostProcessVolume)
{
    FPostProcessAnalysisData AnalysisData;
    
    if (!PostProcessVolume)
    {
        return AnalysisData;
    }
    
    AnalysisData.VolumeName = PostProcessVolume->GetName();
    AnalysisData.bIsUnbound = PostProcessVolume->bUnbound;
    AnalysisData.Priority = PostProcessVolume->Priority;
    AnalysisData.BlendRadius = PostProcessVolume->BlendRadius;
    AnalysisData.BlendWeight = PostProcessVolume->BlendWeight;
    
    const FPostProcessSettings& Settings = PostProcessVolume->Settings;
    
    if (Settings.bOverride_BloomIntensity && Settings.BloomIntensity > 0.0f)
    {
        AnalysisData.ActiveEffects.Add(TEXT("Bloom"));
    }
    if (Settings.bOverride_AutoExposureMinBrightness || Settings.bOverride_AutoExposureMaxBrightness)
    {
        AnalysisData.ActiveEffects.Add(TEXT("Auto Exposure"));
    }
    if (Settings.bOverride_ColorSaturation)
    {
        AnalysisData.ActiveEffects.Add(TEXT("Color Grading"));
    }
    if (Settings.bOverride_VignetteIntensity && Settings.VignetteIntensity > 0.0f)
    {
        AnalysisData.ActiveEffects.Add(TEXT("Vignette"));
    }
    
    return AnalysisData;
}

bool UResourceAnalyzer::ExportAnalysisToJSON(const FString& FilePath)
{
    UE_LOG(LogOptiLogger, Log, TEXT("Optilogger: Exporting analysis results to JSON"));
    
    TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);
    
    RootObject->SetStringField(TEXT("ExportDate"), FDateTime::Now().ToString());
    RootObject->SetStringField(TEXT("PluginVersion"), TEXT("1.0"));
    RootObject->SetStringField(TEXT("EngineVersion"), FEngineVersion::Current().ToString());
    
    TArray<TSharedPtr<FJsonValue>> StaticMeshArray;
    for (const FStaticMeshAnalysisData& Data : StaticMeshAnalysisResults)
    {
        StaticMeshArray.Add(MakeShareable(new FJsonValueObject(StaticMeshDataToJson(Data))));
    }
    RootObject->SetArrayField(TEXT("StaticMeshes"), StaticMeshArray);
    
    TArray<TSharedPtr<FJsonValue>> SkeletalMeshArray;
    for (const FSkeletalMeshAnalysisData& Data : SkeletalMeshAnalysisResults)
    {
        SkeletalMeshArray.Add(MakeShareable(new FJsonValueObject(SkeletalMeshDataToJson(Data))));
    }
    RootObject->SetArrayField(TEXT("SkeletalMeshes"), SkeletalMeshArray);
    
    TArray<TSharedPtr<FJsonValue>> TextureArray;
    for (const FTextureAnalysisData& Data : TextureAnalysisResults)
    {
        TextureArray.Add(MakeShareable(new FJsonValueObject(TextureDataToJson(Data))));
    }
    RootObject->SetArrayField(TEXT("Textures"), TextureArray);
    
    TArray<TSharedPtr<FJsonValue>> MaterialArray;
    for (const FMaterialAnalysisData& Data : MaterialAnalysisResults)
    {
        MaterialArray.Add(MakeShareable(new FJsonValueObject(MaterialDataToJson(Data))));
    }
    RootObject->SetArrayField(TEXT("Materials"), MaterialArray);
    
    TArray<TSharedPtr<FJsonValue>> AnimationArray;
    for (const FAnimationAnalysisData& Data : AnimationAnalysisResults)
    {
        AnimationArray.Add(MakeShareable(new FJsonValueObject(AnimationDataToJson(Data))));
    }
    RootObject->SetArrayField(TEXT("Animations"), AnimationArray);
    
    TArray<TSharedPtr<FJsonValue>> AudioArray;
    for (const FAudioAnalysisData& Data : AudioAnalysisResults)
    {
        AudioArray.Add(MakeShareable(new FJsonValueObject(AudioDataToJson(Data))));
    }
    RootObject->SetArrayField(TEXT("Audio"), AudioArray);
    
    TArray<TSharedPtr<FJsonValue>> LightingArray;
    for (const FLightingAnalysisData& Data : LightingAnalysisResults)
    {
        LightingArray.Add(MakeShareable(new FJsonValueObject(LightingDataToJson(Data))));
    }
    RootObject->SetArrayField(TEXT("Lighting"), LightingArray);
    
    TArray<TSharedPtr<FJsonValue>> PostProcessArray;
    for (const FPostProcessAnalysisData& Data : PostProcessAnalysisResults)
    {
        PostProcessArray.Add(MakeShareable(new FJsonValueObject(PostProcessDataToJson(Data))));
    }
    RootObject->SetArrayField(TEXT("PostProcess"), PostProcessArray);
    
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);
    
    FString FinalFilePath = FilePath;
    if (FinalFilePath.IsEmpty())
    {
        FString SavedDir = FPaths::ProjectSavedDir();
        FString ReportDir = FPaths::Combine(SavedDir, TEXT("ResourceReport"));
        
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        if (!PlatformFile.DirectoryExists(*ReportDir))
        {
            PlatformFile.CreateDirectoryTree(*ReportDir);
        }
        
        FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
        FinalFilePath = FPaths::Combine(ReportDir, FString::Printf(TEXT("OptiloggerReport_%s.json"), *Timestamp));
    }
    
    bool bSuccess = FFileHelper::SaveStringToFile(OutputString, *FinalFilePath);
    
    if (bSuccess)
    {
        UE_LOG(LogOptiLogger, Log, TEXT("Optilogger: Analysis report exported to: %s"), *FinalFilePath);
    }
    else
    {
        UE_LOG(LogOptiLogger, Error, TEXT("Optilogger: Failed to export analysis report to: %s"), *FinalFilePath);
    }
    
    return bSuccess;
}

// Utility function implementations
FString UResourceAnalyzer::GetCompressionFormatString(TextureCompressionSettings CompressionSettings)
{
    switch (CompressionSettings)
    {
        case TC_Default: return TEXT("Default");
        case TC_Normalmap: return TEXT("Normal Map");
        case TC_Masks: return TEXT("Masks");
        case TC_Grayscale: return TEXT("Grayscale");
        case TC_Displacementmap: return TEXT("Displacement Map");
        case TC_VectorDisplacementmap: return TEXT("Vector Displacement Map");
        case TC_HDR: return TEXT("HDR");
        case TC_EditorIcon: return TEXT("Editor Icon");
        case TC_Alpha: return TEXT("Alpha");
        case TC_DistanceFieldFont: return TEXT("Distance Field Font");
        case TC_HDR_Compressed: return TEXT("HDR Compressed");
        case TC_BC7: return TEXT("BC7");
        default: return TEXT("Unknown");
    }
}

FString UResourceAnalyzer::GetMobilityString(EComponentMobility::Type Mobility)
{
    switch (Mobility)
    {
        case EComponentMobility::Static: return TEXT("Static");
        case EComponentMobility::Stationary: return TEXT("Stationary");
        case EComponentMobility::Movable: return TEXT("Movable");
        default: return TEXT("Unknown");
    }
}

FString UResourceAnalyzer::GetLightTypeString(ULightComponent* LightComponent)
{
    if (Cast<UDirectionalLightComponent>(LightComponent))
    {
        return TEXT("Directional Light");
    }
    else if (Cast<USpotLightComponent>(LightComponent))
    {
        return TEXT("Spot Light");
    }
    else if (Cast<UPointLightComponent>(LightComponent))
    {
        return TEXT("Point Light");
    }
    else
    {
        return TEXT("Unknown Light Type");
    }
}

float UResourceAnalyzer::EstimateTextureMemoryUsage(int32 Width, int32 Height, TextureCompressionSettings CompressionSettings, int32 MipLevels)
{
    float BaseSize = Width * Height * 4;
    
    float CompressionFactor = 1.0f;
    switch (CompressionSettings)
    {
        case TC_Default:
        case TC_BC7:
            CompressionFactor = 0.25f;
            break;
        case TC_Normalmap:
            CompressionFactor = 0.5f;
            break;
        case TC_Grayscale:
        case TC_Alpha:
            CompressionFactor = 0.25f;
            break;
        case TC_HDR:
        case TC_HDR_Compressed:
            CompressionFactor = 2.0f;
            break;
        default:
            CompressionFactor = 0.5f;
            break;
    }
    
    float MipFactor = MipLevels > 1 ? 1.33f : 1.0f;
    
    float TotalSize = BaseSize * CompressionFactor * MipFactor;
    return TotalSize / (1024.0f * 1024.0f);
}

float UResourceAnalyzer::EstimateStaticMeshMemoryUsage(int32 VertexCount, int32 TriangleCount)
{
    float VertexSize = VertexCount * 32;
    float IndexSize = TriangleCount * 3 * 4;
    
    float TotalSize = VertexSize + IndexSize;
    return TotalSize / (1024.0f * 1024.0f);
}

float UResourceAnalyzer::EstimateSkeletalMeshMemoryUsage(int32 VertexCount, int32 BoneCount)
{
    float VertexSize = VertexCount * 48;
    float BoneSize = BoneCount * 64;
    
    float TotalSize = VertexSize + BoneSize;
    return TotalSize / (1024.0f * 1024.0f);
}

// JSON conversion helper functions
TSharedPtr<FJsonObject> UResourceAnalyzer::StaticMeshDataToJson(const FStaticMeshAnalysisData& Data)
{
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
    
    JsonObject->SetStringField(TEXT("AssetName"), Data.AssetName);
    JsonObject->SetNumberField(TEXT("VertexCount"), Data.VertexCount);
    JsonObject->SetNumberField(TEXT("TriangleCount"), Data.TriangleCount);
    JsonObject->SetNumberField(TEXT("LODCount"), Data.LODCount);
    JsonObject->SetNumberField(TEXT("EstimatedMemoryUsageMB"), Data.EstimatedMemoryUsageMB);
    
    TArray<TSharedPtr<FJsonValue>> LODVertexArray;
    for (int32 Count : Data.LODVertexCounts)
    {
        LODVertexArray.Add(MakeShareable(new FJsonValueNumber(Count)));
    }
    JsonObject->SetArrayField(TEXT("LODVertexCounts"), LODVertexArray);
    
    return JsonObject;
}

TSharedPtr<FJsonObject> UResourceAnalyzer::SkeletalMeshDataToJson(const FSkeletalMeshAnalysisData& Data)
{
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
    
    JsonObject->SetStringField(TEXT("AssetName"), Data.AssetName);
    JsonObject->SetNumberField(TEXT("BoneCount"), Data.BoneCount);
    JsonObject->SetNumberField(TEXT("VertexCount"), Data.VertexCount);
    JsonObject->SetNumberField(TEXT("LODCount"), Data.LODCount);
    JsonObject->SetNumberField(TEXT("EstimatedMemoryUsageMB"), Data.EstimatedMemoryUsageMB);
    
    return JsonObject;
}

TSharedPtr<FJsonObject> UResourceAnalyzer::TextureDataToJson(const FTextureAnalysisData& Data)
{
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
    
    JsonObject->SetStringField(TEXT("AssetName"), Data.AssetName);
    JsonObject->SetNumberField(TEXT("Width"), Data.Width);
    JsonObject->SetNumberField(TEXT("Height"), Data.Height);
    JsonObject->SetStringField(TEXT("CompressionFormat"), Data.CompressionFormat);
    JsonObject->SetNumberField(TEXT("EstimatedMemoryUsageMB"), Data.EstimatedMemoryUsageMB);
    JsonObject->SetNumberField(TEXT("MipLevels"), Data.MipLevels);
    JsonObject->SetBoolField(TEXT("IsVirtualTexture"), Data.bIsVirtualTexture);
    
    return JsonObject;
}

TSharedPtr<FJsonObject> UResourceAnalyzer::MaterialDataToJson(const FMaterialAnalysisData& Data)
{
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
    
    JsonObject->SetStringField(TEXT("AssetName"), Data.AssetName);
    JsonObject->SetNumberField(TEXT("TextureReferences"), Data.TextureReferences);
    JsonObject->SetNumberField(TEXT("VertexShaderInstructions"), Data.NumVertexShaderInstructions);
    JsonObject->SetNumberField(TEXT("PixelShaderInstructions"),  Data.NumPixelShaderInstructions);
    JsonObject->SetNumberField(TEXT("TotalShaderInstructions"),  Data.TotalShaderInstructions);
    JsonObject->SetStringField(TEXT("ComplexityLevel"), Data.ComplexityLevel);
    JsonObject->SetBoolField(TEXT("IsTranslucent"), Data.bIsTranslucent);
    JsonObject->SetBoolField(TEXT("IsMasked"), Data.bIsMasked);
    
    return JsonObject;
}

TSharedPtr<FJsonObject> UResourceAnalyzer::AnimationDataToJson(const FAnimationAnalysisData& Data)
{
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
    
    JsonObject->SetStringField(TEXT("AssetName"), Data.AssetName);
    JsonObject->SetNumberField(TEXT("Duration"), Data.Duration);
    JsonObject->SetNumberField(TEXT("KeyframeCount"), Data.KeyframeCount);
    JsonObject->SetStringField(TEXT("CompressionScheme"), Data.CompressionScheme);
    JsonObject->SetNumberField(TEXT("EstimatedMemoryUsageMB"), Data.EstimatedMemoryUsageMB);
    JsonObject->SetNumberField(TEXT("FrameRate"), Data.FrameRate);
    
    return JsonObject;
}

TSharedPtr<FJsonObject> UResourceAnalyzer::AudioDataToJson(const FAudioAnalysisData& Data)
{
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
    
    JsonObject->SetStringField(TEXT("AssetName"), Data.AssetName);
    JsonObject->SetNumberField(TEXT("Duration"), Data.Duration);
    JsonObject->SetNumberField(TEXT("SampleRate"), Data.SampleRate);
    JsonObject->SetNumberField(TEXT("BitDepth"), Data.BitDepth);
    JsonObject->SetStringField(TEXT("CompressionFormat"), Data.CompressionFormat);
    JsonObject->SetNumberField(TEXT("EstimatedMemoryUsageMB"), Data.EstimatedMemoryUsageMB);
    JsonObject->SetNumberField(TEXT("NumChannels"), Data.NumChannels);
    
    return JsonObject;
}

TSharedPtr<FJsonObject> UResourceAnalyzer::LightingDataToJson(const FLightingAnalysisData& Data)
{
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
    
    JsonObject->SetStringField(TEXT("LightName"), Data.LightName);
    JsonObject->SetStringField(TEXT("LightType"), Data.LightType);
    JsonObject->SetStringField(TEXT("Mobility"), Data.Mobility);
    JsonObject->SetNumberField(TEXT("Intensity"), Data.Intensity);
    JsonObject->SetNumberField(TEXT("AttenuationRadius"), Data.AttenuationRadius);
    JsonObject->SetBoolField(TEXT("CastShadows"), Data.bCastShadows);
    JsonObject->SetBoolField(TEXT("HasLightFunction"), Data.bHasLightFunction);
    
    return JsonObject;
}

TSharedPtr<FJsonObject> UResourceAnalyzer::PostProcessDataToJson(const FPostProcessAnalysisData& Data)
{
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
    
    JsonObject->SetStringField(TEXT("VolumeName"), Data.VolumeName);
    JsonObject->SetBoolField(TEXT("IsUnbound"), Data.bIsUnbound);
    JsonObject->SetNumberField(TEXT("Priority"), Data.Priority);
    JsonObject->SetNumberField(TEXT("BlendRadius"), Data.BlendRadius);
    JsonObject->SetNumberField(TEXT("BlendWeight"), Data.BlendWeight);
    
    TArray<TSharedPtr<FJsonValue>> EffectsArray;
    for (const FString& Effect : Data.ActiveEffects)
    {
        EffectsArray.Add(MakeShareable(new FJsonValueString(Effect)));
    }
    JsonObject->SetArrayField(TEXT("ActiveEffects"), EffectsArray);
    
    return JsonObject;
}