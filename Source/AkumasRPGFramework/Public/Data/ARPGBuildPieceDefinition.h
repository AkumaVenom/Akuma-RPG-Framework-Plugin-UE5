#pragma once
#include "CoreMinimal.h"
#include "Data/ARPGDefinitionBase.h"
#include "Data/ARPGRecipeDefinition.h"
#include "ARPGBuildPieceDefinition.generated.h"

class AARPGBuildPieceActor;
class UARPGCraftingStationDefinition;
class UStaticMesh;
class USkeletalMesh;
class UMaterialInterface;
class UAnimSequenceBase;
class USoundBase;
class UNiagaraSystem;
class UParticleSystem;

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
    Custom,
    /** Interactive buildable lighting/decor. Appended to preserve every existing enum value. */
    Light
};


/** Native placement contract for buildable light fixtures. */
UENUM(BlueprintType)
enum class EARPGBuildLightPlacementMode : uint8
{
    /** Upright fixture seated on terrain or on the top surface of a built Foundation/Floor. */
    HorizontalSurface UMETA(DisplayName="Ground / Foundation / Floor"),
    /** Fixture seated on either face of a completed Wall/WindowWall/Doorway. Actor local +Y points away from the host surface. */
    WallSurface UMETA(DisplayName="Built Wall Surface")
};

/** Runtime light component used by the native buildable-light actor. */
UENUM(BlueprintType)
enum class EARPGBuildLightType : uint8
{
    Point UMETA(DisplayName="Point Light"),
    Spot UMETA(DisplayName="Spot Light")
};

/** Niagara/Cascade selection policy for a buildable light fixture. */
UENUM(BlueprintType)
enum class EARPGBuildLightFXMode : uint8
{
    None UMETA(DisplayName="None"),
    NiagaraPreferredWithCascadeFallback UMETA(DisplayName="Niagara Preferred / Cascade Fallback"),
    NiagaraOnly UMETA(DisplayName="Niagara Only"),
    CascadeOnly UMETA(DisplayName="Cascade Only"),
    NiagaraAndCascade UMETA(DisplayName="Niagara + Cascade")
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
    /** Main visible world Static Mesh. Existing definitions continue to use this path unchanged. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Actor") TSoftObjectPtr<UStaticMesh> BuildMesh;
    /**
     * Optional main visible world Skeletal Mesh. When a valid skeletal asset is assigned it takes
     * presentation/bounds precedence over Build Mesh, allowing animated build pieces without a custom
     * actor Blueprint while preserving every existing Static Mesh definition unchanged.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Actor", meta=(DisplayName="Build Skeletal Mesh")) TSoftObjectPtr<USkeletalMesh> BuildSkeletalMesh;
    /** Optional lighter Static Mesh proxy for placement. If neither preview field is assigned, the active Build Mesh is used. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Actor") TSoftObjectPtr<UStaticMesh> PreviewMesh;
    /** Optional Skeletal Mesh placement proxy. Takes preview precedence when valid. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Actor", meta=(DisplayName="Preview Skeletal Mesh")) TSoftObjectPtr<USkeletalMesh> PreviewSkeletalMesh;
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

    /**
     * Optional host-local correction for the standard WindowWall -> Window insert socket. The native
     * default centers the incoming Window's transformed visible bounds inside the WindowWall's
     * transformed visible bounds on X/Y/Z. Use this only when a particular WindowWall kit authors its
     * opening away from that visual center (for example a deliberately high sill). Because the offset
     * belongs to the WindowWall host, one Window asset can remain reusable across differently authored
     * wall openings without abusing the incoming piece's generic Placement Offset.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Window", meta=(DisplayName="Window Insert Offset", EditCondition="PieceKind==EARPGBuildPieceKind::WindowWall", EditConditionHides))
    FVector WindowInsertOffset = FVector::ZeroVector;

    /** Skeletal Window only. Animation from the authored closed pose to the open pose. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Window|Interaction", meta=(EditCondition="PieceKind==EARPGBuildPieceKind::Window", EditConditionHides))
    TSoftObjectPtr<UAnimSequenceBase> WindowOpenAnimation;
    /** Optional explicit open-to-closed animation. Leave empty to play Window Open Animation in reverse. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Window|Interaction", meta=(EditCondition="PieceKind==EARPGBuildPieceKind::Window", EditConditionHides))
    TSoftObjectPtr<UAnimSequenceBase> WindowCloseAnimation;
    /** Playback-rate magnitude for native Window open/close animation. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Window|Interaction", meta=(ClampMin="0.01", EditCondition="PieceKind==EARPGBuildPieceKind::Window", EditConditionHides))
    float WindowAnimationPlayRate = 1.f;
    /** Optional one-shot sounds played when an authoritative Window begins opening/closing. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Window|Interaction", meta=(EditCondition="PieceKind==EARPGBuildPieceKind::Window", EditConditionHides))
    TSoftObjectPtr<USoundBase> WindowOpenSound;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Window|Interaction", meta=(EditCondition="PieceKind==EARPGBuildPieceKind::Window", EditConditionHides))
    TSoftObjectPtr<USoundBase> WindowCloseSound;
    /** When true, the native gameplay blocker is removed while open and throughout a closing transition, then restored once fully closed. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Window|Interaction", meta=(EditCondition="PieceKind==EARPGBuildPieceKind::Window", EditConditionHides))
    bool bDisableWindowCollisionWhenOpen = true;


    /**
     * Buildable-light placement mode. Horizontal fixtures stay world-upright and may seat on terrain,
     * Foundations and Floors. Wall fixtures attach to the actually aimed face of a completed
     * Wall/WindowWall/Doorway and use actor local +Y as the outward surface normal.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Light|Placement", meta=(EditCondition="PieceKind==EARPGBuildPieceKind::Light", EditConditionHides))
    EARPGBuildLightPlacementMode LightPlacementMode = EARPGBuildLightPlacementMode::HorizontalSurface;
    /** Small outward lift from the resolved support plane after visible-bounds seating. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Light|Placement", meta=(ClampMin="0.0", Units="cm", EditCondition="PieceKind==EARPGBuildPieceKind::Light", EditConditionHides))
    float LightSurfaceOffset = 0.f;
    /** Minimum centre-to-centre spacing from another completed buildable Light. Prevents exact fixture stacking while remaining non-blocking to structural pieces. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Light|Placement", meta=(ClampMin="0.0", Units="cm", EditCondition="PieceKind==EARPGBuildPieceKind::Light", EditConditionHides))
    float LightMinimumSpacing = 20.f;
    /** View-corridor radius used by the existing Interact Built Structure button. The fixture itself remains non-blocking to placement and Pawn movement. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Light|Interaction", meta=(ClampMin="10.0", Units="cm", EditCondition="PieceKind==EARPGBuildPieceKind::Light", EditConditionHides))
    float LightInteractionRadius = 75.f;
    /** Initial replicated state for freshly placed fixtures. No fuel item is consumed by native toggling. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Light|Interaction", meta=(EditCondition="PieceKind==EARPGBuildPieceKind::Light", EditConditionHides))
    bool bLightStartsOn = false;
    /** Smooth intensity/emissive fade duration for On/Off transitions. Zero applies immediately. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Light|Interaction", meta=(ClampMin="0.0", Units="s", EditCondition="PieceKind==EARPGBuildPieceKind::Light", EditConditionHides))
    float LightFadeSeconds = 0.35f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Light|Interaction", meta=(EditCondition="PieceKind==EARPGBuildPieceKind::Light", EditConditionHides))
    TSoftObjectPtr<USoundBase> LightOnSound;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Light|Interaction", meta=(EditCondition="PieceKind==EARPGBuildPieceKind::Light", EditConditionHides))
    TSoftObjectPtr<USoundBase> LightOffSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Light|Light Source", meta=(EditCondition="PieceKind==EARPGBuildPieceKind::Light", EditConditionHides))
    EARPGBuildLightType BuildLightType = EARPGBuildLightType::Point;
    /** Transform of the native Point/Spot light relative to the logical build actor. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Light|Light Source", meta=(EditCondition="PieceKind==EARPGBuildPieceKind::Light", EditConditionHides))
    FTransform LightComponentRelativeTransform = FTransform::Identity;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Light|Light Source", meta=(ClampMin="0.0", EditCondition="PieceKind==EARPGBuildPieceKind::Light", EditConditionHides))
    float LightIntensity = 3000.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Light|Light Source", meta=(ClampMin="1.0", Units="cm", EditCondition="PieceKind==EARPGBuildPieceKind::Light", EditConditionHides))
    float LightAttenuationRadius = 900.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Light|Light Source", meta=(EditCondition="PieceKind==EARPGBuildPieceKind::Light", EditConditionHides))
    FLinearColor LightColor = FLinearColor(1.f, 0.48f, 0.16f, 1.f);
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Light|Light Source", meta=(EditCondition="PieceKind==EARPGBuildPieceKind::Light", EditConditionHides))
    bool bLightUseTemperature = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Light|Light Source", meta=(ClampMin="1000.0", ClampMax="15000.0", Units="K", EditCondition="PieceKind==EARPGBuildPieceKind::Light", EditConditionHides))
    float LightTemperature = 2500.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Light|Light Source", meta=(EditCondition="PieceKind==EARPGBuildPieceKind::Light", EditConditionHides))
    bool bLightCastShadows = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Light|Light Source", meta=(ClampMin="0.0", Units="cm", EditCondition="PieceKind==EARPGBuildPieceKind::Light", EditConditionHides))
    float LightSourceRadius = 5.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Light|Light Source", meta=(ClampMin="0.0", Units="cm", EditCondition="PieceKind==EARPGBuildPieceKind::Light", EditConditionHides))
    float LightSoftSourceRadius = 20.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Light|Light Source", meta=(ClampMin="0.0", ClampMax="89.0", Units="deg", EditCondition="PieceKind==EARPGBuildPieceKind::Light", EditConditionHides))
    float LightSpotInnerConeAngle = 20.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Light|Light Source", meta=(ClampMin="1.0", ClampMax="89.0", Units="deg", EditCondition="PieceKind==EARPGBuildPieceKind::Light", EditConditionHides))
    float LightSpotOuterConeAngle = 45.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Light|FX", meta=(EditCondition="PieceKind==EARPGBuildPieceKind::Light", EditConditionHides))
    EARPGBuildLightFXMode LightFXMode = EARPGBuildLightFXMode::NiagaraPreferredWithCascadeFallback;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Light|FX", meta=(EditCondition="PieceKind==EARPGBuildPieceKind::Light", EditConditionHides))
    TSoftObjectPtr<UNiagaraSystem> LightNiagaraSystem;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Light|FX", meta=(EditCondition="PieceKind==EARPGBuildPieceKind::Light", EditConditionHides))
    TSoftObjectPtr<UParticleSystem> LightCascadeSystem;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Light|FX", meta=(EditCondition="PieceKind==EARPGBuildPieceKind::Light", EditConditionHides))
    FTransform LightEffectRelativeTransform = FTransform::Identity;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Light|FX", meta=(EditCondition="PieceKind==EARPGBuildPieceKind::Light", EditConditionHides))
    bool bResetLightEffectsOnActivation = true;
    /** Optional scalar applied to all build-visual materials during fades (for example an emissive multiplier). NAME_None disables this path. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Light|FX", meta=(EditCondition="PieceKind==EARPGBuildPieceKind::Light", EditConditionHides))
    FName LightEmissiveMaterialParameter = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Light|FX", meta=(EditCondition="PieceKind==EARPGBuildPieceKind::Light", EditConditionHides))
    float LightEmissiveOffValue = 0.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building|Light|FX", meta=(EditCondition="PieceKind==EARPGBuildPieceKind::Light", EditConditionHides))
    float LightEmissiveOnValue = 1.f;

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
