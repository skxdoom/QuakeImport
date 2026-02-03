// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "QuakeImport.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "QuakeBspImportAssetTypeActions.h"

#define LOCTEXT_NAMESPACE "FQuakeImportModule"

void FQuakeImportModule::StartupModule()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	TSharedPtr<IAssetTypeActions> Actions = MakeShared<FQuakeBspImportAssetTypeActions>();
	AssetTools.RegisterAssetTypeActions(Actions.ToSharedRef());
	RegisteredAssetTypeActions.Add(Actions);
}

void FQuakeImportModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		for (const TSharedPtr<IAssetTypeActions>& Actions : RegisteredAssetTypeActions)
		{
			if (Actions.IsValid())
			{
				AssetTools.UnregisterAssetTypeActions(Actions.ToSharedRef());
			}
		}
	}
	RegisteredAssetTypeActions.Reset();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FQuakeImportModule, QuakeImport)