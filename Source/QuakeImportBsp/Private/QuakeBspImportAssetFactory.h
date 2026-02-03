#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "QuakeBspImportAssetFactory.generated.h"

UCLASS()
class UQuakeBspImportAssetFactory : public UFactory
{
    GENERATED_BODY()

public:
    UQuakeBspImportAssetFactory();
    virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
};
