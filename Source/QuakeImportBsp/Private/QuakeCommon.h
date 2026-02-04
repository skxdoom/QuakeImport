#pragma once

#include "CoreMinimal.h"

class UTexture2D;
class UPackage;
class UMaterial;
class UMaterialInstanceConstant;
class UMaterialInterface;

namespace QuakeCommon
{
    // Represent a single RGB 8bit sample
    struct QColor
    {
        uint8 r;
        uint8 g;
        uint8 b;
    };

    // Load Quake color palette from file in our plugin content
    bool LoadPalette(TArray<QColor>& outPalette);

    // Create a UTexture2D in the given package then save
    UTexture2D* CreateUTexture2D(const FString& name, int width, int height, const TArray<uint8>& data, UPackage& texturePackage, const TArray<QColor>& pal, bool savePackage = true);

	// Create or update a UTexture2D. If bOverwrite is false and the texture exists, it will be reused as-is.
	UTexture2D* CreateOrUpdateUTexture2D(const FString& name, int width, int height, const TArray<uint8>& data, UPackage& texturePackage, const TArray<QColor>& pal, bool bOverwrite, bool savePackage = true);

    // Create matching material for texture
    void CreateUMaterial(const FString& textureName, UPackage& materialPackage, UTexture2D& initialTexture);

    // Create (or reuse) a master material that exposes a single albedo texture parameter.
    UMaterial* GetOrCreateMasterMaterial(const FString& materialName, UPackage& materialPackage);

    // Create (or reuse) a master translucent material that exposes the same color texture parameter and has constant 0.5 opacity.
    UMaterial* GetOrCreateTransparentMasterMaterial(const FString& materialName, UPackage& materialPackage);

    // Create (or reuse) a master unlit material that drives Emissive from the same color texture parameter.
    UMaterial* GetOrCreateSkyUnlitMasterMaterial(const FString& materialName, UPackage& materialPackage);

    // Create (or reuse) a material instance that binds the master material's albedo parameter.
    UMaterialInstanceConstant* GetOrCreateMaterialInstance(const FString& instanceName, UPackage& materialPackage, UMaterial& parentMaterial, UTexture2D& albedoTexture);

	// Same as above, but supports overwriting an existing instance.
	UMaterialInstanceConstant* GetOrCreateMaterialInstance(const FString& instanceName, UPackage& materialPackage, UMaterial& parentMaterial, UTexture2D& albedoTexture, bool bOverwrite);

    // Same as above, but allows a UMaterialInterface parent (material or material instance).
    UMaterialInstanceConstant* GetOrCreateMaterialInstance(const FString& instanceName, UPackage& materialPackage, UMaterialInterface& parentMaterial, UTexture2D& albedoTexture);

	// Same as above, but supports overwriting an existing instance.
	UMaterialInstanceConstant* GetOrCreateMaterialInstance(const FString& instanceName, UPackage& materialPackage, UMaterialInterface& parentMaterial, UTexture2D& albedoTexture, bool bOverwrite);

    // Utilities

    template<class T>
    int ReadData(const uint8*& data, const int position, T& out)
    {
        out = *reinterpret_cast<const T*>(data + position);
        return sizeof(T);
    }

    template<typename T>
    T* CheckIfAssetExist(const FString& name, const UPackage& package)
    {
        const FString fullname = package.GetName() + TEXT(".") + name;
        return LoadObject<T>(nullptr, *fullname, nullptr, LOAD_Quiet | LOAD_NoWarn);
    }

    template<typename T>
    T* CheckIfAssetExist(const FString& name, const UPackage* package)
    {
        return package ? CheckIfAssetExist<T>(name, *package) : nullptr;
    }

    void SaveAsset(UObject& object, UPackage& package);

    void SavePackage(UPackage& package);

} // namespace QuakeCommon