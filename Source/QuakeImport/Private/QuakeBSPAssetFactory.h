// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "RHIDefinitions.h"
#include "Factories/Factory.h"
#include "QuakeBSPAssetFactory.generated.h"

UCLASS(MinimalAPI)
class UQuakeBSPAssetFactory : public UFactory
{
    GENERATED_BODY()

public:
    UQuakeBSPAssetFactory(const FObjectInitializer& ObjectInitializer);

    virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName Name, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
};
