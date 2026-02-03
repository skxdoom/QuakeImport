#include "QuakeBspImportRunner.h"

#include "QuakeCommon.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"

#include "BspUtilities.h"

DEFINE_LOG_CATEGORY_STATIC(LogQuakeImportRunner, Log, All);

namespace
{
    FString SanitizeSurfaceNameForAsset(const FString& InName)
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
    }

    bool IsTransparentSurfaceName(const FString& TexName)
    {
        if (TexName.StartsWith(TEXT("*")))
        {
            return true;
        }
        return TexName.Equals(TEXT("trigger"), ESearchCase::IgnoreCase);
    }

    UPackage* CreateAssetPackage(const FString& LongPackageName)
    {
        UPackage* Pkg = CreatePackage(*LongPackageName);
        if (Pkg)
        {
            Pkg->MarkPackageDirty();
        }
        return Pkg;
    }
}

namespace QuakeBspImportRunner
{
    bool ImportBsp(const FString& BspFilePath, const FString& TargetFolderLongPackagePath, EWorldChunkMode WorldChunkMode, int32 WorldChunkSize, float ImportScale, TArray<FString>* OutWorldMeshObjectPaths)
    {
        using namespace bsputils;

        FString AbsPath = BspFilePath;
        if (!FPaths::IsRelative(AbsPath) && !FPaths::FileExists(AbsPath))
        {
            UE_LOG(LogQuakeImportRunner, Error, TEXT("BSP file not found: %s"), *AbsPath);
            return false;
        }

        if (FPaths::IsRelative(AbsPath))
        {
            AbsPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), AbsPath);
        }

        if (!FPaths::FileExists(AbsPath))
        {
            UE_LOG(LogQuakeImportRunner, Error, TEXT("BSP file not found: %s"), *AbsPath);
            return false;
        }

        const FString MapName = FPaths::GetBaseFilename(AbsPath);
        const FString MapPath = TargetFolderLongPackagePath / MapName;
        const FString TexturesPath = TargetFolderLongPackagePath / TEXT("Textures");
        const FString MaterialsPath = TargetFolderLongPackagePath / TEXT("Materials");

        TArray<uint8> FileData;
        if (!FFileHelper::LoadFileToArray(FileData, *AbsPath))
        {
            UE_LOG(LogQuakeImportRunner, Error, TEXT("Failed to read bsp file: %s"), *AbsPath);
            return false;
        }

        const uint8* Buffer = FileData.GetData();

        BspLoader Loader;
        Loader.Load(Buffer, FileData.Num());
        const bspformat29::Bsp_29* Model = Loader.GetBspPtr();

        if (!Model)
        {
            UE_LOG(LogQuakeImportRunner, Error, TEXT("Failed to parse bsp file: %s"), *AbsPath);
            return false;
        }

        TArray<QuakeCommon::QColor> QuakePalette;
        if (!QuakeCommon::LoadPalette(QuakePalette))
        {
            UE_LOG(LogQuakeImportRunner, Error, TEXT("Palette.lmp not found."));
            return false;
        }

        TMap<FString, UMaterialInterface*> MaterialsByName;

        const FString SurfaceMasterName = TEXT("M_BSP_Surface");
        UPackage* SurfaceMasterPkg = CreateAssetPackage(MaterialsPath / SurfaceMasterName);
        UMaterial* SurfaceMasterMat = QuakeCommon::GetOrCreateMasterMaterial(SurfaceMasterName, *SurfaceMasterPkg);

        const FString TransparentMasterName = TEXT("M_BSP_Transparent");
        UPackage* TransparentMasterPkg = CreateAssetPackage(MaterialsPath / TransparentMasterName);
        UMaterial* TransparentMasterMat = QuakeCommon::GetOrCreateTransparentMasterMaterial(TransparentMasterName, *TransparentMasterPkg);

        auto CreateTexturePackageAndTexture = [&](const FString& TexOriginalName, int32 W, int32 H, const TArray<uint8>& Src) -> UTexture2D*
        {
            const FString SafeBaseName = SanitizeSurfaceNameForAsset(TexOriginalName);
            const FString TexAssetName = TEXT("T_") + SafeBaseName;
            UPackage* TexPkg = CreateAssetPackage(TexturesPath / TexAssetName);
            return QuakeCommon::CreateUTexture2D(SafeBaseName, W, H, Src, *TexPkg, QuakePalette);
        };

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

        for (const auto& ItTex : Model->textures)
        {
            const FString SafeTexName = SanitizeSurfaceNameForAsset(ItTex.name);

            if (ItTex.name.StartsWith(TEXT("sky")))
            {
                TArray<uint8> Front;
                TArray<uint8> Back;
                Front.Reserve((ItTex.width / 2) * ItTex.height);
                Back.Reserve((ItTex.width / 2) * ItTex.height);

                for (unsigned Y = 0; Y < ItTex.height; Y++)
                {
                    for (unsigned X = 0; X < ItTex.width; X++)
                    {
                        const unsigned Pos = (Y * ItTex.width) + X;
                        if (X < ItTex.width / 2)
                        {
                            Front.Add(ItTex.mip0[Pos]);
                        }
                        else
                        {
                            Back.Add(ItTex.mip0[Pos]);
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
                while (bsputils::AppendNextTextureData(ItTex.name, NumFrames, *Model, Data))
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

        const bool bChunkWorld = (WorldChunkMode == EWorldChunkMode::Grid);
        ModelToStaticmeshes(*Model, MapPath, MapName, MaterialsByName, bChunkWorld, WorldChunkSize, ImportScale, OutWorldMeshObjectPaths);

        {
            FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
            TArray<FString> Paths;
            Paths.Add(MapPath);
            Paths.Add(TexturesPath);
            Paths.Add(MaterialsPath);
            ARM.Get().ScanPathsSynchronous(Paths, true);
        }

        return true;
    }
}
