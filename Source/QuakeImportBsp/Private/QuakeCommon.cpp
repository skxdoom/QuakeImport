#include "QuakeCommon.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Interfaces/IPluginManager.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Engine/Texture2D.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/TextureFactory.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/Package.h"

namespace QuakeCommon
{
    static const FName ColorParamName(TEXT("Color"));
    bool LoadPalette(TArray<QColor>& outPalette)
    {
        FString palFilename = IPluginManager::Get().FindPlugin(TEXT("QuakeImport"))->GetContentDir() / FString("palette.lmp");

        TArray<uint8> data;
        if (FFileHelper::LoadFileToArray(data, *palFilename))
        {
            int32 count = data.Num() / sizeof(QColor);
            outPalette.Empty();
            QColor* in = (QColor*)data.GetData();
            outPalette.Append(in, count);

            return true;
        }

        return false;
    }

	UTexture2D* CreateUTexture2D(const FString& name, int width, int height, const TArray<uint8>& data, UPackage& texturePackage, const TArray<QColor>& pal, bool savePackage)
	{
		// Defensive validation: some BSPs reference external WAD textures (or contain bad miptex headers)
		// which can yield invalid sizes and crash inside FTextureSource::Init.
		if (width <= 0 || height <= 0)
		{
			return nullptr;
		}
		if (width > 8192 || height > 8192)
		{
			return nullptr;
		}

		const int64 PixelCount = int64(width) * int64(height);
		if (PixelCount <= 0 || data.Num() != PixelCount)
		{
			return nullptr;
		}

		const FString FinalName = TEXT("T_") + name;
		if (UTexture2D* Existing = CheckIfAssetExist<UTexture2D>(FinalName, texturePackage))
		{
			return Existing;
		}

		TArray<uint8> FinalData;
		FinalData.Reserve(int32(PixelCount) * 4);
		for (const uint8& It : data)
		{
			FinalData.Add(pal[It].b);
			FinalData.Add(pal[It].g);
			FinalData.Add(pal[It].r);
			FinalData.Add(255);
		}

		UTexture2D* Texture = NewObject<UTexture2D>(&texturePackage, FName(*FinalName), RF_Public | RF_Standalone);
		if (!Texture)
		{
			return nullptr;
		}

		Texture->SRGB = true;
		Texture->Filter = TF_Nearest;
		Texture->LODGroup = TEXTUREGROUP_Pixels2D;
		Texture->NeverStream = true;

		FTexturePlatformData* PlatformData = new FTexturePlatformData();
		PlatformData->SizeX = width;
		PlatformData->SizeY = height;
		PlatformData->PixelFormat = PF_B8G8R8A8;
		Texture->SetPlatformData(PlatformData);

		const int32 MipIndex = PlatformData->Mips.Add(new FTexture2DMipMap());
		FTexture2DMipMap* TexMip = &PlatformData->Mips[MipIndex];
		TexMip->SizeX = width;
		TexMip->SizeY = height;
		TexMip->BulkData.Lock(LOCK_READ_WRITE);
		const uint32 TextureDataSize = (width * height) * sizeof(uint8) * 4;
		uint8* TextureData = (uint8*)TexMip->BulkData.Realloc(TextureDataSize);
		FMemory::Memcpy(TextureData, FinalData.GetData(), TextureDataSize);
		TexMip->BulkData.Unlock();

		Texture->MipGenSettings = TMGS_NoMipmaps;
		Texture->CompressionSettings = TextureCompressionSettings::TC_Default;
		Texture->Source.Init(width, height, 1, 1, TSF_BGRA8, FinalData.GetData());

		FAssetRegistryModule::AssetCreated(Texture);
		Texture->UpdateResource();
		texturePackage.MarkPackageDirty();
		return Texture;
	}

	UTexture2D* CreateOrUpdateUTexture2D(const FString& name, int width, int height, const TArray<uint8>& data, UPackage& texturePackage, const TArray<QColor>& pal, bool bOverwrite, bool savePackage)
	{
		if (!bOverwrite)
		{
			return CreateUTexture2D(name, width, height, data, texturePackage, pal, savePackage);
		}
		if (width <= 0 || height <= 0 || width > 8192 || height > 8192)
		{
			return nullptr;
		}

		const int64 PixelCount = int64(width) * int64(height);
		if (PixelCount <= 0 || data.Num() != PixelCount)
		{
			return nullptr;
		}

		const FString FinalName = TEXT("T_") + name;

		TArray<uint8> FinalData;
		FinalData.Reserve(int32(PixelCount) * 4);
		for (const uint8& It : data)
		{
			FinalData.Add(pal[It].b);
			FinalData.Add(pal[It].g);
			FinalData.Add(pal[It].r);
			FinalData.Add(255);
		}

		UTexture2D* Texture = CheckIfAssetExist<UTexture2D>(FinalName, texturePackage);
		if (!Texture)
		{
			Texture = NewObject<UTexture2D>(&texturePackage, FName(*FinalName), RF_Public | RF_Standalone);
			if (!Texture)
			{
				return nullptr;
			}
			FAssetRegistryModule::AssetCreated(Texture);
		}

		Texture->PreEditChange(nullptr);
		Texture->SRGB = true;
		Texture->Filter = TF_Nearest;
		Texture->LODGroup = TEXTUREGROUP_Pixels2D;
		Texture->NeverStream = true;
		Texture->MipGenSettings = TMGS_NoMipmaps;
		Texture->CompressionSettings = TextureCompressionSettings::TC_Default;

		FTexturePlatformData* PlatformData = Texture->GetPlatformData();
		if (!PlatformData)
		{
			PlatformData = new FTexturePlatformData();
			Texture->SetPlatformData(PlatformData);
		}
		PlatformData->SizeX = width;
		PlatformData->SizeY = height;
		PlatformData->PixelFormat = PF_B8G8R8A8;

		PlatformData->Mips.Empty();
		const int32 MipIndex = PlatformData->Mips.Add(new FTexture2DMipMap());
		FTexture2DMipMap* TexMip = &PlatformData->Mips[MipIndex];
		TexMip->SizeX = width;
		TexMip->SizeY = height;
		TexMip->BulkData.Lock(LOCK_READ_WRITE);
		const uint32 TextureDataSize = (width * height) * sizeof(uint8) * 4;
		uint8* TextureData = (uint8*)TexMip->BulkData.Realloc(TextureDataSize);
		FMemory::Memcpy(TextureData, FinalData.GetData(), TextureDataSize);
		TexMip->BulkData.Unlock();

		Texture->Source.Init(width, height, 1, 1, TSF_BGRA8, FinalData.GetData());
		Texture->UpdateResource();
		Texture->MarkPackageDirty();
		texturePackage.MarkPackageDirty();
		Texture->PostEditChange();
		return Texture;
	}

    void CreateUMaterial(const FString& materialName, UPackage& materialPackage, UTexture2D& initialTexture)
    {
        if (QuakeCommon::CheckIfAssetExist<UMaterial>(materialName, materialPackage))
        {
            return;
        }

        UMaterialFactoryNew* materialFactory = NewObject<UMaterialFactoryNew>();

        materialFactory->InitialTexture = &initialTexture;

        UMaterial* material = (UMaterial*)materialFactory->FactoryCreateNew(UMaterial::StaticClass(), &materialPackage, *materialName, RF_Standalone | RF_Public, NULL, GWarn);

        UMaterialExpressionConstant* specValue = NewObject<UMaterialExpressionConstant>(material);
        material->GetEditorOnlyData()->Specular.Connect(0, specValue);

        FAssetRegistryModule::AssetCreated(material);

        material->PreEditChange(NULL);
        material->MarkPackageDirty();
        materialPackage.SetDirtyFlag(true);
        material->PostEditChange();
    }

    UMaterial* GetOrCreateMasterMaterial(const FString& materialName, UPackage& materialPackage)
    {
        if (UMaterial* Existing = CheckIfAssetExist<UMaterial>(materialName, materialPackage))
        {
            return Existing;
        }

        UMaterial* Material = NewObject<UMaterial>(&materialPackage, FName(*materialName), RF_Public | RF_Standalone);
        if (!Material)
        {
            return nullptr;
        }

        Material->BlendMode = BLEND_Opaque;
        Material->SetShadingModel(MSM_DefaultLit);
        Material->TwoSided = false;

        UMaterialExpressionTextureSampleParameter2D* TexParam = NewObject<UMaterialExpressionTextureSampleParameter2D>(Material);
        TexParam->ParameterName = ColorParamName;
        TexParam->SamplerType = SAMPLERTYPE_Color;
        TexParam->MaterialExpressionEditorX = -400;
        TexParam->MaterialExpressionEditorY = 0;
        if (UMaterialEditorOnlyData* EditorOnly = Material->GetEditorOnlyData())
        {
            EditorOnly->ExpressionCollection.Expressions.Add(TexParam);
        }

        Material->GetEditorOnlyData()->BaseColor.Connect(0, TexParam);
        Material->GetEditorOnlyData()->Roughness.Constant = 1.0f;
        Material->GetEditorOnlyData()->Metallic.Constant = 0.0f;
        Material->GetEditorOnlyData()->Specular.Constant = 0.0f;

        FAssetRegistryModule::AssetCreated(Material);
        Material->PreEditChange(nullptr);
        Material->MarkPackageDirty();
        materialPackage.SetDirtyFlag(true);
        Material->PostEditChange();

        return Material;
    }

    UMaterial* GetOrCreateTransparentMasterMaterial(const FString& materialName, UPackage& materialPackage)
    {
        if (UMaterial* Existing = CheckIfAssetExist<UMaterial>(materialName, materialPackage))
        {
            return Existing;
        }

        UMaterial* Material = NewObject<UMaterial>(&materialPackage, FName(*materialName), RF_Public | RF_Standalone);
        if (!Material)
        {
            return nullptr;
        }

        Material->BlendMode = BLEND_Translucent;
        Material->SetShadingModel(MSM_DefaultLit);
        Material->TwoSided = false;

        UMaterialExpressionTextureSampleParameter2D* TexParam = NewObject<UMaterialExpressionTextureSampleParameter2D>(Material);
        TexParam->ParameterName = ColorParamName;
        TexParam->SamplerType = SAMPLERTYPE_Color;
        TexParam->MaterialExpressionEditorX = -400;
        TexParam->MaterialExpressionEditorY = 0;

        UMaterialExpressionConstant* OpacityConst = NewObject<UMaterialExpressionConstant>(Material);
        OpacityConst->R = 0.5f;
        OpacityConst->MaterialExpressionEditorX = -400;
        OpacityConst->MaterialExpressionEditorY = 200;

        if (UMaterialEditorOnlyData* EditorOnly = Material->GetEditorOnlyData())
        {
            EditorOnly->ExpressionCollection.Expressions.Add(TexParam);
            EditorOnly->ExpressionCollection.Expressions.Add(OpacityConst);

            EditorOnly->BaseColor.Connect(0, TexParam);
            EditorOnly->Opacity.Connect(0, OpacityConst);
            EditorOnly->Roughness.Constant = 1.0f;
            EditorOnly->Metallic.Constant = 0.0f;
            EditorOnly->Specular.Constant = 0.0f;
        }

        FAssetRegistryModule::AssetCreated(Material);
        Material->PreEditChange(nullptr);
        Material->MarkPackageDirty();
        materialPackage.SetDirtyFlag(true);
        Material->PostEditChange();

        return Material;
    }

    UMaterial* GetOrCreateSkyUnlitMasterMaterial(const FString& materialName, UPackage& materialPackage)
    {
        if (UMaterial* Existing = CheckIfAssetExist<UMaterial>(materialName, materialPackage))
        {
            return Existing;
        }

        UMaterial* Material = NewObject<UMaterial>(&materialPackage, FName(*materialName), RF_Public | RF_Standalone);
        if (!Material)
        {
            return nullptr;
        }

        Material->BlendMode = BLEND_Opaque;
        Material->SetShadingModel(MSM_Unlit);
        Material->TwoSided = false;

        UMaterialExpressionTextureSampleParameter2D* TexParam = NewObject<UMaterialExpressionTextureSampleParameter2D>(Material);
        TexParam->ParameterName = ColorParamName;
        TexParam->SamplerType = SAMPLERTYPE_Color;
        TexParam->MaterialExpressionEditorX = -400;
        TexParam->MaterialExpressionEditorY = 0;

        if (UMaterialEditorOnlyData* EditorOnly = Material->GetEditorOnlyData())
        {
            EditorOnly->ExpressionCollection.Expressions.Add(TexParam);
            EditorOnly->EmissiveColor.Connect(0, TexParam);
            EditorOnly->Roughness.Constant = 1.0f;
            EditorOnly->Metallic.Constant = 0.0f;
            EditorOnly->Specular.Constant = 0.0f;
        }

        FAssetRegistryModule::AssetCreated(Material);
        Material->PreEditChange(nullptr);
        Material->MarkPackageDirty();
        materialPackage.SetDirtyFlag(true);
        Material->PostEditChange();

        return Material;
    }

    UMaterialInstanceConstant* GetOrCreateMaterialInstance(const FString& instanceName, UPackage& materialPackage, UMaterial& parentMaterial, UTexture2D& albedoTexture)
    {
        return GetOrCreateMaterialInstance(instanceName, materialPackage, (UMaterialInterface&)parentMaterial, albedoTexture);
    }

	UMaterialInstanceConstant* GetOrCreateMaterialInstance(const FString& instanceName, UPackage& materialPackage, UMaterial& parentMaterial, UTexture2D& albedoTexture, bool bOverwrite)
	{
		return GetOrCreateMaterialInstance(instanceName, materialPackage, (UMaterialInterface&)parentMaterial, albedoTexture, bOverwrite);
	}

    UMaterialInstanceConstant* GetOrCreateMaterialInstance(const FString& instanceName, UPackage& materialPackage, UMaterialInterface& parentMaterial, UTexture2D& albedoTexture)
    {
        if (UMaterialInstanceConstant* Existing = CheckIfAssetExist<UMaterialInstanceConstant>(instanceName, materialPackage))
        {
            return Existing;
        }

        UMaterialInstanceConstant* MI = NewObject<UMaterialInstanceConstant>(&materialPackage, FName(*instanceName), RF_Public | RF_Standalone);
        if (!MI)
        {
            return nullptr;
        }

        MI->SetParentEditorOnly(&parentMaterial);
        MI->PreEditChange(nullptr);

        const FMaterialParameterInfo Info(ColorParamName);
        MI->SetTextureParameterValueEditorOnly(Info, &albedoTexture);

        FAssetRegistryModule::AssetCreated(MI);
        MI->MarkPackageDirty();
        materialPackage.SetDirtyFlag(true);
        MI->PostEditChange();
        return MI;
    }

	UMaterialInstanceConstant* GetOrCreateMaterialInstance(const FString& instanceName, UPackage& materialPackage, UMaterialInterface& parentMaterial, UTexture2D& albedoTexture, bool bOverwrite)
	{
		if (UMaterialInstanceConstant* Existing = CheckIfAssetExist<UMaterialInstanceConstant>(instanceName, materialPackage))
		{
			if (!bOverwrite)
			{
				return Existing;
			}

			Existing->PreEditChange(nullptr);
			Existing->SetParentEditorOnly(&parentMaterial);
			const FMaterialParameterInfo Info(ColorParamName);
			Existing->SetTextureParameterValueEditorOnly(Info, &albedoTexture);
			Existing->MarkPackageDirty();
			materialPackage.SetDirtyFlag(true);
			Existing->PostEditChange();
			return Existing;
		}

		UMaterialInstanceConstant* MI = NewObject<UMaterialInstanceConstant>(&materialPackage, FName(*instanceName), RF_Public | RF_Standalone);
		if (!MI)
		{
			return nullptr;
		}

		MI->SetParentEditorOnly(&parentMaterial);
		MI->PreEditChange(nullptr);

		const FMaterialParameterInfo Info(ColorParamName);
		MI->SetTextureParameterValueEditorOnly(Info, &albedoTexture);

		FAssetRegistryModule::AssetCreated(MI);
		MI->MarkPackageDirty();
		materialPackage.SetDirtyFlag(true);
		MI->PostEditChange();
		return MI;
	}

    void SaveAsset(UObject& object, UPackage& package)
    {
        const FString filename = FPackageName::LongPackageNameToFilename(
            package.GetName(),
            FPackageName::GetAssetPackageExtension()
        );

        FSavePackageArgs args;
        args.TopLevelFlags = RF_Public | RF_Standalone;
        args.SaveFlags = SAVE_NoError;
        args.Error = GError;

        UPackage::SavePackage(&package, &object, *filename, args);
    }

    void SavePackage(UPackage& package)
    {
        const FString filename = FPackageName::LongPackageNameToFilename(
            package.GetName(),
            FPackageName::GetAssetPackageExtension()
        );

        FSavePackageArgs args;
        args.TopLevelFlags = RF_Public | RF_Standalone;
        args.SaveFlags = SAVE_NoError;
        args.Error = GError;

        UPackage::SavePackage(&package, nullptr, *filename, args);
    }

} // namespace QuakeCommon
