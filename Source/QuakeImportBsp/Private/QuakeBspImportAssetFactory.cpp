#include "QuakeBspImportAssetFactory.h"

#include "QuakeBspImportAsset.h"

UQuakeBspImportAssetFactory::UQuakeBspImportAssetFactory()
{
    SupportedClass = UQuakeBspImportAsset::StaticClass();
    bCreateNew = true;
    bEditAfterNew = true;
}

UObject* UQuakeBspImportAssetFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
    return NewObject<UQuakeBspImportAsset>(InParent, InClass, Name, Flags);
}
