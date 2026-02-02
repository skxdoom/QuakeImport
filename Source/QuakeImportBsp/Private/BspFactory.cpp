// Fill out your copyright notice in the Description page of Project Settings.

#include "BspFactory.h"
#include "QuakeCommon.h"

// Epic
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Modules/ModuleManager.h"

// Quake Import
#include "BspUtilities.h"

DEFINE_LOG_CATEGORY(LogQuakeImporter);

#define LOCTEXT_NAMESPACE "BspFactory"

UBspFactory::UBspFactory(const FObjectInitializer& ObjectInitializer):
    Super(ObjectInitializer)
{
    SupportedClass = UStaticMesh::StaticClass();
    Formats.Add(TEXT("bsp;Quake BSP model files"));
    bCreateNew = false;
    bEditorImport = true;
}
/*
bool FindPlayerStart(const TArray<AttributeGroup>& entities)
{
    for (const auto& it : entities)
    {
        if (it.Get("classname") != nullptr)
        {
            if (it.Get("classname")->ToString() == "info_player_start")
            {
                return true;
            }
        }
    }

    return false;
}
*/
static void MakeImportPaths(UObject* InParent, const FName& Name, FString& OutRootPath, FString& OutMapPath)
{
    const FString ParentPath = InParent ? InParent->GetName() : TEXT("/Game");
    const FString MapName = Name.ToString();
    const FString Suffix = TEXT("/") + MapName;

    // Depending on how the user imported, InParent may already be the map folder (e.g. /Game/MyFolder/e1m1)
    // In that case, appending Name again would create /Game/MyFolder/e1m1/e1m1.
    if (ParentPath.EndsWith(Suffix))
    {
        OutMapPath = ParentPath;
        OutRootPath = FPackageName::GetLongPackagePath(ParentPath);
    }
    else
    {
        OutRootPath = ParentPath;
        OutMapPath = ParentPath / MapName;
    }
}

static UPackage* MakePackage(const FString& LongPackageName)
{
    return CreatePackage(*LongPackageName);
}

UObject* UBspFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName Name, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
    using namespace bsputils;
    bOutOperationCanceled = false;

    TArray<uint8> FileData;
    if (!FFileHelper::LoadFileToArray(FileData, *Filename))
    {
        UE_LOG(LogQuakeImporter, Error, TEXT("Failed to read bsp file '%s'"), *Filename);
        return nullptr;
    }

    const uint8* Buffer = FileData.GetData();

    FString RootPath;
    FString MapPath;
    MakeImportPaths(InParent, Name, RootPath, MapPath);

    // Textures and materials are shared across maps.
    // Default is sibling folders next to the map folder.
    const FString TexturesPath = RootPath / TEXT("Textures");
    const FString MaterialsPath = RootPath / TEXT("Materials");

    auto CreateAssetPackage = [](const FString& LongPkg) -> UPackage*
    {
        return CreatePackage(*LongPkg);
    };

    // Create Submodels
    BspLoader* loader = new BspLoader();
    loader->Load(Buffer, FileData.Num());
    const bspformat29::Bsp_29* model = loader->GetBspPtr();

    if (!model)
    {
        UE_LOG(LogQuakeImporter, Error, TEXT("Failed to import bsp file '%s'"), *Name.ToString());
        return nullptr;
    }

    // Load Palette
    TArray<QuakeCommon::QColor> quakePalette;
    if (!QuakeCommon::LoadPalette(quakePalette))
    {
        UE_LOG(LogQuakeImporter, Error, TEXT("Palette.lmp not found."));
        return nullptr;
    }

    // Create Textures and Materials
    TMap<FString, UMaterialInterface*> MaterialsByName;

    static auto SanitizeSurfaceNameForAsset = [](const FString& InName) -> FString
    {
        FString Out = InName;
        if (Out.StartsWith(TEXT("*")))
        {
            Out[0] = TCHAR('-');
        }

        for (int32 I = 0; I < Out.Len(); I++)
        {
            const TCHAR C = Out[I];
            if (!(FChar::IsAlnum(C) || C == TCHAR('_') || C == TCHAR('-')))
            {
                Out[I] = TCHAR('_');
            }
        }
        return Out;
    };

    static auto IsTransparentSurfaceName = [](const FString& TexName) -> bool
    {
        if (TexName.StartsWith(TEXT("*")))
        {
            return true;
        }
        return TexName.Equals(TEXT("trigger"), ESearchCase::IgnoreCase);
    };

    // Create (or reuse) master materials in the shared materials folder.
    // Instances will be shared across maps as well.
    const FString SurfaceMasterName = TEXT("M_BSP_Surface");
    UPackage* SurfaceMasterPkg = CreateAssetPackage(MaterialsPath / SurfaceMasterName);
    UMaterial* SurfaceMasterMat = QuakeCommon::GetOrCreateMasterMaterial(SurfaceMasterName, *SurfaceMasterPkg);

    const FString TransparentMasterName = TEXT("M_BSP_Transparent");
    UPackage* TransparentMasterPkg = CreateAssetPackage(MaterialsPath / TransparentMasterName);
    UMaterial* TransparentMasterMat = QuakeCommon::GetOrCreateTransparentMasterMaterial(TransparentMasterName, *TransparentMasterPkg);

    auto CreateMaterialForTextureName = [&](const FString& TextureName, const FString& SafeTextureName, UTexture2D* Texture) -> void
    {
        if (!Texture)
        {
            return;
        }

        const bool bTransparent = IsTransparentSurfaceName(TextureName);
        UMaterial* ParentMat = bTransparent ? TransparentMasterMat : SurfaceMasterMat;
        if (!ParentMat)
        {
            return;
        }

        const FString InstanceName = TEXT("MI_") + SafeTextureName;
        UPackage* MatPkg = CreateAssetPackage(MaterialsPath / InstanceName);
        UMaterialInstanceConstant* MI = QuakeCommon::GetOrCreateMaterialInstance(InstanceName, *MatPkg, *ParentMat, *Texture);
        if (MI)
        {
            MaterialsByName.Add(TextureName, MI);
        }
    };

    auto CreateTexturePackageAndTexture = [&](const FString& TexOriginalName, int32 W, int32 H, const TArray<uint8>& Src) -> UTexture2D*
    {
        const FString SafeBaseName = SanitizeSurfaceNameForAsset(TexOriginalName);
        const FString TexAssetName = TEXT("T_") + SafeBaseName;
        UPackage* TexPkg = CreateAssetPackage(TexturesPath / TexAssetName);
        return QuakeCommon::CreateUTexture2D(SafeBaseName, W, H, Src, *TexPkg, quakePalette);
    };

    for (const auto& ItTex : model->textures)
    {
        const FString SafeTexName = SanitizeSurfaceNameForAsset(ItTex.name);

        if (ItTex.name.StartsWith(TEXT("sky")))
        {
            TArray<uint8> Front;
            TArray<uint8> Back;
            Front.Reserve((ItTex.width / 2) * ItTex.height);
            Back.Reserve((ItTex.width / 2) * ItTex.height);

            for (unsigned y = 0; y < ItTex.height; y++)
            {
                for (unsigned x = 0; x < ItTex.width; x++)
                {
                    const unsigned pos = (y * ItTex.width) + x;
                    if (x < ItTex.width / 2)
                    {
                        Front.Add(ItTex.mip0[pos]);
                    }
                    else
                    {
                        Back.Add(ItTex.mip0[pos]);
                    }
                }
            }

            CreateTexturePackageAndTexture(SanitizeSurfaceNameForAsset(ItTex.name + TEXT("_front")), ItTex.width / 2, ItTex.height, Front);
            UTexture2D* BackTex = CreateTexturePackageAndTexture(SanitizeSurfaceNameForAsset(ItTex.name + TEXT("_back")), ItTex.width / 2, ItTex.height, Back);
            CreateMaterialForTextureName(ItTex.name, SafeTexName, BackTex);
            continue;
        }

        if (ItTex.name.StartsWith(TEXT("+0")))
        {
            TArray<uint8> Data;
            Data.Append(ItTex.mip0);

            int32 NumFrames = 1;
            while (AppendNextTextureData(ItTex.name, NumFrames, *model, Data))
            {
                NumFrames++;
            }

            UTexture2D* FlipTex = CreateTexturePackageAndTexture(ItTex.name, ItTex.width, ItTex.height * NumFrames, Data);
            CreateMaterialForTextureName(ItTex.name, SafeTexName, FlipTex);
            continue;
        }

        UTexture2D* Tex = CreateTexturePackageAndTexture(ItTex.name, ItTex.width, ItTex.height, ItTex.mip0);
        CreateMaterialForTextureName(ItTex.name, SafeTexName, Tex);
    }

    TArray<FString> WorldMeshObjectPaths;
    ModelToStaticmeshes(*model, MapPath, MaterialsByName, WorldChunkMode == EWorldChunkMode::Grid, WorldChunkSize, ImportScale, &WorldMeshObjectPaths);

    {
        FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        TArray<FString> Paths;
        Paths.Add(MapPath);
        Paths.Add(TexturesPath);
        Paths.Add(MaterialsPath);
        ARM.Get().ScanPathsSynchronous(Paths, true);
    }
    UObject* ReturnObject = nullptr;
    if (WorldMeshObjectPaths.Num() > 0)
    {
        ReturnObject = LoadObject<UStaticMesh>(nullptr, *WorldMeshObjectPaths[0]);
    }

    return ReturnObject;
}

#undef LOCTEXT_NAMESPACE
