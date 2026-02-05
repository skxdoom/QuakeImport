#include "QuakeBSPLevelInstanceUtils.h"

#include "QuakeBSPImportAsset.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "Engine/Level.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/CollisionProfile.h"
#include "Factories/WorldFactory.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

#if __has_include("LevelInstance/LevelInstanceActor.h")
#include "LevelInstance/LevelInstanceActor.h"
#include "LevelInstance/LevelInstanceInterface.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#define QUAKEIMPORT_HAS_LEVELINSTANCE 1
#else
#define QUAKEIMPORT_HAS_LEVELINSTANCE 0
#endif

DEFINE_LOG_CATEGORY_STATIC(LogQuakeLevelInstance, Log, All);

namespace
{
	const FName GeneratedTag(TEXT("QBSP_Generated"));

	FName MakeLevelInstanceTag(const FGuid& Id)
	{
		return FName(*FString::Printf(TEXT("QBSP_LevelInstance_%s"), *Id.ToString(EGuidFormats::Digits)));
	}

	UWorld* GetEditorWorld()
	{
		if (!GEditor)
		{
			return nullptr;
		}
		return GEditor->GetEditorWorldContext().World();
	}

	UWorld* CreateOrLoadLevelAsset(const FString& LongPackagePath, const FString& LevelAssetName)
	{
		const FString LongPackageName = LongPackagePath / LevelAssetName;
		const FString ObjectPath = LongPackageName + TEXT(".") + LevelAssetName;

		UWorld* Existing = LoadObject<UWorld>(nullptr, *ObjectPath);
		if (Existing)
		{
			return Existing;
		}

		UPackage* Pkg = CreatePackage(*LongPackageName);
		if (!Pkg)
		{
			UE_LOG(LogQuakeLevelInstance, Error, TEXT("Failed to create package: %s"), *LongPackageName);
			return nullptr;
		}

		UWorldFactory* Factory = NewObject<UWorldFactory>();
		UObject* NewObj = Factory->FactoryCreateNew(UWorld::StaticClass(), Pkg, FName(*LevelAssetName),
		                                            RF_Public | RF_Standalone, nullptr, GWarn);
		UWorld* NewWorld = Cast<UWorld>(NewObj);
		if (!NewWorld)
		{
			UE_LOG(LogQuakeLevelInstance, Error, TEXT("Failed to create world asset: %s"), *LongPackageName);
			return nullptr;
		}

		FAssetRegistryModule::AssetCreated(NewWorld);
		Pkg->MarkPackageDirty();
		return NewWorld;
	}

	bool SaveLevelAsset(UWorld& LevelAsset)
	{
		UPackage* Pkg = LevelAsset.GetOutermost();
		if (!Pkg)
		{
			return false;
		}

		TArray<UPackage*> Packages;
		Packages.Add(Pkg);
		return UEditorLoadingAndSavingUtils::SavePackages(Packages, true);
	}

	TSoftObjectPtr<UWorld>& GetGeneratedLevelRef(UQuakeBSPImportAsset& ImportAsset,
	                                             QuakeLevelInstanceUtils::EGenLevelKind Kind)
	{
		if (Kind == QuakeLevelInstanceUtils::EGenLevelKind::Entities)
		{
			return ImportAsset.GeneratedLevelEntities;
		}
		return ImportAsset.GeneratedLevelBsp;
	}

	FGuid& GetLevelInstanceIdRef(UQuakeBSPImportAsset& ImportAsset, QuakeLevelInstanceUtils::EGenLevelKind Kind)
	{
		if (Kind == QuakeLevelInstanceUtils::EGenLevelKind::Entities)
		{
			return ImportAsset.EntitiesLevelInstanceId;
		}
		return ImportAsset.LevelInstanceId;
	}

	FString GetLevelAssetName(const FString& MapName, QuakeLevelInstanceUtils::EGenLevelKind Kind)
	{
		if (Kind == QuakeLevelInstanceUtils::EGenLevelKind::Entities)
		{
			return FString::Printf(TEXT("Map_%s_Entities"), *MapName);
		}
		return FString::Printf(TEXT("Map_%s_BSP"), *MapName);
	}

#if QUAKEIMPORT_HAS_LEVELINSTANCE
	void FindLevelInstancesReferencing(UWorld& EditorWorld, const UWorld& LevelAsset, TArray<ALevelInstance*>& Out)
	{
		Out.Reset();
		for (TActorIterator<ALevelInstance> It(&EditorWorld); It; ++It)
		{
			ALevelInstance* LI = *It;
			if (!LI)
			{
				continue;
			}
			if (LI->GetWorldAsset() == &LevelAsset)
			{
				Out.Add(LI);
			}
		}
	}

	ALevelInstance* FindLevelInstanceActorByTag(UWorld& EditorWorld, const FName Tag)
	{
		for (TActorIterator<ALevelInstance> It(&EditorWorld); It; ++It)
		{
			if (It->Tags.Contains(Tag))
			{
				return *It;
			}
		}
		return nullptr;
	}

	ALevelInstance* SpawnLevelInstanceActor(UWorld& EditorWorld, UWorld& LevelAsset, const FName Tag,
	                                        const FString& Label)
	{
		FActorSpawnParameters Params;
		Params.OverrideLevel = EditorWorld.PersistentLevel;
		Params.ObjectFlags = RF_Transactional;

		ALevelInstance* LI = EditorWorld.SpawnActor<ALevelInstance>(Params);
		if (!LI)
		{
			return nullptr;
		}

		LI->Tags.Add(Tag);
		LI->SetActorLabel(Label);
		LI->SetWorldAsset(&LevelAsset);
		return LI;
	}

	void ReloadLevelInstance(UWorld& EditorWorld, ILevelInstanceInterface& LII)
	{
		ULevelInstanceSubsystem* Subsys = EditorWorld.GetSubsystem<ULevelInstanceSubsystem>();
		if (!Subsys)
		{
			return;
		}

		if (Subsys->IsLoaded(&LII))
		{
			Subsys->RequestUnloadLevelInstance(&LII);
			EditorWorld.UpdateLevelStreaming();
			EditorWorld.FlushLevelStreaming(EFlushLevelStreamingType::Full);
		}

		Subsys->RequestLoadLevelInstance(&LII, true);
		EditorWorld.UpdateLevelStreaming();
		EditorWorld.FlushLevelStreaming(EFlushLevelStreamingType::Full);
	}
#endif
}

namespace QuakeLevelInstanceUtils
{
	bool EnsureGeneratedLevelReady(UQuakeBSPImportAsset& ImportAsset, const FString& MapName,
	                               const FString& FolderLongPackagePath, EGenLevelKind Kind, ULevel*& OutLoadedLevel)
	{
		OutLoadedLevel = nullptr;

		UWorld* EditorWorld = GetEditorWorld();
		if (!EditorWorld)
		{
			UE_LOG(LogQuakeLevelInstance, Error, TEXT("No editor world"));
			return false;
		}

		FGuid& Id = GetLevelInstanceIdRef(ImportAsset, Kind);
		if (!Id.IsValid())
		{
			ImportAsset.Modify();
			Id = FGuid::NewGuid();
		}

		const FString LevelFolder = FolderLongPackagePath / MapName;
		const FString LevelAssetName = GetLevelAssetName(MapName, Kind);

		TSoftObjectPtr<UWorld>& LevelRef = GetGeneratedLevelRef(ImportAsset, Kind);
		UWorld* LevelAsset = LevelRef.Get();
		if (!LevelAsset)
		{
			LevelAsset = CreateOrLoadLevelAsset(LevelFolder, LevelAssetName);
			if (!LevelAsset)
			{
				return false;
			}

			ImportAsset.Modify();
			LevelRef = LevelAsset;
		}

		if (!LevelAsset->PersistentLevel)
		{
			UE_LOG(LogQuakeLevelInstance, Error, TEXT("Generated level asset has no PersistentLevel"));
			return false;
		}

		OutLoadedLevel = LevelAsset->PersistentLevel;

#if QUAKEIMPORT_HAS_LEVELINSTANCE
		if (ImportAsset.bSpawnLevelInstanceActorInWorld)
		{
			const FName Tag = MakeLevelInstanceTag(Id);
			ALevelInstance* LI = FindLevelInstanceActorByTag(*EditorWorld, Tag);
			if (!LI)
			{
				const FString Label = LevelAssetName;
				LI = SpawnLevelInstanceActor(*EditorWorld, *LevelAsset, Tag, Label);
			}
			else if (LI->GetWorldAsset() != LevelAsset)
			{
				LI->Modify();
				LI->SetWorldAsset(LevelAsset);
			}

			if (LI)
			{
				if (ILevelInstanceInterface* LII = Cast<ILevelInstanceInterface>(LI))
				{
					ReloadLevelInstance(*EditorWorld, *LII);
				}
			}
		}
#endif

		return true;
	}

	void RefreshPlacedLevelInstances(UQuakeBSPImportAsset& ImportAsset, EGenLevelKind Kind)
	{
#if QUAKEIMPORT_HAS_LEVELINSTANCE
		if (!ImportAsset.bAutoSaveGeneratedLevel && !ImportAsset.bAutoReloadPlacedLevelInstances)
		{
			return;
		}

		UWorld* EditorWorld = GetEditorWorld();
		if (!EditorWorld)
		{
			return;
		}

		UWorld* LevelAsset = GetGeneratedLevelRef(ImportAsset, Kind).Get();
		if (!LevelAsset)
		{
			return;
		}

		if (ImportAsset.bAutoSaveGeneratedLevel)
		{
			SaveLevelAsset(*LevelAsset);
		}

		if (!ImportAsset.bAutoReloadPlacedLevelInstances)
		{
			return;
		}

		TArray<ALevelInstance*> Instances;
		FindLevelInstancesReferencing(*EditorWorld, *LevelAsset, Instances);
		for (ALevelInstance* LI : Instances)
		{
			if (ILevelInstanceInterface* LII = Cast<ILevelInstanceInterface>(LI))
			{
				ReloadLevelInstance(*EditorWorld, *LII);
			}
		}
#else
		(void)ImportAsset;
		(void)Kind;
#endif
	}

	void ClearGeneratedActors(ULevel& TargetLevel, EGenLevelKind Kind)
	{
		(void)Kind;
		TArray<AActor*> ToDestroy;
		for (AActor* A : TargetLevel.Actors)
		{
			if (!A)
			{
				continue;
			}
			if (A->Tags.Contains(GeneratedTag))
			{
				ToDestroy.Add(A);
			}
		}

		for (AActor* A : ToDestroy)
		{
			A->Modify();
			A->Destroy();
		}
	}

	static void PopulateLevelWithMeshesImpl(ULevel& TargetLevel, const TArray<FString>& StaticMeshObjectPaths,
	                                        const FName& CollisionProfileName, EGenLevelKind Kind)
	{
		(void)Kind;
		UWorld* World = TargetLevel.GetWorld();
		if (!World)
		{
			return;
		}

		const FName UseProfile = CollisionProfileName.IsNone()
			                         ? UCollisionProfile::BlockAll_ProfileName
			                         : CollisionProfileName;

		for (const FString& ObjPath : StaticMeshObjectPaths)
		{
			UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, *ObjPath);
			if (!SM)
			{
				continue;
			}

			FActorSpawnParameters Params;
			Params.OverrideLevel = &TargetLevel;
			Params.ObjectFlags = RF_Transactional;

			AStaticMeshActor* SMA = World->SpawnActor<AStaticMeshActor>(Params);
			if (!SMA)
			{
				continue;
			}

			SMA->Tags.Add(GeneratedTag);
			SMA->SetActorLabel(SM->GetName());

			UStaticMeshComponent* Comp = SMA->GetStaticMeshComponent();
			if (Comp)
			{
				Comp->SetStaticMesh(SM);
				Comp->SetMobility(EComponentMobility::Static);
				Comp->SetCollisionProfileName(UseProfile);
			}
		}
	}

	void PopulateLevelWithMeshes(ULevel& TargetLevel, const TArray<FString>& StaticMeshObjectPaths, EGenLevelKind Kind)
	{
		PopulateLevelWithMeshesImpl(TargetLevel, StaticMeshObjectPaths, UCollisionProfile::BlockAll_ProfileName, Kind);
	}

	void PopulateLevelWithMeshesWithCollision(ULevel& TargetLevel, const TArray<FString>& StaticMeshObjectPaths,
	                                          const FName& CollisionProfileName, EGenLevelKind Kind)
	{
		PopulateLevelWithMeshesImpl(TargetLevel, StaticMeshObjectPaths, CollisionProfileName, Kind);
	}
}
