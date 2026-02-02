// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "RHIDefinitions.h"
#include "Factories/Factory.h"
#include "BspFactory.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogQuakeImporter, Log, All);

UENUM(BlueprintType)
enum class EWorldChunkMode : uint8
{
    Leaves UMETA(DisplayName="Leaves"),
    Grid UMETA(DisplayName="Grid")
};

UCLASS(MinimalAPI)
class UBspFactory : public UFactory
{
    GENERATED_BODY()

public:
    UBspFactory(const FObjectInitializer& ObjectInitializer);

    UPROPERTY(EditAnywhere, Category="Quake Import")
    EWorldChunkMode WorldChunkMode = EWorldChunkMode::Leaves;

    // Used only when WorldChunkMode is Grid.
    UPROPERTY(EditAnywhere, Category="Quake Import", meta=(ClampMin="1"))
    int32 WorldChunkSize = 2048;

    // Optional scale applied to all imported geometry (Quake units to UE units).
    UPROPERTY(EditAnywhere, Category="Quake Import", meta=(ClampMin="0.0001"))
    float ImportScale = 1.0f;

    virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName Name, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
};
