#pragma once

#include "CoreMinimal.h"
#include "BspFactory.h"
#include "QuakeBspImportAsset.generated.h"

UCLASS(BlueprintType)
class QUAKEIMPORT_API UQuakeBspImportAsset : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category="Quake Import")
    FFilePath BspFile;

    UPROPERTY(EditAnywhere, Category="Quake Import")
    EWorldChunkMode WorldChunkMode = EWorldChunkMode::Grid;

    UPROPERTY(EditAnywhere, Category="Quake Import", meta=(ClampMin="1", EditCondition="WorldChunkMode==EWorldChunkMode::Grid", EditConditionHides))
    int32 WorldChunkSize = 512;

    UPROPERTY(EditAnywhere, Category="Quake Import", meta=(ClampMin="0.0001"))
    float ImportScale = 2.5f;

    UFUNCTION(CallInEditor, Category="Quake Import")
    void Import();
};
