#pragma once

#include "CoreMinimal.h"
#include "QuakeBSPImportAsset.h"

namespace QuakeBspImportRunner
{
	bool ImportBspWorld(const FString& BspFilePath, const FString& TargetFolderLongPackagePath, const FString& LitFilePath, EWorldChunkMode WorldChunkMode, int32 WorldChunkSize, float ImportScale, bool bIncludeSky, bool bIncludeWater, bool bImportLightmaps, bool bOverwriteMaterialsAndTextures, class UMaterialInterface* BspParentOverride, class UMaterialInterface* WaterParentOverride, class UMaterialInterface* SkyParentOverride, class UMaterialInterface* MaskedParentOverride, const FName& BspCollisionProfile, const FName& MaskedCollisionProfile, const FName& WaterCollisionProfile, const FName& SkyCollisionProfile, TArray<FString>* OutBspMeshObjectPaths, TArray<FString>* OutWaterMeshObjectPaths, TArray<FString>* OutSkyMeshObjectPaths);

    // Imports brush entities (bmodels) into individual meshes grouped per entity.
	bool ImportBspEntities(const FString& BspFilePath, const FString& TargetFolderLongPackagePath, const FString& LitFilePath, float ImportScale, bool bImportFuncDoors, bool bImportFuncPlats, bool bImportTriggers, bool bImportLightmaps, bool bOverwriteMaterialsAndTextures, class UMaterialInterface* SolidParentOverride, class UMaterialInterface* WaterParentOverride, class UMaterialInterface* SkyParentOverride, class UMaterialInterface* TriggerParentOverride, class UMaterialInterface* MaskedParentOverride, const FName& SolidCollisionProfile, const FName& MaskedCollisionProfile, const FName& TriggerCollisionProfile, TArray<FString>* OutSolidEntityMeshObjectPaths, TArray<FString>* OutTriggerEntityMeshObjectPaths);
}
