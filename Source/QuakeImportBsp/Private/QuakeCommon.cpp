#include "QuakeCommon.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Interfaces/IPluginManager.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Engine/Texture2D.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/TextureFactory.h"
#include "Materials/Material.h"
#include "Materials/MaterialEditorOnlyData.h"
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

		// Reasonable sanity limits for Quake-era textures (also protects against corrupted files).
		if (width > 8192 || height > 8192)
		{
			return nullptr;
		}

		// The source data is 8-bit palette indices, one byte per pixel.
		const int64 PixelCount = int64(width) * int64(height);
		if (PixelCount <= 0)
		{
			return nullptr;
		}
		if (data.Num() != PixelCount)
		{
			return nullptr;
		}
        FString finalName = TEXT("T_") + name;

        if (UTexture2D* Existing = CheckIfAssetExist<UTexture2D>(finalName, texturePackage))
        {
            return Existing;
        }

        // get colors from palette
        TArray<uint8> finalData;

        for (const auto& it : data)
        {
            finalData.Add(pal[it].b);
            finalData.Add(pal[it].g);
            finalData.Add(pal[it].r);
            finalData.Add(255);
        }

        // Create Texture
        UTexture2D* texture = NewObject<UTexture2D>(&texturePackage, FName(*finalName), RF_Public | RF_Standalone);

        texture->SRGB = true;
        texture->Filter = TF_Nearest;
        texture->LODGroup = TEXTUREGROUP_Pixels2D;
        texture->NeverStream = true;

        FTexturePlatformData* platformData = new FTexturePlatformData();
        platformData->SizeX = width;
        platformData->SizeY = height;
        platformData->PixelFormat = PF_B8G8R8A8;
        texture->SetPlatformData(platformData);

        // Create first mip
        const int32 mipIndex = platformData->Mips.Add(new FTexture2DMipMap());
        FTexture2DMipMap* texmip = &platformData->Mips[mipIndex];
        texmip->SizeX = width;
        texmip->SizeY = height;
        texmip->BulkData.Lock(LOCK_READ_WRITE);
        uint32 textureDataSize = (width * height) * sizeof(uint8) * 4;
        uint8* textureData = (uint8*)texmip->BulkData.Realloc(textureDataSize);
        FMemory::Memcpy(textureData, finalData.GetData(), textureDataSize);
        texmip->BulkData.Unlock();

        texture->MipGenSettings = TMGS_NoMipmaps;
        texture->CompressionSettings = TextureCompressionSettings::TC_Default;
        texture->Source.Init(width, height, 1, 1, TSF_BGRA8, finalData.GetData());

        FAssetRegistryModule::AssetCreated(texture);

        texture->UpdateResource();
        texturePackage.MarkPackageDirty();

        return texture;
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

    UMaterialInstanceConstant* GetOrCreateMaterialInstance(const FString& instanceName, UPackage& materialPackage, UMaterial& parentMaterial, UTexture2D& albedoTexture)
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
