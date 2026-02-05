#include "QuakeBSPImportAssetFactory.h"

#include "QuakeBSPImportAsset.h"

UQuakeBSPImportAssetFactory::UQuakeBSPImportAssetFactory()
{
    SupportedClass = UQuakeBSPImportAsset::StaticClass();
    bCreateNew = true;
    bEditAfterNew = true;
}

UObject* UQuakeBSPImportAssetFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
    return NewObject<UQuakeBSPImportAsset>(InParent, InClass, Name, Flags);
}
