#include "QuakeBSPImportAsset.h"

#include "QuakeBSPImportRunner.h"
#include "QuakeBSPLevelInstanceUtils.h"

#include "Misc/PackageName.h"
#include "Misc/Paths.h"

#include "UObject/ConstructorHelpers.h"

UQuakeBSPImportAsset::UQuakeBSPImportAsset()
{
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatSolid(TEXT("/QuakeImport/M_BSP_Solid.M_BSP_Solid"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatSolidLightmap(TEXT("/QuakeImport/M_BSP_Solid_Lightmap.M_BSP_Solid_Lightmap"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatWater(TEXT("/QuakeImport/M_BSP_Liquid.M_BSP_Liquid"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatSky(TEXT("/QuakeImport/M_BSP_Sky.M_BSP_Sky"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatTrigger(TEXT("/QuakeImport/M_BSP_Trigger.M_BSP_Trigger"));

	if (MatSolid.Succeeded())
	{
		BSPWorldSolidMaterial = MatSolid.Object;
		BSPEntitySolidMaterial = MatSolid.Object;
	}
	if (MatSolidLightmap.Succeeded())
	{
		BSPWorldSolidLightmapMaterial = MatSolidLightmap.Object;
		BSPEntitySolidLightmapMaterial = MatSolidLightmap.Object;
	}
	if (MatSolidLightmap.Succeeded())
	{
		BSPWorldSolidLightmapMaterial = MatSolidLightmap.Object;
		BSPEntitySolidLightmapMaterial = MatSolidLightmap.Object;
	}
	if (MatWater.Succeeded())
	{
		BSPWorldLiquidMaterial = MatWater.Object;
	}
	if (MatSky.Succeeded())
	{
		BSPWorldSkyMaterial = MatSky.Object;
	}
	if (MatTrigger.Succeeded())
	{
		BSPEntityTriggerMaterial = MatTrigger.Object;
	}
}

void UQuakeBSPImportAsset::ImportBSP()
{
const FString PackageName = GetOutermost() ? GetOutermost()->GetName() : TEXT("/Game");
const FString FolderPath = FPackageName::GetLongPackagePath(PackageName);
const FString MapName = FPaths::GetBaseFilename(BSPFile.FilePath);

TArray<FString> BspMeshes;
TArray<FString> WaterMeshes;
TArray<FString> SkyMeshes;

UMaterialInterface* BspParent = bImportLightmaps ? BSPWorldSolidLightmapMaterial.LoadSynchronous() : BSPWorldSolidMaterial.LoadSynchronous();
if (!BspParent)
{
BspParent = BSPWorldSolidMaterial.LoadSynchronous();
}
UMaterialInterface* WaterParent = BSPWorldLiquidMaterial.LoadSynchronous();
UMaterialInterface* SkyParent = BSPWorldSkyMaterial.LoadSynchronous();

	if (!QuakeBspImportRunner::ImportBspWorld(BSPFile.FilePath, FolderPath, WorldChunkMode, WorldChunkSize, ImportScale, bBSPWorldImportSky, bBSPWorldImportLiquids, bImportLightmaps, bOverwriteMaterialsAndTextures, BspParent, WaterParent, SkyParent, BSPWorldSolidCollisionProfile.Name, BSPLiquidCollisionProfile.Name, BSPSkyCollisionProfile.Name, &BspMeshes, &WaterMeshes, &SkyMeshes))
{
return;
}

MarkPackageDirty();

ULevel* TargetLevel = nullptr;
if (!QuakeLevelInstanceUtils::EnsureGeneratedLevelReady(*this, MapName, FolderPath, QuakeLevelInstanceUtils::EGenLevelKind::BspWorld, TargetLevel) || !TargetLevel)
{
return;
}

QuakeLevelInstanceUtils::ClearGeneratedActors(*TargetLevel, QuakeLevelInstanceUtils::EGenLevelKind::BspWorld);
QuakeLevelInstanceUtils::PopulateLevelWithMeshesWithCollision(*TargetLevel, BspMeshes, BSPWorldSolidCollisionProfile.Name, QuakeLevelInstanceUtils::EGenLevelKind::BspWorld);
QuakeLevelInstanceUtils::PopulateLevelWithMeshesWithCollision(*TargetLevel, WaterMeshes, BSPLiquidCollisionProfile.Name, QuakeLevelInstanceUtils::EGenLevelKind::BspWorld);
QuakeLevelInstanceUtils::PopulateLevelWithMeshesWithCollision(*TargetLevel, SkyMeshes, BSPSkyCollisionProfile.Name, QuakeLevelInstanceUtils::EGenLevelKind::BspWorld);
QuakeLevelInstanceUtils::RefreshPlacedLevelInstances(*this, QuakeLevelInstanceUtils::EGenLevelKind::BspWorld);
}

void UQuakeBSPImportAsset::ImportEntities()
{
const FString PackageName = GetOutermost() ? GetOutermost()->GetName() : TEXT("/Game");
const FString FolderPath = FPackageName::GetLongPackagePath(PackageName);
const FString MapName = FPaths::GetBaseFilename(BSPFile.FilePath);

	TArray<FString> SolidEntityMeshes;
	TArray<FString> TriggerEntityMeshes;

	UMaterialInterface* SolidParent = bImportLightmaps ? BSPEntitySolidLightmapMaterial.LoadSynchronous() : BSPEntitySolidMaterial.LoadSynchronous();
	if (!SolidParent)
	{
		SolidParent = BSPEntitySolidMaterial.LoadSynchronous();
	}
	UMaterialInterface* TriggerParent = BSPEntityTriggerMaterial.LoadSynchronous();
	UMaterialInterface* WaterParent = BSPWorldLiquidMaterial.LoadSynchronous();
	UMaterialInterface* SkyParent = BSPWorldSkyMaterial.LoadSynchronous();

	if (!QuakeBspImportRunner::ImportBspEntities(BSPFile.FilePath, FolderPath, ImportScale, bImportFuncDoors, bImportFuncPlats, bImportFuncTriggers, bImportLightmaps, bOverwriteMaterialsAndTextures, SolidParent, WaterParent, SkyParent, TriggerParent, BSPEntitySolidCollisionProfile.Name, BSPEntityTriggerCollisionProfile.Name, &SolidEntityMeshes, &TriggerEntityMeshes))
{
return;
}

	TArray<FString> AllEntityMeshes;
	AllEntityMeshes.Reserve(SolidEntityMeshes.Num() + TriggerEntityMeshes.Num());
	AllEntityMeshes.Append(SolidEntityMeshes);
	AllEntityMeshes.Append(TriggerEntityMeshes);

MarkPackageDirty();

ULevel* TargetLevel = nullptr;
if (!QuakeLevelInstanceUtils::EnsureGeneratedLevelReady(*this, MapName, FolderPath, QuakeLevelInstanceUtils::EGenLevelKind::Entities, TargetLevel) || !TargetLevel)
{
return;
}

	QuakeLevelInstanceUtils::ClearGeneratedActors(*TargetLevel, QuakeLevelInstanceUtils::EGenLevelKind::Entities);
	QuakeLevelInstanceUtils::PopulateLevelWithMeshesWithCollision(*TargetLevel, SolidEntityMeshes, BSPEntitySolidCollisionProfile.Name, QuakeLevelInstanceUtils::EGenLevelKind::Entities);
	QuakeLevelInstanceUtils::PopulateLevelWithMeshesWithCollision(*TargetLevel, TriggerEntityMeshes, BSPEntityTriggerCollisionProfile.Name, QuakeLevelInstanceUtils::EGenLevelKind::Entities);
QuakeLevelInstanceUtils::RefreshPlacedLevelInstances(*this, QuakeLevelInstanceUtils::EGenLevelKind::Entities);
}
