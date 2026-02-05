#include "QuakeBSPAssetFactory.h"

#include "QuakeBSPImportAsset.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY(LogQuakeImporter);

#define LOCTEXT_NAMESPACE "BspFactory"

UQuakeBSPAssetFactory::UQuakeBSPAssetFactory(const FObjectInitializer& ObjectInitializer):
Super(ObjectInitializer)
{
	SupportedClass = UQuakeBSPImportAsset::StaticClass();
	Formats.Add(TEXT("bsp;Quake BSP map files"));
	bCreateNew = false;
	bEditorImport = true;
}

static FString MakeAbsolutePath(const FString& InFilename)
{
	FString FullFile = FPaths::ConvertRelativePathToFull(InFilename);
	FPaths::NormalizeFilename(FullFile);
	return FullFile;
}

UObject* UQuakeBSPAssetFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName Name, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	bOutOperationCanceled = false;

	UQuakeBSPImportAsset* Asset = NewObject<UQuakeBSPImportAsset>(InParent, UQuakeBSPImportAsset::StaticClass(), Name, Flags);
	if (!Asset)
	{
		UE_LOG(LogQuakeImporter, Error, TEXT("Failed to create Quake BSP Import Asset for '%s'"), *Filename);
		return nullptr;
	}

	Asset->BSPFile.FilePath = MakeAbsolutePath(Filename);

	if (UPackage* Pkg = Asset->GetOutermost())
	{
		Pkg->MarkPackageDirty();
	}

	FAssetRegistryModule::AssetCreated(Asset);
	return Asset;
}

#undef LOCTEXT_NAMESPACE
