#pragma once

#include "CoreMinimal.h"
#include "BspFactory.h"
#include "UObject/SoftObjectPtr.h"
#include "Engine/EngineTypes.h"
#include "Engine/CollisionProfile.h"
#include "QuakeBspImportAsset.generated.h"

UCLASS(BlueprintType)
class QUAKEIMPORT_API UQuakeBspImportAsset : public UObject
{
    GENERATED_BODY()

public:
	UQuakeBspImportAsset();
    
    UFUNCTION(CallInEditor, Category="Quake Import", meta = (DisplayName="Import BSP World"))
    void ImportBSP();
    
    UFUNCTION(CallInEditor, Category="Quake Import", meta = (DisplayName="Import BSP Entities"))
    void ImportEntities();
    
    UPROPERTY(EditAnywhere, Category = "Quake Import", meta = (DisplayName="BSP File"))
    FFilePath BspFile;

    UPROPERTY(EditAnywhere, Category = "Quake Import")
    EWorldChunkMode WorldChunkMode = EWorldChunkMode::Grid;

    UPROPERTY(EditAnywhere, Category = "Quake Import", meta=(ClampMin="1", EditCondition="WorldChunkMode==EWorldChunkMode::Grid", EditConditionHides))
    int32 WorldChunkSize = 512;

    UPROPERTY(EditAnywhere, Category = "Quake Import", meta=(ClampMin="0.0001"))
    float ImportScale = 2.5f;

	// If enabled, the importer will overwrite existing generated textures and material instances on reimport.
	// Static meshes are still overwritten regardless.
	UPROPERTY(EditAnywhere, Category = "Quake Import|Materials")
	bool bOverwriteMaterialsAndTextures = false;
    
    // Include sky surfaces (textures starting with "sky") in the chunked BSP world geometry.
    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP World")
    bool bBspIncludeSky = false;

    // Include water/lava/slime surfaces (textures starting with "*") in the chunked BSP world geometry.
    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP World")
    bool bBspIncludeWater = false;

    // Optional override for the opaque BSP parent material. If unset, the importer creates/uses M_BSP_Surface.
    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP World|Material")
    TSoftObjectPtr<UMaterialInterface> BspParentMaterial;

    // Optional override for the water parent material. If unset, the importer creates/uses M_BSP_Transparent.
    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP World|Material")
    TSoftObjectPtr<UMaterialInterface> WaterParentMaterial;

    // Optional override for the sky parent material. If unset, the importer creates/uses an unlit master that drives Emissive from the texture.
    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP World|Material")
    TSoftObjectPtr<UMaterialInterface> SkyParentMaterial;

    // Collision profile for BSP chunk actors.
    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP World|Collision")
    FCollisionProfileName BspCollisionProfile = FCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

    // Collision profile for water chunk actors.
    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP World|Collision")
    FCollisionProfileName WaterCollisionProfile = FCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

    // Collision profile for sky chunk actors.
    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP World|Collision")
    FCollisionProfileName SkyCollisionProfile = FCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP Entities", meta = (DisplayName="Import func_door"))
    bool bImportFuncDoors = true;

    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP Entities", meta = (DisplayName="Import func_plat"))
    bool bImportFuncPlats = true;

    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP Entities", meta = (DisplayName="Import func_trigger"))
    bool bImportTriggers = true;
    
    // Optional override for the opaque BSP parent material. If unset, the importer creates/uses M_BSP_Surface.
    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP Entities|Material")
    TSoftObjectPtr<UMaterialInterface> BspEntitySolidMaterial;

    // Optional override for the water parent material. If unset, the importer creates/uses M_BSP_Transparent.
    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP Entities|Material")
    TSoftObjectPtr<UMaterialInterface> BspEntityTriggerMaterial;
    
    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP Entities|Collision")
    FCollisionProfileName BspEntitySolidCollisionProfile = FCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    
    UPROPERTY(EditAnywhere, Category = "Quake Import|BSP Entities|Collision")
    FCollisionProfileName BspEntityTriggerCollisionProfile = FCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
    
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
    
    // Imported world chunk meshes (opaque + transparent). Stored for convenience/debugging.
    UPROPERTY(VisibleAnywhere, Category = "Quake Import|Generated")
    TArray<TSoftObjectPtr<UStaticMesh>> GeneratedChunkMeshes;

    // Imported water chunk meshes.
    UPROPERTY(VisibleAnywhere, Category = "Quake Import|Generated")
    TArray<TSoftObjectPtr<UStaticMesh>> GeneratedWaterChunkMeshes;

    // Imported sky chunk meshes.
    UPROPERTY(VisibleAnywhere, Category = "Quake Import|Generated")
    TArray<TSoftObjectPtr<UStaticMesh>> GeneratedSkyChunkMeshes;
    
    // Imported entity meshes (one per brush entity). Stored for convenience/debugging.
    UPROPERTY(VisibleAnywhere, Category = "Quake Import|Generated")
    TArray<TSoftObjectPtr<UStaticMesh>> GeneratedEntityMeshes;
};
