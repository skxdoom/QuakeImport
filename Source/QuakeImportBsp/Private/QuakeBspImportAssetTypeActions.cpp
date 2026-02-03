#include "QuakeBspImportAssetTypeActions.h"

#include "QuakeBspImportAsset.h"

FText FQuakeBspImportAssetTypeActions::GetName() const
{
    return NSLOCTEXT("QuakeImport", "QuakeBspImportAssetName", "Quake BSP Import Asset");
}

FColor FQuakeBspImportAssetTypeActions::GetTypeColor() const
{
    return FColor(180, 60, 60);
}

UClass* FQuakeBspImportAssetTypeActions::GetSupportedClass() const
{
    return UQuakeBspImportAsset::StaticClass();
}

uint32 FQuakeBspImportAssetTypeActions::GetCategories()
{
    return EAssetTypeCategories::Misc;
}
