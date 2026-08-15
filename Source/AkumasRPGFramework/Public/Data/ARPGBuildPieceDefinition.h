#pragma once
#include "CoreMinimal.h"
#include "Data/ARPGDefinitionBase.h"
#include "Data/ARPGRecipeDefinition.h"
#include "ARPGBuildPieceDefinition.generated.h"

class AARPGBuildPieceActor;
class UARPGCraftingStationDefinition;
class UStaticMesh;
class UMaterialInterface;
class USoundBase;

UENUM(BlueprintType)
enum class EARPGBuildPieceKind : uint8
{
    Foundation,
    Wall,
    WindowWall,
    Window,
    Doorway,
    Door,
    Floor,
    Ceiling,
    Roof,
    Stair,
    Pillar,
    Storage,
    Production,
    Decoration,
    Custom
};

/** Physical hinge edge for native Door pieces, expressed in the framework's logical wall axes. */
UENUM(BlueprintType)
enum class EARPGBuildDoorHingeSide : uint8
{
    /** Left edge when looking at the Door from the wall's logical +Y/front side. Maps to local +X. */
    Left UMETA(DisplayName="Left"),
    /** Right edge when looking at the Door from the wall's logical +Y/front side. Maps to local -X. */
    Right UMETA(DisplayName="Right")
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGBuildSnapPoint
{
    GENERATED_BODY()

    /** Designer label only. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Snap") FName SnapName = NAME_None;
    /** Desired incoming actor transform relative to this built piece. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Snap") FTransform IncomingPlacementTransform;
    /** Empty means every incoming piece kind is accepted. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Snap") TArray<EARPGBuildPieceKind> AcceptedIncomingKinds;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Snap") bool bEnabled = true;
};

UCLASS(BlueprintType)
class AKUMASRPGFRAMEWORK_API UARPGBuildPieceDefinition : public UARPGDefinitionBase
{
    GENERATED_BODY()
public:
    /** Optional custom actor class. Leave empty to use the framework's native actor for this Piece Kind. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Actor") TSoftClassPtr<AARPGBuildPieceActor> ActorClass;
    /** Main visible world mesh. This is enough to use the native build actor without making a Blueprint actor. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Actor") TSoftObjectPtr<UStaticMesh> BuildMesh;
    /** Optional lighter proxy mesh for placement. Falls back to Build Mesh. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Actor") TSoftObjectPtr<UStaticMesh> PreviewMesh;
    /**
     * Data-driven transform applied to the visible Build/Preview mesh inside the framework actor.
     * Use this to adapt third-party modular meshes without reimporting them. For wall-family pieces,
     * actor-local X is the logical wall run and actor-local +Y is the logical front/exterior side;
     * rotate/flip the visible mesh here so its intended art-facing matches that convention.
     * Identity preserves all pre-v2.15.3 content exactly.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Actor", meta=(DisplayName="Mesh Relative Transform"))
    FTransform MeshRelativeTransform = FTransform::Identity;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Identity") EARPGBuildPieceKind PieceKind = EARPGBuildPieceKind::Foundation;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Identity") FName BuildCategory = TEXT("Structure");
    /** Existing tag-based classification remains available for project-specific Wood/Stone/Metal tiers. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Identity") FGameplayTag PieceType;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Identity") FGameplayTag MaterialTier;

    /**
     * Native Door hinge edge. Left/Right are evaluated after Mesh Relative Transform, so imported meshes
     * can be re-oriented without changing the gameplay hinge convention. Left is logical +X and Right
     * is logical -X when viewed from the wall's logical +Y/front side.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Door", meta=(EditCondition="PieceKind==EARPGBuildPieceKind::Door", EditConditionHides))
    EARPGBuildDoorHingeSide DoorHingeSide = EARPGBuildDoorHingeSide::Left;

    /** Resource requirements are read directly from the player's replicated Inventory. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Cost") TArray<FARPGItemAmount> BuildCost;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Cost") bool bRefundOnDemolish = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Cost", meta=(ClampMin="0.0", ClampMax="1.0")) float DemolishRefundFraction = 1.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Placement") bool bSnapPlacement = true;
    /** Walls, ceilings, roofs and doors normally enable this; foundations normally leave it disabled. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Placement") bool bRequiresSnapTarget = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Placement") bool bRequiresSupport = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Placement") bool bAllowGroundPlacement = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Placement") bool bRequireMostlyFlatGround = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Placement", meta=(ClampMin="0.0", ClampMax="89.0", Units="deg")) float MaxGroundSlopeDegrees = 35.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Placement") FVector PlacementBounds = FVector(190.f, 190.f, 20.f);
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Placement") FVector PlacementOffset = FVector::ZeroVector;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Placement", meta=(ClampMin="0.0")) float SupportTraceDepth = 200.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Placement", meta=(ClampMin="0.0")) float PlacementCollisionClearance = 2.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Placement", meta=(ClampMin="1.0")) float SnapSize = 400.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Placement", meta=(ClampMin="1.0")) float StandardWallHeight = 300.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Placement", meta=(ClampMin="1.0")) float SnapSearchRadius = 500.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Placement", meta=(ClampMin="0.0")) float SnapCaptureDistance = 140.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Placement", meta=(ClampMin="1.0", ClampMax="180.0", Units="deg")) float RotationStepDegrees = 90.f;
    /** Native standard sockets cover foundations, walls, doorways, floors/ceilings, roofs and flat-support Stairs. Add custom points for unusual kits. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Placement") bool bGenerateStandardSnapPoints = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Placement") TArray<FARPGBuildSnapPoint> CustomSnapPoints;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Construction", meta=(ClampMin="1.0")) float MaxHealth = 500.f;
    /** 0 = instant placement. Non-zero pieces visibly materialize over this duration. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Construction", meta=(ClampMin="0.0", Units="s")) float ConstructionSeconds = 0.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Construction") bool bCollisionDuringConstruction = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Construction", meta=(ClampMin="0.01", ClampMax="1.0")) float ConstructionStartScaleZ = 0.05f;
    /** Materials may optionally expose either ConstructionProgress or BuildProgress scalar parameters for a custom dissolve/reveal. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Construction") FName ConstructionProgressMaterialParameter = TEXT("ConstructionProgress");
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Construction") TSoftObjectPtr<USoundBase> ConstructionStartSound;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Construction") TSoftObjectPtr<USoundBase> ConstructionCompleteSound;

    /** Storage Piece Kind only. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Utility", meta=(ClampMin="1", EditCondition="PieceKind==EARPGBuildPieceKind::Storage", EditConditionHides)) int32 StorageSlots = 48;
    /** Production Piece Kind only. Lets a furnace/workbench be entirely Data Asset driven with no actor Blueprint required. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Utility", meta=(EditCondition="PieceKind==EARPGBuildPieceKind::Production", EditConditionHides)) TObjectPtr<UARPGCraftingStationDefinition> StationDefinition = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Faction") FName RequiredBuilderFactionId = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Faction") int32 MinimumBuilderReputation = TNumericLimits<int32>::Lowest();
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Faction") bool bInheritBuilderFaction = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Faction") bool bSameFactionCanUse = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Faction") bool bAlliesCanUse = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Faction") bool bNeutralCanUse = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Faction") bool bHostilesCanUse = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Faction") bool bFactionMembersCanModify = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Faction") bool bHostilesCanDamage = true;
};
