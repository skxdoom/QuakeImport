#include "QuakeBSPImportAssetTypeActions.h"

#include "QuakeBSPImportAsset.h"

FText FQuakeBSPImportAssetTypeActions::GetName() const
{
    return NSLOCTEXT("QuakeImport", "QuakeBspImportAssetName", "Quake BSP Import Asset");
}

FColor FQuakeBSPImportAssetTypeActions::GetTypeColor() const
{
    return FColor(180, 60, 60);
}

UClass* FQuakeBSPImportAssetTypeActions::GetSupportedClass() const
{
    return UQuakeBSPImportAsset::StaticClass();
}

uint32 FQuakeBSPImportAssetTypeActions::GetCategories()
{
    return EAssetTypeCategories::Misc;
}
