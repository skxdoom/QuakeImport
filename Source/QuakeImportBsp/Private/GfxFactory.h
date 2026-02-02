// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "GfxFactory.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI)
class UGfxFactory : public UFactory
{
    GENERATED_BODY()

public:
    UGfxFactory(const FObjectInitializer& ObjectInitializer);

    virtual UObject* FactoryCreateBinary(UClass* InClass, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn) override;
};
