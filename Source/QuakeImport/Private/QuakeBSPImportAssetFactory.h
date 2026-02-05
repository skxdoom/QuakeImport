#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "QuakeBSPImportAssetFactory.generated.h"

UCLASS()
class UQuakeBSPImportAssetFactory : public UFactory
{
    GENERATED_BODY()

public:
    UQuakeBSPImportAssetFactory();
    virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
};
