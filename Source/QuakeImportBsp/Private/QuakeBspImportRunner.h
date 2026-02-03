#pragma once

#include "CoreMinimal.h"
#include "BspFactory.h"

namespace QuakeBspImportRunner
{
    bool ImportBsp(const FString& BspFilePath, const FString& TargetFolderLongPackagePath, EWorldChunkMode WorldChunkMode, int32 WorldChunkSize, float ImportScale, TArray<FString>* OutWorldMeshObjectPaths);
}
