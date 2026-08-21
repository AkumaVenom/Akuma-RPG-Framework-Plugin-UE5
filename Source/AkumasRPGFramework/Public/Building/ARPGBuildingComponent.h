#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGTypes.h"
#include "ARPGBuildingComponent.generated.h"

class UARPGBuildPieceDefinition;
class AARPGBuildPieceActor;
class AARPGBuildPreviewActor;
class AARPGBuildPathActor;
class UMaterialInterface;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGBuildPlacementResult, EARPGPlacementResult, Result, AARPGBuildPieceActor*, PlacedActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGBuildModeChanged, bool, bBuildModeActive, UARPGBuildPieceDefinition*, SelectedPiece);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGBuildPreviewUpdated, EARPGPlacementResult, Result, FTransform, PreviewTransform);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FARPGSettlementPathSessionChanged, bool, bPathModeActive, bool, bHasConfirmedStartPoint, FVector, LastConfirmedPoint);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGSettlementPathSegmentPlaced, EARPGPlacementResult, Result, AARPGBuildPathActor*, PlacedSegment);

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGBuildingComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGBuildingComponent();

    /** Pieces exposed in the player's ready build catalogue. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Building|Catalogue") TArray<TObjectPtr<UARPGBuildPieceDefinition>> BuildCatalog;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building|Placement", meta=(ClampMin="100.0")) float MaxPlacementDistance = 1200.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building|Placement") TEnumAsByte<ECollisionChannel> PlacementTraceChannel = ECC_Visibility;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building|Placement") TEnumAsByte<ECollisionChannel> PlacementCollisionChannel = ECC_WorldStatic;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building|Placement") bool bConsumeResources = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building|Placement") bool bKeepBuildModeAfterPlacement = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building|Placement") bool bAutoSelectFirstCatalogPiece = false;
    /** Safe multiplayer default: authority accepts only definitions exposed in this character's Build Catalog. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building|Authority", meta=(DisplayName="Allow Unlisted Build Requests")) bool bAllowUnlistedBuildRequests = false;
    /** Prevents snapping new construction onto another player's/faction's structure without modify permission. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building|Authority") bool bRequireSnapTargetModificationAccess = true;

    /** Optional project materials. If unset the preview still works using the piece mesh and material parameters PreviewTint / PlacementValid when available. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building|Preview") TSoftObjectPtr<UMaterialInterface> ValidPreviewMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building|Preview") TSoftObjectPtr<UMaterialInterface> InvalidPreviewMaterial;

    UPROPERTY(BlueprintAssignable) FARPGBuildPlacementResult OnPlacementResult;
    UPROPERTY(BlueprintAssignable) FARPGBuildModeChanged OnBuildModeChanged;
    UPROPERTY(BlueprintAssignable) FARPGBuildPreviewUpdated OnBuildPreviewUpdated;
    /** Fired when Settlement Path chaining begins, receives its first confirmed anchor, advances, or is cancelled. */
    UPROPERTY(BlueprintAssignable, Category="Building|Settlement Path") FARPGSettlementPathSessionChanged OnSettlementPathSessionChanged;
    /** Local placement result for a confirmed Settlement Path segment. */
    UPROPERTY(BlueprintAssignable, Category="Building|Settlement Path") FARPGSettlementPathSegmentPlaced OnSettlementPathSegmentPlaced;

    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Mode") bool BeginBuildMode(UARPGBuildPieceDefinition* Piece);
    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Mode") void EndBuildMode();
    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Mode") bool ToggleBuildMode(UARPGBuildPieceDefinition* OptionalPiece = nullptr);
    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Mode") bool SelectBuildPiece(UARPGBuildPieceDefinition* Piece);
    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Mode") bool SelectNextBuildPiece();
    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Mode") bool SelectPreviousBuildPiece();
    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Mode") void RotatePreview(float Direction = 1.f);
    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Mode") bool ConfirmPreviewPlacement();
    UFUNCTION(BlueprintPure, Category="ARPG|Building|Mode") bool IsBuildModeActive() const { return bBuildModeActive; }
    UFUNCTION(BlueprintPure, Category="ARPG|Building|Mode") UARPGBuildPieceDefinition* GetSelectedBuildPiece() const { return SelectedBuildPiece; }
    UFUNCTION(BlueprintPure, Category="ARPG|Building|Mode") EARPGPlacementResult GetCurrentPreviewResult() const { return CurrentPreviewResult; }
    UFUNCTION(BlueprintPure, Category="ARPG|Building|Mode") FTransform GetCurrentPreviewTransform() const { return CurrentPreviewTransform; }

    /** Settlement Path placement remains in one continuous session until this or End Build Mode is called. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Settlement Path") void CancelSettlementPathPlacement();
    UFUNCTION(BlueprintPure, Category="ARPG|Building|Settlement Path") bool IsSettlementPathPlacementActive() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Building|Settlement Path") bool HasSettlementPathStartPoint() const { return bSettlementPathHasConfirmedStart; }
    UFUNCTION(BlueprintPure, Category="ARPG|Building|Settlement Path") FVector GetSettlementPathLastConfirmedPoint() const { return SettlementPathLastConfirmedPoint; }
    UFUNCTION(BlueprintPure, Category="ARPG|Building|Settlement Path") bool IsSettlementPathRequestPending() const { return bSettlementPathRequestPending; }

    UFUNCTION(BlueprintPure, Category="ARPG|Building") FTransform SnapTransform(const UARPGBuildPieceDefinition* Piece, const FTransform& DesiredTransform) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Building") EARPGPlacementResult EvaluatePlacement(const UARPGBuildPieceDefinition* Piece, const FTransform& DesiredTransform) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Building") bool RequestPlacePiece(UARPGBuildPieceDefinition* Piece, const FTransform& DesiredTransform);
    UFUNCTION(BlueprintPure, Category="ARPG|Building") int32 GetBuildableCount(const UARPGBuildPieceDefinition* Piece) const;

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
protected:
    UFUNCTION(Server, Reliable) void ServerPlacePiece(UARPGBuildPieceDefinition* Piece, FTransform DesiredTransform);
    UFUNCTION(Server, Reliable) void ServerBeginSettlementPath(UARPGBuildPieceDefinition* Piece, FVector DesiredStartPoint);
    UFUNCTION(Server, Reliable) void ServerPlaceSettlementPathPoint(UARPGBuildPieceDefinition* Piece, FVector DesiredEndPoint);
    UFUNCTION(Server, Reliable) void ServerCancelSettlementPath();
    UFUNCTION(Client, Reliable) void ClientSettlementPathAnchorResult(EARPGPlacementResult Result, FVector AuthoritativeStartPoint);
    UFUNCTION(Client, Reliable) void ClientSettlementPathSegmentResult(EARPGPlacementResult Result, FVector AuthoritativeEndPoint, AARPGBuildPathActor* PlacedSegment);
    bool PlacePieceAuthority(UARPGBuildPieceDefinition* Piece, const FTransform& DesiredTransform);
    bool HasBuildResources(const UARPGBuildPieceDefinition* Piece) const;
    bool ConsumeBuildResources(const UARPGBuildPieceDefinition* Piece);
    void RefundBuildResources(const UARPGBuildPieceDefinition* Piece);
private:
    UPROPERTY(Transient) TObjectPtr<UARPGBuildPieceDefinition> SelectedBuildPiece = nullptr;
    UPROPERTY(Transient) TObjectPtr<AARPGBuildPreviewActor> ActivePreviewActor = nullptr;
    bool bBuildModeActive = false;
    float PreviewYawOffset = 0.f;
    EARPGPlacementResult CurrentPreviewResult = EARPGPlacementResult::NoPiece;
    FTransform CurrentPreviewTransform;
    TWeakObjectPtr<AARPGBuildPieceActor> CurrentSnapTarget;

    // Local authoring state. The server keeps a separate authoritative last point so a client cannot
    // inject disconnected segments or advance the chain after a rejected request.
    bool bSettlementPathHasConfirmedStart = false;
    bool bSettlementPathRequestPending = false;
    FVector SettlementPathLastConfirmedPoint = FVector::ZeroVector;
    /** Last locally confirmed segment, used only so the live preview can mirror the next turn tangent. */
    TWeakObjectPtr<AARPGBuildPathActor> SettlementPathLastPlacedSegment;
    bool bAuthoritySettlementPathActive = false;
    FVector AuthoritySettlementPathLastPoint = FVector::ZeroVector;
    TWeakObjectPtr<UARPGBuildPieceDefinition> AuthoritySettlementPathPiece;
    TWeakObjectPtr<AARPGBuildPathActor> AuthoritySettlementPathLastSegment;

    bool IsLocalBuildController() const;
    void EnsurePreviewActor();
    void DestroyPreviewActor();
    void UpdatePlacementPreview();
    FTransform ResolvePlacementTransform(const UARPGBuildPieceDefinition* Piece, const FTransform& DesiredTransform, AARPGBuildPieceActor*& OutSnapTarget) const;
    bool FindBestSnapTransform(const UARPGBuildPieceDefinition* Piece, const FTransform& DesiredTransform, FTransform& OutTransform, AARPGBuildPieceActor*& OutSnapTarget) const;
    EARPGPlacementResult EvaluatePlacementInternal(const UARPGBuildPieceDefinition* Piece, const FTransform& FinalTransform, const AARPGBuildPieceActor* SnapTarget) const;
    bool ResolveSettlementPathSurfacePoint(const UARPGBuildPieceDefinition* Piece, const FVector& DesiredPoint, FVector& OutProjectedPoint) const;
    EARPGPlacementResult EvaluateSettlementPathPoint(const UARPGBuildPieceDefinition* Piece, const FVector& ProjectedPoint, const FVector* PreviousPoint) const;
    EARPGPlacementResult BeginSettlementPathAuthority(UARPGBuildPieceDefinition* Piece, const FVector& DesiredStartPoint, FVector& OutAuthoritativeStartPoint);
    EARPGPlacementResult PlaceSettlementPathPointAuthority(UARPGBuildPieceDefinition* Piece, const FVector& DesiredEndPoint, FVector& OutAuthoritativeEndPoint, AARPGBuildPathActor*& OutPlacedSegment);
    void ResetLocalSettlementPathSession(bool bBroadcast);
    void ResetAuthoritySettlementPathSession();
    void BroadcastSettlementPathSessionState();
    UClass* ResolveNativeBuildActorClass(const UARPGBuildPieceDefinition* Piece) const;
};
