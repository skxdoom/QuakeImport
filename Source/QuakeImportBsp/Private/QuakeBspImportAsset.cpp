#include "QuakeBspImportAsset.h"

#include "QuakeBspImportRunner.h"

#include "Misc/PackageName.h"

void UQuakeBspImportAsset::Import()
{
    const FString PackageName = GetOutermost() ? GetOutermost()->GetName() : TEXT("/Game");
    const FString FolderPath = FPackageName::GetLongPackagePath(PackageName);

    TArray<FString> WorldMeshes;
    QuakeBspImportRunner::ImportBsp(BspFile.FilePath, FolderPath, WorldChunkMode, WorldChunkSize, ImportScale, &WorldMeshes);
}
