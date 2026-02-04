#pragma once

#include "CoreMinimal.h"
#include "BspFactory.h"

namespace QuakeBspImportRunner
{
	bool ImportBspWorld(const FString& BspFilePath, const FString& TargetFolderLongPackagePath, EWorldChunkMode WorldChunkMode, int32 WorldChunkSize, float ImportScale, bool bIncludeSky, bool bIncludeWater, bool bOverwriteMaterialsAndTextures, class UMaterialInterface* BspParentOverride, class UMaterialInterface* WaterParentOverride, class UMaterialInterface* SkyParentOverride, const FName& BspCollisionProfile, const FName& WaterCollisionProfile, const FName& SkyCollisionProfile, TArray<FString>* OutBspMeshObjectPaths, TArray<FString>* OutWaterMeshObjectPaths, TArray<FString>* OutSkyMeshObjectPaths);

    // Imports brush entities (bmodels) into individual meshes grouped per entity.
	bool ImportBspEntities(const FString& BspFilePath, const FString& TargetFolderLongPackagePath, float ImportScale, bool bImportFuncDoors, bool bImportFuncPlats, bool bImportTriggers, bool bOverwriteMaterialsAndTextures, class UMaterialInterface* SolidParentOverride, class UMaterialInterface* WaterParentOverride, class UMaterialInterface* SkyParentOverride, class UMaterialInterface* TriggerParentOverride, const FName& SolidCollisionProfile, const FName& TriggerCollisionProfile, TArray<FString>* OutSolidEntityMeshObjectPaths, TArray<FString>* OutTriggerEntityMeshObjectPaths);
}
