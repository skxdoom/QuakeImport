#include "QuakeBspImportAsset.h"

#include "QuakeBspImportRunner.h"
#include "QuakeLevelInstanceUtils.h"

#include "Misc/PackageName.h"
#include "Misc/Paths.h"

#include "UObject/ConstructorHelpers.h"

UQuakeBspImportAsset::UQuakeBspImportAsset()
{
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatSolid(TEXT("/QuakeImport/M_BSP_Solid.M_BSP_Solid"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatWater(TEXT("/QuakeImport/M_BSP_Water.M_BSP_Water"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatSky(TEXT("/QuakeImport/M_BSP_Sky.M_BSP_Sky"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatTrigger(TEXT("/QuakeImport/M_BSP_Trigger.M_BSP_Trigger"));

	if (MatSolid.Succeeded())
	{
		BspParentMaterial = MatSolid.Object;
		BspEntitySolidMaterial = MatSolid.Object;
	}
	if (MatWater.Succeeded())
	{
		WaterParentMaterial = MatWater.Object;
	}
	if (MatSky.Succeeded())
	{
		SkyParentMaterial = MatSky.Object;
	}
	if (MatTrigger.Succeeded())
	{
		BspEntityTriggerMaterial = MatTrigger.Object;
	}
}

void UQuakeBspImportAsset::ImportBSP()
{
const FString PackageName = GetOutermost() ? GetOutermost()->GetName() : TEXT("/Game");
const FString FolderPath = FPackageName::GetLongPackagePath(PackageName);
const FString MapName = FPaths::GetBaseFilename(BspFile.FilePath);

TArray<FString> BspMeshes;
TArray<FString> WaterMeshes;
TArray<FString> SkyMeshes;

UMaterialInterface* BspParent = BspParentMaterial.LoadSynchronous();
UMaterialInterface* WaterParent = WaterParentMaterial.LoadSynchronous();
UMaterialInterface* SkyParent = SkyParentMaterial.LoadSynchronous();

	if (!QuakeBspImportRunner::ImportBspWorld(BspFile.FilePath, FolderPath, WorldChunkMode, WorldChunkSize, ImportScale, bBspIncludeSky, bBspIncludeWater, bOverwriteMaterialsAndTextures, BspParent, WaterParent, SkyParent, BspCollisionProfile.Name, WaterCollisionProfile.Name, SkyCollisionProfile.Name, &BspMeshes, &WaterMeshes, &SkyMeshes))
{
return;
}

GeneratedChunkMeshes.Reset();
GeneratedChunkMeshes.Reserve(BspMeshes.Num());
for (const FString& ObjPath : BspMeshes)
{
GeneratedChunkMeshes.Add(TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(ObjPath)));
}

GeneratedWaterChunkMeshes.Reset();
GeneratedWaterChunkMeshes.Reserve(WaterMeshes.Num());
for (const FString& ObjPath : WaterMeshes)
{
GeneratedWaterChunkMeshes.Add(TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(ObjPath)));
}

GeneratedSkyChunkMeshes.Reset();
GeneratedSkyChunkMeshes.Reserve(SkyMeshes.Num());
for (const FString& ObjPath : SkyMeshes)
{
GeneratedSkyChunkMeshes.Add(TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(ObjPath)));
}
MarkPackageDirty();

ULevel* TargetLevel = nullptr;
if (!QuakeLevelInstanceUtils::EnsureGeneratedLevelReady(*this, MapName, FolderPath, QuakeLevelInstanceUtils::EGenLevelKind::BspWorld, TargetLevel) || !TargetLevel)
{
return;
}

QuakeLevelInstanceUtils::ClearGeneratedActors(*TargetLevel, QuakeLevelInstanceUtils::EGenLevelKind::BspWorld);
QuakeLevelInstanceUtils::PopulateLevelWithMeshesWithCollision(*TargetLevel, BspMeshes, BspCollisionProfile.Name, QuakeLevelInstanceUtils::EGenLevelKind::BspWorld);
QuakeLevelInstanceUtils::PopulateLevelWithMeshesWithCollision(*TargetLevel, WaterMeshes, WaterCollisionProfile.Name, QuakeLevelInstanceUtils::EGenLevelKind::BspWorld);
QuakeLevelInstanceUtils::PopulateLevelWithMeshesWithCollision(*TargetLevel, SkyMeshes, SkyCollisionProfile.Name, QuakeLevelInstanceUtils::EGenLevelKind::BspWorld);
QuakeLevelInstanceUtils::RefreshPlacedLevelInstances(*this, QuakeLevelInstanceUtils::EGenLevelKind::BspWorld);
}

void UQuakeBspImportAsset::ImportEntities()
{
const FString PackageName = GetOutermost() ? GetOutermost()->GetName() : TEXT("/Game");
const FString FolderPath = FPackageName::GetLongPackagePath(PackageName);
const FString MapName = FPaths::GetBaseFilename(BspFile.FilePath);

	TArray<FString> SolidEntityMeshes;
	TArray<FString> TriggerEntityMeshes;

	UMaterialInterface* SolidParent = BspEntitySolidMaterial.LoadSynchronous();
	UMaterialInterface* TriggerParent = BspEntityTriggerMaterial.LoadSynchronous();
	UMaterialInterface* WaterParent = WaterParentMaterial.LoadSynchronous();
	UMaterialInterface* SkyParent = SkyParentMaterial.LoadSynchronous();

	if (!QuakeBspImportRunner::ImportBspEntities(BspFile.FilePath, FolderPath, ImportScale, bImportFuncDoors, bImportFuncPlats, bImportTriggers, bOverwriteMaterialsAndTextures, SolidParent, WaterParent, SkyParent, TriggerParent, BspEntitySolidCollisionProfile.Name, BspEntityTriggerCollisionProfile.Name, &SolidEntityMeshes, &TriggerEntityMeshes))
{
return;
}

	TArray<FString> AllEntityMeshes;
	AllEntityMeshes.Reserve(SolidEntityMeshes.Num() + TriggerEntityMeshes.Num());
	AllEntityMeshes.Append(SolidEntityMeshes);
	AllEntityMeshes.Append(TriggerEntityMeshes);

	GeneratedEntityMeshes.Reset();
	GeneratedEntityMeshes.Reserve(AllEntityMeshes.Num());
	for (const FString& ObjPath : AllEntityMeshes)
{
GeneratedEntityMeshes.Add(TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(ObjPath)));
}
MarkPackageDirty();

ULevel* TargetLevel = nullptr;
if (!QuakeLevelInstanceUtils::EnsureGeneratedLevelReady(*this, MapName, FolderPath, QuakeLevelInstanceUtils::EGenLevelKind::Entities, TargetLevel) || !TargetLevel)
{
return;
}

	QuakeLevelInstanceUtils::ClearGeneratedActors(*TargetLevel, QuakeLevelInstanceUtils::EGenLevelKind::Entities);
	QuakeLevelInstanceUtils::PopulateLevelWithMeshesWithCollision(*TargetLevel, SolidEntityMeshes, BspEntitySolidCollisionProfile.Name, QuakeLevelInstanceUtils::EGenLevelKind::Entities);
	QuakeLevelInstanceUtils::PopulateLevelWithMeshesWithCollision(*TargetLevel, TriggerEntityMeshes, BspEntityTriggerCollisionProfile.Name, QuakeLevelInstanceUtils::EGenLevelKind::Entities);
QuakeLevelInstanceUtils::RefreshPlacedLevelInstances(*this, QuakeLevelInstanceUtils::EGenLevelKind::Entities);
}
