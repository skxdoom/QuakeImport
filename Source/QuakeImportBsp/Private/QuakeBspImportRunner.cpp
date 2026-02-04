#include "QuakeBspImportRunner.h"

#include "BspUtilities.h"
#include "QuakeCommon.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Engine/CollisionProfile.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"

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

	struct FLoadedBsp
	{
		TArray<uint8> FileData;
		bsputils::BspLoader Loader;
		const bsputils::bspformat29::Bsp_29* Model = nullptr;

		FString AbsPath;
		FString MapName;
		FString MapPath;
		FString TexturesPath;
		FString MaterialsPath;
	};

	bool LoadBspFile(const FString& BspFilePath, const FString& TargetFolderLongPackagePath, FLoadedBsp& Out)
	{
		Out.AbsPath = BspFilePath;

		if (FPaths::IsRelative(Out.AbsPath))
		{
			Out.AbsPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), Out.AbsPath);
		}

		if (!FPaths::FileExists(Out.AbsPath))
		{
			UE_LOG(LogQuakeImportRunner, Error, TEXT("BSP file not found: %s"), *Out.AbsPath);
			return false;
		}

		Out.MapName = FPaths::GetBaseFilename(Out.AbsPath);
		Out.MapPath = TargetFolderLongPackagePath / Out.MapName;
		Out.TexturesPath = TargetFolderLongPackagePath / TEXT("Textures");
		Out.MaterialsPath = TargetFolderLongPackagePath / TEXT("Materials");

		if (!FFileHelper::LoadFileToArray(Out.FileData, *Out.AbsPath))
		{
			UE_LOG(LogQuakeImportRunner, Error, TEXT("Failed to read bsp file: %s"), *Out.AbsPath);
			return false;
		}

		const uint8* Buffer = Out.FileData.GetData();
		Out.Loader.Load(Buffer, Out.FileData.Num());
		Out.Model = Out.Loader.GetBspPtr();

		if (!Out.Model)
		{
			UE_LOG(LogQuakeImportRunner, Error, TEXT("Failed to parse bsp file: %s"), *Out.AbsPath);
			return false;
		}

		return true;
	}

	bool EnsureMaterials(const bsputils::bspformat29::Bsp_29& Model, const FString& TexturesPath,
		const FString& MaterialsPath, bool bOverwriteMaterialsAndTextures, UMaterialInterface* BspParentOverride,
		UMaterialInterface* WaterParentOverride, UMaterialInterface* SkyParentOverride,
		UMaterialInterface* TriggerParentOverride, TMap<FString, UMaterialInterface*>& OutMaterialsByName)
	{
		TArray<QuakeCommon::QColor> QuakePalette;
		if (!QuakeCommon::LoadPalette(QuakePalette))
		{
			UE_LOG(LogQuakeImportRunner, Error, TEXT("Palette.lmp not found."));
			return false;
		}

		UMaterialInterface* SurfaceParent = BspParentOverride;
		if (!SurfaceParent)
		{
			const FString SurfaceMasterName = TEXT("M_BSP_Surface");
			UPackage* SurfaceMasterPkg = CreateAssetPackage(MaterialsPath / SurfaceMasterName);
			SurfaceParent = QuakeCommon::GetOrCreateMasterMaterial(SurfaceMasterName, *SurfaceMasterPkg);
		}

		UMaterialInterface* TransparentParent = WaterParentOverride;
		if (!TransparentParent)
		{
			const FString TransparentMasterName = TEXT("M_BSP_Transparent");
			UPackage* TransparentMasterPkg = CreateAssetPackage(MaterialsPath / TransparentMasterName);
			TransparentParent = QuakeCommon::GetOrCreateTransparentMasterMaterial(
				TransparentMasterName, *TransparentMasterPkg);
		}

		UMaterialInterface* SkyParent = SkyParentOverride;
		if (!SkyParent)
		{
			const FString SkyMasterName = TEXT("M_BSP_SkyUnlit");
			UPackage* SkyMasterPkg = CreateAssetPackage(MaterialsPath / SkyMasterName);
			SkyParent = QuakeCommon::GetOrCreateSkyUnlitMasterMaterial(SkyMasterName, *SkyMasterPkg);
		}

		auto CreateTexturePackageAndTexture = [&](const FString& TexOriginalName, int32 W, int32 H,
			const TArray<uint8>& Src) -> UTexture2D*
		{
			const FString SafeBaseName = SanitizeSurfaceNameForAsset(TexOriginalName);
			const FString TexAssetName = TEXT("T_") + SafeBaseName;
			UPackage* TexPkg = CreateAssetPackage(TexturesPath / TexAssetName);
			return QuakeCommon::CreateOrUpdateUTexture2D(SafeBaseName, W, H, Src, *TexPkg, QuakePalette, bOverwriteMaterialsAndTextures);
		};

		auto CreateMaterialForTextureName = [&](const FString& TextureName, const FString& SafeTextureName,
		                                        UTexture2D* Texture) -> void
		{
			if (!Texture)
			{
				return;
			}

			UMaterialInterface* ParentMat = nullptr;
			if (TriggerParentOverride && TextureName.StartsWith(TEXT("trigger"), ESearchCase::IgnoreCase))
			{
				ParentMat = TriggerParentOverride;
			}
			else if (TextureName.StartsWith(TEXT("sky")))
			{
				ParentMat = SkyParent;
			}
			else if (TextureName.StartsWith(TEXT("*")))
			{
				ParentMat = TransparentParent;
			}
			else
			{
				const bool bTransparent = IsTransparentSurfaceName(TextureName);
				ParentMat = bTransparent ? TransparentParent : SurfaceParent;
			}
			if (!ParentMat)
			{
				return;
			}

			const FString InstanceName = TEXT("MI_") + SafeTextureName;
			UPackage* MatPkg = CreateAssetPackage(MaterialsPath / InstanceName);
			UMaterialInstanceConstant* MI = QuakeCommon::GetOrCreateMaterialInstance(
				InstanceName, *MatPkg, (*ParentMat), *Texture, bOverwriteMaterialsAndTextures);
			if (MI)
			{
				OutMaterialsByName.Add(TextureName, MI);
			}
		};

		for (const auto& ItTex : Model.textures)
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

				CreateTexturePackageAndTexture(SanitizeSurfaceNameForAsset(ItTex.name + TEXT("_front")),
				                               ItTex.width / 2, ItTex.height, Front);
				UTexture2D* BackTex = CreateTexturePackageAndTexture(
					SanitizeSurfaceNameForAsset(ItTex.name + TEXT("_back")), ItTex.width / 2, ItTex.height, Back);
				CreateMaterialForTextureName(ItTex.name, SafeTexName, BackTex);
				continue;
			}

			if (ItTex.name.StartsWith(TEXT("+0")))
			{
				TArray<uint8> Data;
				Data.Append(ItTex.mip0);

				int32 NumFrames = 1;
				while (bsputils::AppendNextTextureData(ItTex.name, NumFrames, Model, Data))
				{
					NumFrames++;
				}

				UTexture2D* FlipTex = CreateTexturePackageAndTexture(ItTex.name, ItTex.width, ItTex.height * NumFrames,
				                                                     Data);
				CreateMaterialForTextureName(ItTex.name, SafeTexName, FlipTex);
				continue;
			}

			UTexture2D* Tex = CreateTexturePackageAndTexture(ItTex.name, ItTex.width, ItTex.height, ItTex.mip0);
			CreateMaterialForTextureName(ItTex.name, SafeTexName, Tex);
		}

		return true;
	}

	struct FParsedEntity
	{
		FString ClassName;
		int32 SubModelIndex = -1;
		int32 EntityIndex = -1;
	};

	bool ParseEntitiesForBmodels(const FString& EntitiesText, TArray<FParsedEntity>& Out)
	{
		Out.Reset();

		int32 EntityIdx = -1;
		int32 Pos = 0;
		while (Pos < EntitiesText.Len())
		{
			const int32 Open = EntitiesText.Find(TEXT("{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Pos);
			if (Open == INDEX_NONE)
			{
				break;
			}

			const int32 Close = EntitiesText.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart,
			                                      Open + 1);
			if (Close == INDEX_NONE)
			{
				break;
			}

			EntityIdx++;
			FString Block = EntitiesText.Mid(Open + 1, Close - Open - 1);
			Pos = Close + 1;

			auto FindKV = [&](const TCHAR* Key) -> FString
			{
				FString K(Key);
				int32 KeyPos = Block.Find(FString::Printf(TEXT("\"%s\""), *K));
				if (KeyPos == INDEX_NONE)
				{
					return FString();
				}
				int32 ValStart = Block.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart,
				                            KeyPos + K.Len() + 2);
				if (ValStart == INDEX_NONE)
				{
					return FString();
				}
				ValStart++;
				int32 ValEnd = Block.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, ValStart);
				if (ValEnd == INDEX_NONE)
				{
					return FString();
				}
				return Block.Mid(ValStart, ValEnd - ValStart);
			};

			FString ClassName = FindKV(TEXT("classname"));
			FString ModelStr = FindKV(TEXT("model"));
			if (ClassName.IsEmpty() || ModelStr.IsEmpty())
			{
				continue;
			}

			if (!ModelStr.StartsWith(TEXT("*")))
			{
				continue;
			}

			const int32 SubModelIndex = FCString::Atoi(*ModelStr.Mid(1));
			if (SubModelIndex <= 0)
			{
				continue;
			}

			FParsedEntity E;
			E.ClassName = ClassName;
			E.SubModelIndex = SubModelIndex;
			E.EntityIndex = EntityIdx;
			Out.Add(E);
		}

		return true;
	}
}

namespace QuakeBspImportRunner
{
	bool ImportBspWorld(const FString& BspFilePath, const FString& TargetFolderLongPackagePath,
		EWorldChunkMode WorldChunkMode, int32 WorldChunkSize, float ImportScale, bool bIncludeSky,
		bool bIncludeWater, bool bOverwriteMaterialsAndTextures, UMaterialInterface* BspParentOverride,
	                    UMaterialInterface* WaterParentOverride, UMaterialInterface* SkyParentOverride,
	                    const FName& BspCollisionProfile, const FName& WaterCollisionProfile,
	                    const FName& SkyCollisionProfile, TArray<FString>* OutBspMeshObjectPaths,
	                    TArray<FString>* OutWaterMeshObjectPaths, TArray<FString>* OutSkyMeshObjectPaths)
	{
		using namespace bsputils;

		FLoadedBsp Ctx;
		if (!LoadBspFile(BspFilePath, TargetFolderLongPackagePath, Ctx))
		{
			return false;
		}

		TMap<FString, UMaterialInterface*> MaterialsByName;
		if (!EnsureMaterials(*Ctx.Model, Ctx.TexturesPath, Ctx.MaterialsPath, bOverwriteMaterialsAndTextures, BspParentOverride, WaterParentOverride,
			SkyParentOverride, nullptr, MaterialsByName))
		{
			return false;
		}

		const FString WorldMeshesPath = Ctx.MapPath / TEXT("World");
		const bool bChunkWorld = (WorldChunkMode == EWorldChunkMode::Grid);
		ModelToStaticmeshes(*Ctx.Model, WorldMeshesPath, Ctx.MapName, MaterialsByName, bChunkWorld, WorldChunkSize,
		                    ImportScale, bIncludeSky, bIncludeWater, BspCollisionProfile, WaterCollisionProfile,
		                    SkyCollisionProfile, OutBspMeshObjectPaths, OutWaterMeshObjectPaths, OutSkyMeshObjectPaths);

		FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		TArray<FString> Paths;
		Paths.Add(WorldMeshesPath);
		Paths.Add(Ctx.TexturesPath);
		Paths.Add(Ctx.MaterialsPath);
		ARM.Get().ScanPathsSynchronous(Paths, true);

		return true;
	}

	bool ImportBspEntities(const FString& BspFilePath, const FString& TargetFolderLongPackagePath, float ImportScale,
		bool bImportFuncDoors, bool bImportFuncPlats, bool bImportTriggers, bool bOverwriteMaterialsAndTextures,
		UMaterialInterface* SolidParentOverride, UMaterialInterface* WaterParentOverride,
		UMaterialInterface* SkyParentOverride, UMaterialInterface* TriggerParentOverride,
		const FName& SolidCollisionProfile, const FName& TriggerCollisionProfile,
		TArray<FString>* OutSolidEntityMeshObjectPaths, TArray<FString>* OutTriggerEntityMeshObjectPaths)
	{
		using namespace bsputils;

		FLoadedBsp Ctx;
		if (!LoadBspFile(BspFilePath, TargetFolderLongPackagePath, Ctx))
		{
			return false;
		}

		TMap<FString, UMaterialInterface*> MaterialsByName;
		if (!EnsureMaterials(*Ctx.Model, Ctx.TexturesPath, Ctx.MaterialsPath, bOverwriteMaterialsAndTextures,
			SolidParentOverride, WaterParentOverride, SkyParentOverride, TriggerParentOverride, MaterialsByName))
		{
			return false;
		}

		TArray<FParsedEntity> Parsed;
		ParseEntitiesForBmodels(Ctx.Model->entities, Parsed);

		const FString EntitiesMeshesPath = Ctx.MapPath / TEXT("Entities");

		if (OutSolidEntityMeshObjectPaths)
		{
			OutSolidEntityMeshObjectPaths->Reset();
		}
		if (OutTriggerEntityMeshObjectPaths)
		{
			OutTriggerEntityMeshObjectPaths->Reset();
		}

		for (const FParsedEntity& E : Parsed)
		{
			const bool bIsDoor = E.ClassName.Equals(TEXT("func_door"), ESearchCase::IgnoreCase)
				|| E.ClassName.Equals(TEXT("func_door_secret"), ESearchCase::IgnoreCase)
				|| E.ClassName.Equals(TEXT("func_button"), ESearchCase::IgnoreCase)
				|| E.ClassName.Equals(TEXT("func_bossgate"), ESearchCase::IgnoreCase)
				|| E.ClassName.Equals(TEXT("func_episodegate"), ESearchCase::IgnoreCase);
			const bool bIsPlat = E.ClassName.Equals(TEXT("func_plat"), ESearchCase::IgnoreCase);
			const bool bIsTrigger = E.ClassName.StartsWith(TEXT("trigger"), ESearchCase::IgnoreCase);

			if (bIsDoor && !bImportFuncDoors)
			{
				continue;
			}
			if (bIsPlat && !bImportFuncPlats)
			{
				continue;
			}
			if (bIsTrigger && !bImportTriggers)
			{
				continue;
			}
			if (!bIsDoor && !bIsPlat && !bIsTrigger)
			{
				continue;
			}

			const FString SafeClass = SanitizeSurfaceNameForAsset(E.ClassName);
			const FString MeshName = FString::Printf(TEXT("SM_%s_BSP_Entity_%s_%d"), *Ctx.MapName, *SafeClass, E.EntityIndex);

			const FName UseCollisionProfile = bIsTrigger ? TriggerCollisionProfile : SolidCollisionProfile;

			FString ObjPath;
			if (!CreateSubmodelStaticMesh(*Ctx.Model, EntitiesMeshesPath, MeshName, uint8(E.SubModelIndex),
				MaterialsByName, ImportScale, UseCollisionProfile.IsNone() ? UCollisionProfile::BlockAll_ProfileName : UseCollisionProfile,
				ObjPath))
			{
				continue;
			}

			if (bIsTrigger)
			{
				if (OutTriggerEntityMeshObjectPaths)
				{
					OutTriggerEntityMeshObjectPaths->Add(ObjPath);
				}
			}
			else
			{
				if (OutSolidEntityMeshObjectPaths)
				{
					OutSolidEntityMeshObjectPaths->Add(ObjPath);
				}
			}
		}

		FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		TArray<FString> Paths;
		Paths.Add(EntitiesMeshesPath);
		Paths.Add(Ctx.TexturesPath);
		Paths.Add(Ctx.MaterialsPath);
		ARM.Get().ScanPathsSynchronous(Paths, true);

		return true;
	}
}
