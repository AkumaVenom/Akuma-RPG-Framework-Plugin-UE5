#include "Utilities/ARPGAssetLibrary.h"
#include "Data/ARPGDefinitionBase.h"
#include "Engine/AssetManager.h"
#include "UObject/UObjectIterator.h"

UARPGDefinitionBase* UARPGAssetLibrary::ResolveDefinitionById(TSubclassOf<UARPGDefinitionBase> DefinitionClass, FName DefinitionId)
{
    if (!DefinitionClass || DefinitionId.IsNone()) return nullptr;

    // First accept an already-loaded matching Data Asset. This makes editor-authored definitions referenced by
    // Blueprints/levels usable even when the project has not manually added every concrete type to Asset Manager.
    for (TObjectIterator<UARPGDefinitionBase> It; It; ++It)
    {
        UARPGDefinitionBase* Candidate = *It;
        if (!IsValid(Candidate) || Candidate->HasAnyFlags(RF_ClassDefaultObject) || !Candidate->IsA(DefinitionClass)) continue;
        const FName CandidateId = Candidate->DefinitionId.IsNone() ? Candidate->GetFName() : Candidate->DefinitionId;
        if (CandidateId == DefinitionId) return Candidate;
    }

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
