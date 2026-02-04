#pragma once

#include "CoreMinimal.h"

class UQuakeBspImportAsset;
class ULevel;

namespace QuakeLevelInstanceUtils
{
    enum class EGenLevelKind : uint8
    {
        BspWorld,
        Entities
    };

    // Ensure the import asset has a generated sublevel asset for the requested kind.
    // Returns the ULevel of that generated sublevel asset (the content source that Level Instances reference).
    bool EnsureGeneratedLevelReady(UQuakeBspImportAsset& ImportAsset, const FString& MapName, const FString& FolderLongPackagePath, EGenLevelKind Kind, ULevel*& OutLoadedLevel);

    // If placed Level Instances referencing the generated level asset exist in the currently opened persistent level,
    // unload/reload them so they reflect the latest saved state.
    void RefreshPlacedLevelInstances(UQuakeBspImportAsset& ImportAsset, EGenLevelKind Kind);

    // Deletes previously generated actors from the given level.
    void ClearGeneratedActors(ULevel& TargetLevel, EGenLevelKind Kind);

    // Spawns static mesh actors for the provided meshes in the given level and marks them as generated.
    void PopulateLevelWithMeshes(ULevel& TargetLevel, const TArray<FString>& StaticMeshObjectPaths, EGenLevelKind Kind);

    // Same as PopulateLevelWithMeshes, but also applies an explicit collision profile to each spawned component.
    void PopulateLevelWithMeshesWithCollision(ULevel& TargetLevel, const TArray<FString>& StaticMeshObjectPaths, const FName& CollisionProfileName, EGenLevelKind Kind);
}
