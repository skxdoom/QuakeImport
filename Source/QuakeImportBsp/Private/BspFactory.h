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

    virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName Name, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
};
