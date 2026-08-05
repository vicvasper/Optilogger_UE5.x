// Copyright (c) Victor Rivas Perez. All Rights Reserved.

#include "ResourceAnalyzer.h"

#include "Animation/AnimBoneCompressionSettings.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/AudioComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/Light.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformFileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "OptiLogger.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Sound/SoundWave.h"
#include "StaticMeshResources.h"

#if WITH_EDITOR
#include "Editor.h"
#include "LevelEditorViewport.h"
#include "MaterialEditingLibrary.h"
#endif

namespace
{
	// -- Memory estimation ----------------------------------------------------------------
	//
	// These are deliberate approximations: the exact footprint of an asset depends on the
	// target platform's packing, streaming state and cooked format, none of which is knowable
	// from the editor. The figures below are the per-element sizes of a typical desktop
	// cook, and are named so a reader can see what is being assumed rather than having to
	// reverse-engineer a bare literal.

	constexpr float BytesPerMegabyte = 1024.0f * 1024.0f;

	/** Uncompressed RGBA8: four bytes per texel, before any compression factor. */
	constexpr float TextureBytesPerPixelUncompressed = 4.0f;

	/** Block-compressed formats at 4 bits per pixel, a quarter of uncompressed RGBA8. */
	constexpr float TextureCompressionFactorBlockCompressed = 0.25f;

	/** Two-channel normal formats at 8 bits per pixel. */
	constexpr float TextureCompressionFactorNormalMap = 0.5f;

	/** HDR formats carry more than 8 bits per channel, so they exceed the RGBA8 baseline. */
	constexpr float TextureCompressionFactorHDR = 2.0f;

	/** Conservative default for formats not enumerated below. */
	constexpr float TextureCompressionFactorDefault = 0.5f;

	/** A full mip chain adds roughly a third to the base level: 1 + 1/4 + 1/16 + ... -> 4/3. */
	constexpr float TextureMipChainFactor = 1.33f;

	/** Position, normal, tangent and one UV set, packed. */
	constexpr float StaticMeshBytesPerVertex = 32.0f;

	/** Three 32-bit indices per triangle. */
	constexpr float MeshIndicesPerTriangle = 3.0f;
	constexpr float MeshBytesPerIndex = 4.0f;

	/** Static mesh vertex data plus bone indices and weights. */
	constexpr float SkeletalMeshBytesPerVertex = 48.0f;

	/** One transform's worth of reference-pose data per bone. */
	constexpr float SkeletalMeshBytesPerBone = 64.0f;

	/** Rough per-keyframe cost across a typical bone count, after compression. */
	constexpr float AnimationBytesPerKeyframe = 100.0f;

	/**
	 * Assumed sample depth for audio size estimates.
	 *
	 * USoundWave does not expose the source bit depth after import, so this is the 16-bit
	 * default the importer uses. Reported alongside the estimate rather than hidden inside it.
	 */
	constexpr int32 AudioAssumedBitDepth = 16;
	constexpr float BitsPerByte = 8.0f;

	// -- Material complexity --------------------------------------------------------------
	//
	// Bucket boundaries in total shader instructions. Chosen to match the rules of thumb used
	// when reviewing material cost; they are not engine-defined.

	constexpr int32 MaterialComplexityLowMaxInstructions = 100;
	constexpr int32 MaterialComplexityMediumMaxInstructions = 300;

	// -- Visibility test ------------------------------------------------------------------

	/** Fallback field of view when no camera can be queried. */
	constexpr float DefaultCameraFOVDegrees = 90.0f;

	/** Fallback viewport size when the real one cannot be read, used only to normalise. */
	constexpr float FallbackViewportWidth = 1920.0f;
	constexpr float FallbackViewportHeight = 1080.0f;

	/** Near and far planes for the manual editor projection. */
	constexpr float VisibilityNearPlane = 0.1f;
	constexpr float VisibilityFarPlane = 10000.0f;

	/** Bounds are stored as a half-extent; doubling gives the full box size. */
	constexpr float ExtentToSize = 2.0f;

	/** Asset registry tag every UAnimationAsset publishes its skeleton under. */
	const FName AnimationSkeletonTag(TEXT("Skeleton"));

	constexpr const TCHAR* ReportPluginVersion = TEXT("1.0");
	constexpr const TCHAR* ReportSubdirectory = TEXT("ResourceReport");
	constexpr const TCHAR* ReportTimestampFormat = TEXT("%Y%m%d_%H%M%S");
}

UResourceAnalyzer::UResourceAnalyzer()
{
}

void UResourceAnalyzer::Initialize()
{
	ClearAnalysisResults();
	UE_LOG(LogOptiLogger, Log, TEXT("Resource analyzer initialised."));
}

void UResourceAnalyzer::AnalyzeCurrentLevel(bool bFilterVisible)
{
	UE_LOG(LogOptiLogger, Log, TEXT("Starting level analysis (visible-only filter: %s)."),
		bFilterVisible ? TEXT("on") : TEXT("off"));

	ClearAnalysisResults();

	if (!GetAnalysisWorld())
	{
		UE_LOG(LogOptiLogger, Error, TEXT("No world available; analysis cannot run."));
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

	UE_LOG(LogOptiLogger, Log,
		TEXT("Analysis complete: %d static meshes, %d skeletal meshes, %d textures, %d materials, ")
		TEXT("%d animations, %d sounds, %d lights, %d post-process volumes."),
		StaticMeshAnalysisResults.Num(), SkeletalMeshAnalysisResults.Num(),
		TextureAnalysisResults.Num(), MaterialAnalysisResults.Num(),
		AnimationAnalysisResults.Num(), AudioAnalysisResults.Num(),
		LightingAnalysisResults.Num(), PostProcessAnalysisResults.Num());
}

UWorld* UResourceAnalyzer::GetAnalysisWorld() const
{
#if WITH_EDITOR
	if (GEditor)
	{
		// A running PIE session takes priority: it reflects what is actually loaded and
		// streamed in, which is what an optimisation pass wants to measure.
		if (GEditor->PlayWorld)
		{
			return GEditor->PlayWorld;
		}

		if (UWorld* EditorWorld = GEditor->GetEditorWorldContext().World())
		{
			return EditorWorld;
		}
	}

	UE_LOG(LogOptiLogger, Error, TEXT("GEditor is unavailable or holds no world."));
	return nullptr;
#else
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Game)
			{
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

// ---------------------------------------------------------------------------------------
// Collection passes
//
// Each pass is independently callable, which is why they each fetch the world and iterate
// the level themselves rather than sharing one traversal. That costs one actor iteration per
// asset type; AnalyzeCurrentLevel therefore walks the level eight times. Acceptable for an
// on-demand editor tool, and the price of the granular API.
// ---------------------------------------------------------------------------------------

void UResourceAnalyzer::AnalyzeStaticMeshes(bool bFilterVisible)
{
	StaticMeshAnalysisResults.Empty();

	UWorld* World = GetAnalysisWorld();
	if (!World)
	{
		return;
	}

	TSet<UStaticMesh*> FoundMeshes;
	int32 ActorCount = 0;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		++ActorCount;

		if (bFilterVisible && !IsActorVisibleToCamera(*It))
		{
			continue;
		}

		TArray<UStaticMeshComponent*> Components;
		It->GetComponents<UStaticMeshComponent>(Components);

		for (const UStaticMeshComponent* Component : Components)
		{
			if (Component && Component->GetStaticMesh())
			{
				FoundMeshes.Add(Component->GetStaticMesh());
			}
		}
	}

	if (ActorCount == 0)
	{
		UE_LOG(LogOptiLogger, Warning, TEXT("The world contains no actors; every result will be empty."));
	}

	StaticMeshAnalysisResults.Reserve(FoundMeshes.Num());
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
		return;
	}

	TSet<USkeletalMesh*> FoundMeshes;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (bFilterVisible && !IsActorVisibleToCamera(*It))
		{
			continue;
		}

		TArray<USkeletalMeshComponent*> Components;
		It->GetComponents<USkeletalMeshComponent>(Components);

		for (const USkeletalMeshComponent* Component : Components)
		{
			if (Component && Component->GetSkeletalMeshAsset())
			{
				FoundMeshes.Add(Component->GetSkeletalMeshAsset());
			}
		}
	}

	SkeletalMeshAnalysisResults.Reserve(FoundMeshes.Num());
	for (USkeletalMesh* Mesh : FoundMeshes)
	{
		SkeletalMeshAnalysisResults.Add(AnalyzeSkeletalMeshAsset(Mesh));
	}
}

void UResourceAnalyzer::AnalyzeTextures(bool bFilterVisible)
{
	TextureAnalysisResults.Empty();

	UWorld* World = GetAnalysisWorld();
	if (!World)
	{
		return;
	}

	TSet<UTexture2D*> FoundTextures;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (bFilterVisible && !IsActorVisibleToCamera(*It))
		{
			continue;
		}

		TArray<UMeshComponent*> MeshComponents;
		It->GetComponents<UMeshComponent>(MeshComponents);

		for (const UMeshComponent* MeshComponent : MeshComponents)
		{
			if (!MeshComponent)
			{
				continue;
			}

			for (int32 MaterialIndex = 0; MaterialIndex < MeshComponent->GetNumMaterials(); ++MaterialIndex)
			{
				UMaterialInterface* Material = MeshComponent->GetMaterial(MaterialIndex);
				if (!Material)
				{
					continue;
				}

				TArray<UTexture*> UsedTextures;
				Material->GetUsedTextures(UsedTextures, EMaterialQualityLevel::Num,
					/*bAllQualityLevels=*/true, GMaxRHIFeatureLevel, /*bAllFeatureLevels=*/true);

				for (UTexture* Texture : UsedTextures)
				{
					if (UTexture2D* Texture2D = Cast<UTexture2D>(Texture))
					{
						FoundTextures.Add(Texture2D);
					}
				}
			}
		}
	}

	TextureAnalysisResults.Reserve(FoundTextures.Num());
	for (UTexture2D* Texture : FoundTextures)
	{
		TextureAnalysisResults.Add(AnalyzeTextureAsset(Texture));
	}
}

void UResourceAnalyzer::AnalyzeMaterials(bool bFilterVisible)
{
	MaterialAnalysisResults.Empty();

	UWorld* World = GetAnalysisWorld();
	if (!World)
	{
		return;
	}

	TSet<UMaterial*> FoundMaterials;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (bFilterVisible && !IsActorVisibleToCamera(*It))
		{
			continue;
		}

		TArray<UMeshComponent*> Components;
		It->GetComponents<UMeshComponent>(Components);

		for (const UMeshComponent* Component : Components)
		{
			if (!Component)
			{
				continue;
			}

			for (UMaterialInterface* MaterialInterface : Component->GetMaterials())
			{
				if (!MaterialInterface)
				{
					continue;
				}

				if (UMaterial* BaseMaterial = MaterialInterface->GetBaseMaterial())
				{
					FoundMaterials.Add(BaseMaterial);
				}
			}
		}
	}

	MaterialAnalysisResults.Reserve(FoundMaterials.Num());
	for (UMaterial* Material : FoundMaterials)
	{
		MaterialAnalysisResults.Add(AnalyzeMaterialAsset(Material));
	}
}

void UResourceAnalyzer::AnalyzeAnimations(bool bFilterVisible)
{
	AnimationAnalysisResults.Empty();

	UWorld* World = GetAnalysisWorld();
	if (!World)
	{
		return;
	}

	TSet<UAnimSequence*> FoundAnimations;
	TSet<USkeleton*> SkeletonsInUse;

	// 1. Collect the skeletons actually present in the level, plus any sequence a component
	//    plays directly.
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (bFilterVisible && !IsActorVisibleToCamera(*It))
		{
			continue;
		}

		TArray<USkeletalMeshComponent*> Components;
		It->GetComponents<USkeletalMeshComponent>(Components);

		for (const USkeletalMeshComponent* Component : Components)
		{
			if (!Component)
			{
				continue;
			}

			if (const USkeletalMesh* Mesh = Component->GetSkeletalMeshAsset())
			{
				if (USkeleton* Skeleton = Mesh->GetSkeleton())
				{
					SkeletonsInUse.Add(Skeleton);
				}
			}

			if (UAnimSequence* Sequence = Cast<UAnimSequence>(Component->AnimationData.AnimToPlay.Get()))
			{
				FoundAnimations.Add(Sequence);
			}
		}
	}

	// 2. Sequences reached through an Animation Blueprint are not referenced by any component,
	//    so they have to come from the asset registry. Matching is done against the registry's
	//    cached "Skeleton" tag rather than by loading each candidate: the previous version
	//    called GetAsset() on every UAnimSequence in the project just to read its skeleton,
	//    which pulls the project's entire animation library into memory to discard most of it.
	if (SkeletonsInUse.Num() > 0)
	{
		TSet<FString> SkeletonExportNames;
		SkeletonExportNames.Reserve(SkeletonsInUse.Num());
		for (const USkeleton* Skeleton : SkeletonsInUse)
		{
			SkeletonExportNames.Add(FAssetData(Skeleton).GetExportTextName());
		}

		const FAssetRegistryModule& AssetRegistryModule =
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

		FARFilter Filter;
		Filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
		Filter.bRecursiveClasses = true;

		TArray<FAssetData> CandidateAssets;
		AssetRegistryModule.Get().GetAssets(Filter, CandidateAssets);

		int32 LoadedCount = 0;
		for (const FAssetData& Asset : CandidateAssets)
		{
			FString SkeletonExportName;
			if (!Asset.GetTagValue(AnimationSkeletonTag, SkeletonExportName) ||
				!SkeletonExportNames.Contains(SkeletonExportName))
			{
				continue;
			}

			// Only assets that already matched on metadata are loaded.
			if (UAnimSequence* Sequence = Cast<UAnimSequence>(Asset.GetAsset()))
			{
				FoundAnimations.Add(Sequence);
				++LoadedCount;
			}
		}

		UE_LOG(LogOptiLogger, Verbose,
			TEXT("Animation scan: %d sequences in the project, %d matched a skeleton in this level."),
			CandidateAssets.Num(), LoadedCount);
	}

	AnimationAnalysisResults.Reserve(FoundAnimations.Num());
	for (UAnimSequence* Sequence : FoundAnimations)
	{
		AnimationAnalysisResults.Add(AnalyzeAnimationAsset(Sequence));
	}
}

void UResourceAnalyzer::AnalyzeAudio(bool bFilterVisible)
{
	AudioAnalysisResults.Empty();

	UWorld* World = GetAnalysisWorld();
	if (!World)
	{
		return;
	}

	TSet<USoundWave*> FoundSounds;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (bFilterVisible && !IsActorVisibleToCamera(*It))
		{
			continue;
		}

		TArray<UAudioComponent*> Components;
		It->GetComponents<UAudioComponent>(Components);

		for (const UAudioComponent* Component : Components)
		{
			if (!Component)
			{
				continue;
			}

			if (USoundWave* SoundWave = Cast<USoundWave>(Component->Sound))
			{
				FoundSounds.Add(SoundWave);
			}
		}
	}

	AudioAnalysisResults.Reserve(FoundSounds.Num());
	for (USoundWave* SoundWave : FoundSounds)
	{
		AudioAnalysisResults.Add(AnalyzeAudioAsset(SoundWave));
	}
}

void UResourceAnalyzer::AnalyzeLighting(bool bFilterVisible)
{
	LightingAnalysisResults.Empty();

	UWorld* World = GetAnalysisWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ALight> It(World); It; ++It)
	{
		if (bFilterVisible && !IsActorVisibleToCamera(*It))
		{
			continue;
		}

		if (ULightComponent* LightComponent = It->GetLightComponent())
		{
			LightingAnalysisResults.Add(AnalyzeLightComponent(LightComponent));
		}
	}
}

void UResourceAnalyzer::AnalyzePostProcessEffects(bool bFilterVisible)
{
	PostProcessAnalysisResults.Empty();

	UWorld* World = GetAnalysisWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<APostProcessVolume> It(World); It; ++It)
	{
		if (bFilterVisible && !IsActorVisibleToCamera(*It))
		{
			continue;
		}

		PostProcessAnalysisResults.Add(AnalyzePostProcessVolume(*It));
	}
}

// ---------------------------------------------------------------------------------------
// Visibility
// ---------------------------------------------------------------------------------------

bool UResourceAnalyzer::IsActorVisibleToCamera(AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}

	UWorld* World = GetAnalysisWorld();
	if (!World)
	{
		return false;
	}

	FVector CameraLocation = FVector::ZeroVector;
	FRotator CameraRotation = FRotator::ZeroRotator;
	float FieldOfView = DefaultCameraFOVDegrees;
	FVector2D ViewportSize = FVector2D::ZeroVector;

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0);

	if (PlayerController)
	{
		PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);

		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->GetViewportSize(ViewportSize);
		}

		// Guarded: a controller can exist before its camera manager does, and the previous
		// version dereferenced PlayerCameraManager unconditionally.
		if (PlayerController->PlayerCameraManager)
		{
			FieldOfView = PlayerController->PlayerCameraManager->GetFOVAngle();
		}
	}
	else
	{
#if WITH_EDITOR
		FEditorViewportClient* ViewportClient =
			GEditor && GEditor->GetActiveViewport()
				? static_cast<FEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient())
				: nullptr;

		if (!ViewportClient || !ViewportClient->Viewport)
		{
			return false;
		}

		CameraLocation = ViewportClient->GetViewLocation();
		CameraRotation = ViewportClient->GetViewRotation();
		FieldOfView = ViewportClient->FOVAngle;
		ViewportSize = FVector2D(ViewportClient->Viewport->GetSizeXY());
#else
		return false;
#endif
	}

	if (ViewportSize.X <= 0.0f || ViewportSize.Y <= 0.0f)
	{
		ViewportSize = FVector2D(FallbackViewportWidth, FallbackViewportHeight);
	}

	const FBoxSphereBounds Bounds = Actor->GetComponentsBoundingBox(true);

	// An actor counts as visible if any corner of its bounds lands on screen. Cheaper and
	// good enough for an analysis filter; it does not account for occlusion.
	FVector Corners[8];
	FBox(Bounds.Origin - Bounds.BoxExtent, Bounds.Origin + Bounds.BoxExtent).GetVertices(Corners);

	const FVector CameraForward = CameraRotation.Vector();

	for (const FVector& Corner : Corners)
	{
		if (FVector::DotProduct((Corner - CameraLocation).GetSafeNormal(), CameraForward) <= 0.0f)
		{
			continue;
		}

		FVector2D ScreenPosition;

		if (PlayerController)
		{
			if (!UGameplayStatics::ProjectWorldToScreen(PlayerController, Corner, ScreenPosition))
			{
				continue;
			}
		}
		else
		{
			// The editor viewport exposes no projection helper, so the transform is done by
			// hand against the same view parameters.
			const FMatrix ViewMatrix = FRotationTranslationMatrix(CameraRotation, CameraLocation).Inverse();
			const FMatrix ProjectionMatrix = FPerspectiveMatrix(
				FMath::DegreesToRadians(FieldOfView),
				ViewportSize.X / ViewportSize.Y,
				VisibilityNearPlane,
				VisibilityFarPlane);

			const FVector4 ClipSpace =
				ProjectionMatrix.TransformFVector4(ViewMatrix.TransformPosition(Corner));

			if (ClipSpace.W <= 0.0f)
			{
				continue;
			}

			ScreenPosition.X = ((ClipSpace.X / ClipSpace.W + 1.0f) * 0.5f) * ViewportSize.X;
			ScreenPosition.Y = (1.0f - (ClipSpace.Y / ClipSpace.W + 1.0f) * 0.5f) * ViewportSize.Y;
		}

		if (ScreenPosition.X >= 0.0f && ScreenPosition.X <= ViewportSize.X &&
			ScreenPosition.Y >= 0.0f && ScreenPosition.Y <= ViewportSize.Y)
		{
			return true;
		}
	}

	return false;
}

// ---------------------------------------------------------------------------------------
// Per-asset analysis
// ---------------------------------------------------------------------------------------

FStaticMeshAnalysisData UResourceAnalyzer::AnalyzeStaticMeshAsset(UStaticMesh* StaticMesh) const
{
	FStaticMeshAnalysisData AnalysisData;
	if (!StaticMesh)
	{
		return AnalysisData;
	}

	AnalysisData.AssetName = StaticMesh->GetName();

	if (const FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData())
	{
		AnalysisData.LODCount = RenderData->LODResources.Num();
		AnalysisData.LODVertexCounts.Reserve(AnalysisData.LODCount);
		AnalysisData.LODTriangleCounts.Reserve(AnalysisData.LODCount);

		for (int32 LODIndex = 0; LODIndex < AnalysisData.LODCount; ++LODIndex)
		{
			const FStaticMeshLODResources& LODResource = RenderData->LODResources[LODIndex];
			AnalysisData.LODVertexCounts.Add(LODResource.GetNumVertices());
			AnalysisData.LODTriangleCounts.Add(LODResource.GetNumTriangles());
		}

		// LOD 0 is the figure reported at the top level.
		if (AnalysisData.LODCount > 0)
		{
			AnalysisData.VertexCount = AnalysisData.LODVertexCounts[0];
			AnalysisData.TriangleCount = AnalysisData.LODTriangleCounts[0];
		}
	}

	AnalysisData.BoundingBoxSize = StaticMesh->GetBounds().BoxExtent * ExtentToSize;
	AnalysisData.EstimatedMemoryUsageMB =
		EstimateStaticMeshMemoryUsage(AnalysisData.VertexCount, AnalysisData.TriangleCount);

	return AnalysisData;
}

FSkeletalMeshAnalysisData UResourceAnalyzer::AnalyzeSkeletalMeshAsset(USkeletalMesh* SkeletalMesh) const
{
	FSkeletalMeshAnalysisData AnalysisData;
	if (!SkeletalMesh)
	{
		return AnalysisData;
	}

	AnalysisData.AssetName = SkeletalMesh->GetName();

	if (const USkeleton* Skeleton = SkeletalMesh->GetSkeleton())
	{
		AnalysisData.BoneCount = Skeleton->GetReferenceSkeleton().GetNum();
	}

	if (const FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering())
	{
		AnalysisData.LODCount = RenderData->LODRenderData.Num();
		if (AnalysisData.LODCount > 0)
		{
			AnalysisData.VertexCount = RenderData->LODRenderData[0].GetNumVertices();
		}
	}

	AnalysisData.BoundingBoxSize = SkeletalMesh->GetBounds().BoxExtent * ExtentToSize;
	AnalysisData.EstimatedMemoryUsageMB =
		EstimateSkeletalMeshMemoryUsage(AnalysisData.VertexCount, AnalysisData.BoneCount);

	return AnalysisData;
}

FTextureAnalysisData UResourceAnalyzer::AnalyzeTextureAsset(UTexture2D* Texture) const
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
		AnalysisData.Width, AnalysisData.Height, Texture->CompressionSettings, AnalysisData.MipLevels);

	return AnalysisData;
}

FMaterialAnalysisData UResourceAnalyzer::AnalyzeMaterialAsset(UMaterial* Material) const
{
	FMaterialAnalysisData AnalysisData;
	if (!Material)
	{
		return AnalysisData;
	}

	const EBlendMode BlendMode = Material->GetBlendMode();

	AnalysisData.AssetName = Material->GetName();
	AnalysisData.bIsMasked = (BlendMode == BLEND_Masked);

	// Masked is neither opaque nor translucent: it is reported through bIsMasked and excluded
	// here. Testing only for "not opaque", as before, flagged every masked material as
	// translucent while bIsMasked also said it was masked.
	AnalysisData.bIsTranslucent = (BlendMode != BLEND_Opaque && BlendMode != BLEND_Masked);

	TArray<UTexture*> UsedTextures;
	Material->GetUsedTextures(UsedTextures, EMaterialQualityLevel::Num,
		/*bAllQualityLevels=*/true, GMaxRHIFeatureLevel, /*bAllFeatureLevels=*/true);

	AnalysisData.TextureReferences = UsedTextures.Num();
	AnalysisData.ReferencedTextures.Reserve(UsedTextures.Num());
	for (const UTexture* Texture : UsedTextures)
	{
		if (Texture)
		{
			AnalysisData.ReferencedTextures.Add(Texture->GetName());
		}
	}

#if WITH_EDITOR
	// Shader instruction counts come from the material editor's own statistics, so they match
	// what the editor reports. They do not exist outside an editor build.
	const FMaterialStatistics Statistics = UMaterialEditingLibrary::GetStatistics(Material);
	AnalysisData.NumVertexShaderInstructions = Statistics.NumVertexShaderInstructions;
	AnalysisData.NumPixelShaderInstructions = Statistics.NumPixelShaderInstructions;
	AnalysisData.TotalShaderInstructions =
		AnalysisData.NumVertexShaderInstructions + AnalysisData.NumPixelShaderInstructions;
#endif

	if (AnalysisData.TotalShaderInstructions < MaterialComplexityLowMaxInstructions)
	{
		AnalysisData.ComplexityLevel = TEXT("Low");
	}
	else if (AnalysisData.TotalShaderInstructions < MaterialComplexityMediumMaxInstructions)
	{
		AnalysisData.ComplexityLevel = TEXT("Medium");
	}
	else
	{
		AnalysisData.ComplexityLevel = TEXT("High");
	}

	return AnalysisData;
}

FAnimationAnalysisData UResourceAnalyzer::AnalyzeAnimationAsset(UAnimSequence* Animation) const
{
	FAnimationAnalysisData AnalysisData;
	if (!Animation)
	{
		return AnalysisData;
	}

	AnalysisData.AssetName = Animation->GetName();
	AnalysisData.Duration = Animation->GetPlayLength();
	AnalysisData.FrameRate = Animation->GetSamplingFrameRate().AsDecimal();
	AnalysisData.KeyframeCount = FMath::RoundToInt(AnalysisData.Duration * AnalysisData.FrameRate);

	AnalysisData.CompressionScheme = Animation->BoneCompressionSettings
		? Animation->BoneCompressionSettings->GetClass()->GetName()
		: TEXT("None");

	AnalysisData.EstimatedMemoryUsageMB =
		(AnalysisData.KeyframeCount * AnimationBytesPerKeyframe) / BytesPerMegabyte;

	return AnalysisData;
}

FAudioAnalysisData UResourceAnalyzer::AnalyzeAudioAsset(USoundWave* SoundWave) const
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
	AnalysisData.BitDepth = AudioAssumedBitDepth;

	// Whether the asset streams is known; the codec behind it is chosen per platform at cook
	// time and is not available here, so it is not claimed.
	AnalysisData.CompressionFormat = SoundWave->IsStreaming() ? TEXT("Streaming") : TEXT("In-memory");

	const float UncompressedBytes = AnalysisData.Duration * AnalysisData.SampleRate *
		AnalysisData.NumChannels * (AnalysisData.BitDepth / BitsPerByte);

	AnalysisData.EstimatedMemoryUsageMB = UncompressedBytes / BytesPerMegabyte;

	return AnalysisData;
}

FLightingAnalysisData UResourceAnalyzer::AnalyzeLightComponent(ULightComponent* LightComponent) const
{
	FLightingAnalysisData AnalysisData;
	if (!LightComponent)
	{
		return AnalysisData;
	}

	const AActor* Owner = LightComponent->GetOwner();

	AnalysisData.LightName = Owner ? Owner->GetName() : TEXT("Unknown");
	AnalysisData.LightType = GetLightTypeString(LightComponent);
	AnalysisData.Mobility = GetMobilityString(LightComponent->Mobility);
	AnalysisData.Intensity = LightComponent->Intensity;
	AnalysisData.LightColor = LightComponent->LightColor;
	AnalysisData.bCastShadows = LightComponent->CastShadows;
	AnalysisData.bHasLightFunction = (LightComponent->LightFunctionMaterial != nullptr);

	// Spot lights derive from point lights, so they must be tested first.
	if (const USpotLightComponent* SpotLight = Cast<USpotLightComponent>(LightComponent))
	{
		AnalysisData.AttenuationRadius = SpotLight->AttenuationRadius;
	}
	else if (const UPointLightComponent* PointLight = Cast<UPointLightComponent>(LightComponent))
	{
		AnalysisData.AttenuationRadius = PointLight->AttenuationRadius;
	}

	return AnalysisData;
}

FPostProcessAnalysisData UResourceAnalyzer::AnalyzePostProcessVolume(APostProcessVolume* PostProcessVolume) const
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

// ---------------------------------------------------------------------------------------
// Export
// ---------------------------------------------------------------------------------------

bool UResourceAnalyzer::ExportAnalysisToJSON(const FString& FilePath)
{
	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();

	RootObject->SetStringField(TEXT("ExportDate"), FDateTime::Now().ToString());
	RootObject->SetStringField(TEXT("PluginVersion"), ReportPluginVersion);
	RootObject->SetStringField(TEXT("EngineVersion"), FEngineVersion::Current().ToString());

	// Eight near-identical blocks collapsed into one helper: each differed only in the field
	// name, the result array and the converter.
	const auto AddSection = [&RootObject](const TCHAR* FieldName, const auto& Results, auto&& Converter)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		Values.Reserve(Results.Num());

		for (const auto& Data : Results)
		{
			Values.Add(MakeShared<FJsonValueObject>(Converter(Data)));
		}

		RootObject->SetArrayField(FieldName, Values);
	};

	AddSection(TEXT("StaticMeshes"), StaticMeshAnalysisResults,
		[this](const FStaticMeshAnalysisData& Data) { return StaticMeshDataToJson(Data); });
	AddSection(TEXT("SkeletalMeshes"), SkeletalMeshAnalysisResults,
		[this](const FSkeletalMeshAnalysisData& Data) { return SkeletalMeshDataToJson(Data); });
	AddSection(TEXT("Textures"), TextureAnalysisResults,
		[this](const FTextureAnalysisData& Data) { return TextureDataToJson(Data); });
	AddSection(TEXT("Materials"), MaterialAnalysisResults,
		[this](const FMaterialAnalysisData& Data) { return MaterialDataToJson(Data); });
	AddSection(TEXT("Animations"), AnimationAnalysisResults,
		[this](const FAnimationAnalysisData& Data) { return AnimationDataToJson(Data); });
	AddSection(TEXT("Audio"), AudioAnalysisResults,
		[this](const FAudioAnalysisData& Data) { return AudioDataToJson(Data); });
	AddSection(TEXT("Lighting"), LightingAnalysisResults,
		[this](const FLightingAnalysisData& Data) { return LightingDataToJson(Data); });
	AddSection(TEXT("PostProcess"), PostProcessAnalysisResults,
		[this](const FPostProcessAnalysisData& Data) { return PostProcessDataToJson(Data); });

	FString OutputString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject, Writer);

	FString FinalFilePath = FilePath;
	if (FinalFilePath.IsEmpty())
	{
		const FString ReportDir = FPaths::Combine(FPaths::ProjectSavedDir(), ReportSubdirectory);

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (!PlatformFile.DirectoryExists(*ReportDir) && !PlatformFile.CreateDirectoryTree(*ReportDir))
		{
			// Reported rather than left to fail as an opaque write error further down.
			UE_LOG(LogOptiLogger, Error, TEXT("Could not create the report directory '%s'."), *ReportDir);
			return false;
		}

		const FString Timestamp = FDateTime::Now().ToString(ReportTimestampFormat);
		FinalFilePath = FPaths::Combine(ReportDir,
			FString::Printf(TEXT("OptiloggerReport_%s.json"), *Timestamp));
	}

	if (!FFileHelper::SaveStringToFile(OutputString, *FinalFilePath))
	{
		UE_LOG(LogOptiLogger, Error, TEXT("Failed to write the analysis report to '%s'."), *FinalFilePath);
		return false;
	}

	UE_LOG(LogOptiLogger, Log, TEXT("Analysis report written to '%s'."), *FinalFilePath);
	return true;
}

// ---------------------------------------------------------------------------------------
// Formatting helpers
// ---------------------------------------------------------------------------------------

FString UResourceAnalyzer::GetCompressionFormatString(TextureCompressionSettings CompressionSettings)
{
	switch (CompressionSettings)
	{
	case TC_Default:                 return TEXT("Default");
	case TC_Normalmap:               return TEXT("Normal Map");
	case TC_Masks:                   return TEXT("Masks");
	case TC_Grayscale:               return TEXT("Grayscale");
	case TC_Displacementmap:         return TEXT("Displacement Map");
	case TC_VectorDisplacementmap:   return TEXT("Vector Displacement Map");
	case TC_HDR:                     return TEXT("HDR");
	case TC_EditorIcon:              return TEXT("Editor Icon");
	case TC_Alpha:                   return TEXT("Alpha");
	case TC_DistanceFieldFont:       return TEXT("Distance Field Font");
	case TC_HDR_Compressed:          return TEXT("HDR Compressed");
	case TC_BC7:                     return TEXT("BC7");
	default:                         return TEXT("Unknown");
	}
}

FString UResourceAnalyzer::GetMobilityString(EComponentMobility::Type Mobility)
{
	switch (Mobility)
	{
	case EComponentMobility::Static:      return TEXT("Static");
	case EComponentMobility::Stationary:  return TEXT("Stationary");
	case EComponentMobility::Movable:     return TEXT("Movable");
	default:                              return TEXT("Unknown");
	}
}

FString UResourceAnalyzer::GetLightTypeString(ULightComponent* LightComponent)
{
	// Ordered most-derived first: USpotLightComponent derives from UPointLightComponent, so
	// testing for point lights first would report every spot light as a point light.
	if (Cast<UDirectionalLightComponent>(LightComponent))
	{
		return TEXT("Directional Light");
	}
	if (Cast<USpotLightComponent>(LightComponent))
	{
		return TEXT("Spot Light");
	}
	if (Cast<UPointLightComponent>(LightComponent))
	{
		return TEXT("Point Light");
	}

	return TEXT("Unknown Light Type");
}

// ---------------------------------------------------------------------------------------
// Memory estimation
// ---------------------------------------------------------------------------------------

float UResourceAnalyzer::EstimateTextureMemoryUsage(
	int32 Width, int32 Height, TextureCompressionSettings CompressionSettings, int32 MipLevels)
{
	float CompressionFactor = TextureCompressionFactorDefault;

	switch (CompressionSettings)
	{
	case TC_Default:
	case TC_BC7:
	case TC_Grayscale:
	case TC_Alpha:
		CompressionFactor = TextureCompressionFactorBlockCompressed;
		break;

	case TC_Normalmap:
		CompressionFactor = TextureCompressionFactorNormalMap;
		break;

	case TC_HDR:
	case TC_HDR_Compressed:
		CompressionFactor = TextureCompressionFactorHDR;
		break;

	default:
		break;
	}

	const float BaseSizeBytes =
		static_cast<float>(Width) * static_cast<float>(Height) * TextureBytesPerPixelUncompressed;

	const float MipFactor = (MipLevels > 1) ? TextureMipChainFactor : 1.0f;

	return (BaseSizeBytes * CompressionFactor * MipFactor) / BytesPerMegabyte;
}

float UResourceAnalyzer::EstimateStaticMeshMemoryUsage(int32 VertexCount, int32 TriangleCount)
{
	const float VertexBytes = VertexCount * StaticMeshBytesPerVertex;
	const float IndexBytes = TriangleCount * MeshIndicesPerTriangle * MeshBytesPerIndex;

	return (VertexBytes + IndexBytes) / BytesPerMegabyte;
}

float UResourceAnalyzer::EstimateSkeletalMeshMemoryUsage(int32 VertexCount, int32 BoneCount)
{
	const float VertexBytes = VertexCount * SkeletalMeshBytesPerVertex;
	const float BoneBytes = BoneCount * SkeletalMeshBytesPerBone;

	return (VertexBytes + BoneBytes) / BytesPerMegabyte;
}

// ---------------------------------------------------------------------------------------
// JSON conversion
// ---------------------------------------------------------------------------------------

TSharedPtr<FJsonObject> UResourceAnalyzer::StaticMeshDataToJson(const FStaticMeshAnalysisData& Data) const
{
	const TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();

	JsonObject->SetStringField(TEXT("AssetName"), Data.AssetName);
	JsonObject->SetNumberField(TEXT("VertexCount"), Data.VertexCount);
	JsonObject->SetNumberField(TEXT("TriangleCount"), Data.TriangleCount);
	JsonObject->SetNumberField(TEXT("LODCount"), Data.LODCount);
	JsonObject->SetNumberField(TEXT("EstimatedMemoryUsageMB"), Data.EstimatedMemoryUsageMB);

	const auto ToNumberArray = [](const TArray<int32>& Source)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		Values.Reserve(Source.Num());
		for (int32 Count : Source)
		{
			Values.Add(MakeShared<FJsonValueNumber>(Count));
		}
		return Values;
	};

	JsonObject->SetArrayField(TEXT("LODVertexCounts"), ToNumberArray(Data.LODVertexCounts));

	// Per-LOD triangle counts were collected but never exported, so the report showed vertex
	// counts per LOD with no triangle counterpart.
	JsonObject->SetArrayField(TEXT("LODTriangleCounts"), ToNumberArray(Data.LODTriangleCounts));

	return JsonObject;
}

TSharedPtr<FJsonObject> UResourceAnalyzer::SkeletalMeshDataToJson(const FSkeletalMeshAnalysisData& Data) const
{
	const TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();

	JsonObject->SetStringField(TEXT("AssetName"), Data.AssetName);
	JsonObject->SetNumberField(TEXT("BoneCount"), Data.BoneCount);
	JsonObject->SetNumberField(TEXT("VertexCount"), Data.VertexCount);
	JsonObject->SetNumberField(TEXT("LODCount"), Data.LODCount);
	JsonObject->SetNumberField(TEXT("EstimatedMemoryUsageMB"), Data.EstimatedMemoryUsageMB);

	return JsonObject;
}

TSharedPtr<FJsonObject> UResourceAnalyzer::TextureDataToJson(const FTextureAnalysisData& Data) const
{
	const TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();

	JsonObject->SetStringField(TEXT("AssetName"), Data.AssetName);
	JsonObject->SetNumberField(TEXT("Width"), Data.Width);
	JsonObject->SetNumberField(TEXT("Height"), Data.Height);
	JsonObject->SetStringField(TEXT("CompressionFormat"), Data.CompressionFormat);
	JsonObject->SetNumberField(TEXT("MipLevels"), Data.MipLevels);
	JsonObject->SetBoolField(TEXT("IsVirtualTexture"), Data.bIsVirtualTexture);
	JsonObject->SetNumberField(TEXT("EstimatedMemoryUsageMB"), Data.EstimatedMemoryUsageMB);

	return JsonObject;
}

TSharedPtr<FJsonObject> UResourceAnalyzer::MaterialDataToJson(const FMaterialAnalysisData& Data) const
{
	const TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();

	JsonObject->SetStringField(TEXT("AssetName"), Data.AssetName);
	JsonObject->SetNumberField(TEXT("TextureReferences"), Data.TextureReferences);
	JsonObject->SetNumberField(TEXT("VertexShaderInstructions"), Data.NumVertexShaderInstructions);
	JsonObject->SetNumberField(TEXT("PixelShaderInstructions"), Data.NumPixelShaderInstructions);
	JsonObject->SetNumberField(TEXT("TotalShaderInstructions"), Data.TotalShaderInstructions);
	JsonObject->SetStringField(TEXT("ComplexityLevel"), Data.ComplexityLevel);
	JsonObject->SetBoolField(TEXT("IsTranslucent"), Data.bIsTranslucent);
	JsonObject->SetBoolField(TEXT("IsMasked"), Data.bIsMasked);

	TArray<TSharedPtr<FJsonValue>> TextureNames;
	TextureNames.Reserve(Data.ReferencedTextures.Num());
	for (const FString& TextureName : Data.ReferencedTextures)
	{
		TextureNames.Add(MakeShared<FJsonValueString>(TextureName));
	}

	// Collected but never exported, so the report gave a texture count with no way to see
	// which textures it counted.
	JsonObject->SetArrayField(TEXT("ReferencedTextures"), TextureNames);

	return JsonObject;
}

TSharedPtr<FJsonObject> UResourceAnalyzer::AnimationDataToJson(const FAnimationAnalysisData& Data) const
{
	const TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();

	JsonObject->SetStringField(TEXT("AssetName"), Data.AssetName);
	JsonObject->SetNumberField(TEXT("Duration"), Data.Duration);
	JsonObject->SetNumberField(TEXT("FrameRate"), Data.FrameRate);
	JsonObject->SetNumberField(TEXT("KeyframeCount"), Data.KeyframeCount);
	JsonObject->SetStringField(TEXT("CompressionScheme"), Data.CompressionScheme);
	JsonObject->SetNumberField(TEXT("EstimatedMemoryUsageMB"), Data.EstimatedMemoryUsageMB);

	return JsonObject;
}

TSharedPtr<FJsonObject> UResourceAnalyzer::AudioDataToJson(const FAudioAnalysisData& Data) const
{
	const TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();

	JsonObject->SetStringField(TEXT("AssetName"), Data.AssetName);
	JsonObject->SetNumberField(TEXT("Duration"), Data.Duration);
	JsonObject->SetNumberField(TEXT("SampleRate"), Data.SampleRate);
	JsonObject->SetNumberField(TEXT("NumChannels"), Data.NumChannels);
	JsonObject->SetNumberField(TEXT("BitDepth"), Data.BitDepth);
	JsonObject->SetStringField(TEXT("CompressionFormat"), Data.CompressionFormat);
	JsonObject->SetNumberField(TEXT("EstimatedMemoryUsageMB"), Data.EstimatedMemoryUsageMB);

	return JsonObject;
}

TSharedPtr<FJsonObject> UResourceAnalyzer::LightingDataToJson(const FLightingAnalysisData& Data) const
{
	const TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();

	JsonObject->SetStringField(TEXT("LightName"), Data.LightName);
	JsonObject->SetStringField(TEXT("LightType"), Data.LightType);
	JsonObject->SetStringField(TEXT("Mobility"), Data.Mobility);
	JsonObject->SetNumberField(TEXT("Intensity"), Data.Intensity);
	JsonObject->SetNumberField(TEXT("AttenuationRadius"), Data.AttenuationRadius);
	JsonObject->SetBoolField(TEXT("CastShadows"), Data.bCastShadows);
	JsonObject->SetBoolField(TEXT("HasLightFunction"), Data.bHasLightFunction);

	// Collected but never exported.
	JsonObject->SetStringField(TEXT("LightColor"), Data.LightColor.ToString());

	return JsonObject;
}

TSharedPtr<FJsonObject> UResourceAnalyzer::PostProcessDataToJson(const FPostProcessAnalysisData& Data) const
{
	const TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();

	JsonObject->SetStringField(TEXT("VolumeName"), Data.VolumeName);
	JsonObject->SetBoolField(TEXT("IsUnbound"), Data.bIsUnbound);
	JsonObject->SetNumberField(TEXT("Priority"), Data.Priority);
	JsonObject->SetNumberField(TEXT("BlendRadius"), Data.BlendRadius);
	JsonObject->SetNumberField(TEXT("BlendWeight"), Data.BlendWeight);

	TArray<TSharedPtr<FJsonValue>> EffectsArray;
	EffectsArray.Reserve(Data.ActiveEffects.Num());
	for (const FString& Effect : Data.ActiveEffects)
	{
		EffectsArray.Add(MakeShared<FJsonValueString>(Effect));
	}
	JsonObject->SetArrayField(TEXT("ActiveEffects"), EffectsArray);

	return JsonObject;
}
