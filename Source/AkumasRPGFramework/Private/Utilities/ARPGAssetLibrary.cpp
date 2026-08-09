#include "Utilities/ARPGAssetLibrary.h"
#include "Data/ARPGDefinitionBase.h"
#include "Engine/AssetManager.h"

UARPGDefinitionBase* UARPGAssetLibrary::ResolveDefinitionById(TSubclassOf<UARPGDefinitionBase> DefinitionClass, FName DefinitionId)
{
    if (!DefinitionClass || DefinitionId.IsNone()) return nullptr;
    const FPrimaryAssetId AssetId(DefinitionClass->GetFName(), DefinitionId);
    UAssetManager& Manager = UAssetManager::Get();
    UObject* Object = Manager.GetPrimaryAssetObject(AssetId);
    if (!Object)
    {
        const FSoftObjectPath Path = Manager.GetPrimaryAssetPath(AssetId);
        if (Path.IsValid()) Object = Path.TryLoad();
    }
    return Cast<UARPGDefinitionBase>(Object);
}
