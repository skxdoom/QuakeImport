#include "BspFactory.h"

#include "QuakeBspImportAsset.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY(LogQuakeImporter);

#define LOCTEXT_NAMESPACE "BspFactory"

UBspFactory::UBspFactory(const FObjectInitializer& ObjectInitializer):
Super(ObjectInitializer)
{
	SupportedClass = UQuakeBspImportAsset::StaticClass();
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

UObject* UBspFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName Name, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	bOutOperationCanceled = false;

	UQuakeBspImportAsset* Asset = NewObject<UQuakeBspImportAsset>(InParent, UQuakeBspImportAsset::StaticClass(), Name, Flags);
	if (!Asset)
	{
		UE_LOG(LogQuakeImporter, Error, TEXT("Failed to create Quake BSP Import Asset for '%s'"), *Filename);
		return nullptr;
	}

	Asset->BspFile.FilePath = MakeAbsolutePath(Filename);

	if (UPackage* Pkg = Asset->GetOutermost())
	{
		Pkg->MarkPackageDirty();
	}

	FAssetRegistryModule::AssetCreated(Asset);
	return Asset;
}

#undef LOCTEXT_NAMESPACE
