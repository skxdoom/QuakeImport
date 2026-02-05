#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"
#include "Engine/EngineTypes.h"
#include "Engine/CollisionProfile.h"
#include "QuakeBSPImportAsset.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogQuakeImporter, Log, All);

UENUM(BlueprintType)
enum class EWorldChunkMode : uint8
{
	Leaves UMETA(DisplayName="Leaves"),
	Grid UMETA(DisplayName="Grid")
};

UCLASS(BlueprintType)
class QUAKEIMPORT_API UQuakeBSPImportAsset : public UObject
{
    GENERATED_BODY()

public:
	UQuakeBSPImportAsset();
    
    UFUNCTION(CallInEditor, Category="Quake Import", meta = (DisplayName="Import BSP World"))
    void ImportBSP();
    
    UFUNCTION(CallInEditor, Category="Quake Import", meta = (DisplayName="Import BSP Entities"))
    void ImportEntities();
    
    UPROPERTY(EditAnywhere, Category = "Quake Import", meta = (DisplayName="BSP File"))
    FFilePath BSPFile;

    UPROPERTY(EditAnywhere, Category = "Quake Import")
    EWorldChunkMode WorldChunkMode = EWorldChunkMode::Grid;

    UPROPERTY(EditAnywhere, Category = "Quake Import", meta=(ClampMin="1", EditCondition="WorldChunkMode==EWorldChunkMode::Grid", EditConditionHides))
    int32 WorldChunkSize = 512;

    UPROPERTY(EditAnywhere, Category = "Quake Import", meta=(ClampMin="0.0001"))
    float ImportScale = 2.5f;

	// If enabled, the importer will overwrite existing generated textures and material instances on reimport.
	// Static meshes are still overwritten regardless.
	UPROPERTY(EditAnywhere, Category = "Quake Import")
	bool bOverwriteMaterialsAndTextures = true;

	// If enabled, the importer will extract Quake BSP lightmaps into a shared atlas texture and generate UV1 for meshes to sample it.
	UPROPERTY(EditAnywhere, Category = "Quake Import")
	bool bImportLightmaps = false;
    
    // Include sky surfaces (textures starting with "sky") in the chunked BSP world geometry.
    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP World", meta = (DisplayName="Import Sky"))
    bool bBSPWorldImportSky = true;

    // Include water/lava/slime surfaces (textures starting with "*") in the chunked BSP world geometry.
    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP World", meta = (DisplayName="Import Liquids"))
    bool bBSPWorldImportLiquids = true;

    // Optional override for the opaque BSP parent material. If unset, the importer creates/uses M_BSP_Surface.
    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP World|Material", meta = (DisplayName="World Solid Material"))
    TSoftObjectPtr<UMaterialInterface> BSPWorldSolidMaterial;

    // Optional override for the opaque BSP parent material when importing BSP lightmaps.
    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP World|Material", meta=(DisplayName="World Solid Lightmap Material", EditCondition="bImportLightmaps", EditConditionHides))
    TSoftObjectPtr<UMaterialInterface> BSPWorldSolidLightmapMaterial;

    // Optional override for the liquid parent material. If unset, the importer creates/uses M_BSP_Transparent.
    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP World|Material", meta = (DisplayName="World Liquid Material"))
    TSoftObjectPtr<UMaterialInterface> BSPWorldLiquidMaterial;

    // Optional override for the sky parent material. If unset, the importer creates/uses an unlit master that drives Emissive from the texture.
    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP World|Material", meta = (DisplayName="World Sky Material"))
    TSoftObjectPtr<UMaterialInterface> BSPWorldSkyMaterial;

    // Collision profile for BSP chunk actors.
    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP World|Collision", meta = (DisplayName="World Solid Collision Profile"))
    FCollisionProfileName BSPWorldSolidCollisionProfile = FCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

    // Collision profile for water chunk actors.
    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP World|Collision", meta = (DisplayName="World Liquid Collision Profile"))
    FCollisionProfileName BSPLiquidCollisionProfile = FCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

    // Collision profile for sky chunk actors.
    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP World|Collision", meta = (DisplayName="World Sky Collision Profile"))
    FCollisionProfileName BSPSkyCollisionProfile = FCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP Entities", meta = (DisplayName="Import func_door"))
    bool bImportFuncDoors = true;
	
	UPROPERTY(EditAnywhere, Category = "Quake Import|BSP Entities", meta = (DisplayName="Import func_button"))
	bool bImportFuncButtons  = true;

    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP Entities", meta = (DisplayName="Import func_plat"))
    bool bImportFuncPlats = true;

    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP Entities", meta = (DisplayName="Import func_trigger"))
    bool bImportFuncTriggers = false;
    
    // Optional override for the opaque BSP parent material. If unset, the importer creates/uses M_BSP_Surface.
    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP Entities|Material", meta = (DisplayName="Entity Solid Material"))
    TSoftObjectPtr<UMaterialInterface> BSPEntitySolidMaterial;

    // Optional override for the opaque BSP entity parent material when importing BSP lightmaps.
    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP Entities|Material", meta=(DisplayName="Entity Solid Lightmap Material", EditCondition="bImportLightmaps", EditConditionHides))
    TSoftObjectPtr<UMaterialInterface> BSPEntitySolidLightmapMaterial;

    // Optional override for the water parent material. If unset, the importer creates/uses M_BSP_Transparent.
    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP Entities|Material", meta = (DisplayName="Entity Trigger Material"))
    TSoftObjectPtr<UMaterialInterface> BSPEntityTriggerMaterial;
    
    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP Entities|Collision", meta = (DisplayName="Entity Solid Collision Profile"))
    FCollisionProfileName BSPEntitySolidCollisionProfile = FCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    
    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP Entities|Collision", meta = (DisplayName="Entity Trigger Collision Profile"))
    FCollisionProfileName BSPEntityTriggerCollisionProfile = FCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
    
    // If enabled, the importer will spawn (or update) a Level Instance actor in the currently opened persistent level.
    // If disabled, only the generated level asset is updated; you can manually place a Level Instance that references it.
    UPROPERTY(EditAnywhere, Category = "Quake Import|Level Instances")
    bool bSpawnLevelInstanceActorInWorld = false;

    // Automatically save the generated level asset after reimport so placed Level Instances can immediately reload it.
    UPROPERTY(EditAnywhere, Category = "Quake Import|Level Instances")
    bool bAutoSaveGeneratedLevel = true;

    // Automatically unload/reload any placed Level Instance actors in the currently opened persistent world that reference the generated level.
    UPROPERTY(EditAnywhere, Category = "Quake Import|Level Instances")
    bool bAutoReloadPlacedLevelInstances = true;
    
    // Level asset that backs the generated Level Instance content (static mesh chunks only).
    UPROPERTY(VisibleAnywhere, Category = "Quake Import|Level Instances")
    TSoftObjectPtr<UWorld> GeneratedLevelBsp;

    // Stable id used to find the placed Level Instance actor in the persistent level.
    UPROPERTY(VisibleAnywhere, Category = "Quake Import|Level Instances")
    FGuid LevelInstanceId;
    
    // Level asset that backs the generated Level Instance content (brush entities).
    UPROPERTY(VisibleAnywhere, Category = "Quake Import|Level Instances")
    TSoftObjectPtr<UWorld> GeneratedLevelEntities;

    // Stable id used to find the placed Entities Level Instance actor in the persistent level.
    UPROPERTY(VisibleAnywhere, Category="Quake Import|Level Instances")
    FGuid EntitiesLevelInstanceId;
    
    // Generated mesh references were previously stored for convenience/debugging.
    // Removed to keep the asset UI lean.
};
