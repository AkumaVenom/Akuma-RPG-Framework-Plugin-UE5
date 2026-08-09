#include "Components/ARPGBossComponent.h"
#include "Components/ARPGStatsComponent.h"
#include "Components/ARPGCombatComponent.h"
#include "Data/ARPGBossDefinition.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UARPGBossComponent::UARPGBossComponent() { SetIsReplicatedByDefault(true); }

void UARPGBossComponent::BeginPlay()
{
    Super::BeginPlay();
    if (GetOwner()) { HomeLocation = GetOwner()->GetActorLocation(); HomeRotation = GetOwner()->GetActorRotation(); }
    if (UARPGStatsComponent* Stats=GetOwner()?GetOwner()->FindComponentByClass<UARPGStatsComponent>():nullptr)
    {
        BaseMaxHealth=Stats->MaxHealth;BaseAttackPower=Stats->AttackPower;
        Stats->OnHealthChanged.AddDynamic(this,&UARPGBossComponent::HandleHealthChanged);
        Stats->OnDeath.AddDynamic(this,&UARPGBossComponent::HandleBossDeath);
    }
}

void UARPGBossComponent::StartEncounter()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || bEncounterActive || bWaitingForWorldRespawn) return;
    bEncounterActive = true; bEnraged = false; Contributions.Reset(); EvaluatePhase(); OnEncounterStarted.Broadcast();
    if (Definition && Definition->EnrageSeconds > 0.f && GetWorld()) GetWorld()->GetTimerManager().SetTimer(EnrageTimer, FTimerDelegate::CreateUObject(this, &UARPGBossComponent::SetEnraged, true), Definition->EnrageSeconds, false);
    if(Definition && Definition->LeashDistance>0.f && GetWorld())GetWorld()->GetTimerManager().SetTimer(LeashTimer,this,&UARPGBossComponent::CheckLeash,1.f,true);
}

void UARPGBossComponent::ResetEncounter()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    bEncounterActive = false; bEnraged = false; CurrentPhase = NAME_None; Contributions.Reset();
    if (GetWorld()) {GetWorld()->GetTimerManager().ClearTimer(EnrageTimer);GetWorld()->GetTimerManager().ClearTimer(LeashTimer);}
    GetOwner()->SetActorLocationAndRotation(HomeLocation, HomeRotation, false, nullptr, ETeleportType::TeleportPhysics);
    if (UARPGStatsComponent* Stats = GetOwner()->FindComponentByClass<UARPGStatsComponent>()) Stats->RestoreAllVitals();
    OnEncounterReset.Broadcast();
}

void UARPGBossComponent::HandleHealthChanged(float NewHealth,float Delta){if(GetOwner()&&GetOwner()->HasAuthority()&&bEncounterActive)EvaluatePhase();}
void UARPGBossComponent::EvaluatePhase()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Definition) return;
    const UARPGStatsComponent* Stats = GetOwner()->FindComponentByClass<UARPGStatsComponent>(); if (!Stats) return;
    const float HP = Stats->GetHealthPercent(); FName Best = NAME_None; float BestThreshold = 2.f;
    for (const FARPGBossPhaseDefinition& Phase : Definition->Phases)
        if (HP <= Phase.StartsAtHealthPercent && Phase.StartsAtHealthPercent < BestThreshold) { Best = Phase.PhaseId; BestThreshold = Phase.StartsAtHealthPercent; }
    if (Best != CurrentPhase) { const FName Old = CurrentPhase; CurrentPhase = Best; OnPhaseChanged.Broadcast(Old, CurrentPhase); }
}
void UARPGBossComponent::SetEnraged(bool bNewEnraged){if(!GetOwner()||!GetOwner()->HasAuthority())return;bEnraged=bNewEnraged;if(bEnraged)OnEnraged.Broadcast();}

bool UARPGBossComponent::ApplyBossDamage(float Amount,AActor* Contributor)
{
    if(!GetOwner()||!GetOwner()->HasAuthority()||Amount<=0.f||bWaitingForWorldRespawn)return false;
    UARPGStatsComponent* Stats=GetOwner()->FindComponentByClass<UARPGStatsComponent>();if(!Stats)return false;
    if(!bEncounterActive)StartEncounter();
    const float Before=Stats->Health;const bool Applied=Stats->ApplyDamage(Amount);const float Actual=FMath::Max(0.f,Before-Stats->Health);if(Applied&&Actual>0.f)RegisterContribution(Contributor,Actual);return Applied;
}
void UARPGBossComponent::RegisterContribution(AActor* Contributor,float Amount){if(GetOwner()&&GetOwner()->HasAuthority()&&Contributor&&Amount>0.f)Contributions.FindOrAdd(TWeakObjectPtr<AActor>(Contributor))+=Amount;}
float UARPGBossComponent::GetContributionPercent(AActor* Contributor)const
{
    if(!Contributor)return 0.f;float Total=0.f;for(const auto&P:Contributions)Total+=FMath::Max(0.f,P.Value);const float* V=Contributions.Find(TWeakObjectPtr<AActor>(Contributor));return V&&Total>0.f?FMath::Clamp(*V/Total,0.f,1.f):0.f;
}
TArray<AActor*> UARPGBossComponent::GetEligibleContributors()const
{
    TArray<AActor*> Out;const float Min=Definition?Definition->MinimumContributionPercent:0.f;for(const auto&P:Contributions)if(P.Key.IsValid()&&GetContributionPercent(P.Key.Get())>=Min)Out.Add(P.Key.Get());return Out;
}
void UARPGBossComponent::ApplyPlayerCountScaling(int32 PlayerCount,float HealthPerExtraPlayer,float DamagePerExtraPlayer)
{
    if(!GetOwner()||!GetOwner()->HasAuthority()||!Definition||!Definition->bScaleWithPlayers)return;UARPGStatsComponent* Stats=GetOwner()->FindComponentByClass<UARPGStatsComponent>();if(!Stats)return;
    const int32 Count=FMath::Clamp(PlayerCount,1,FMath::Max(1,Definition->MaxScalingPlayers));const int32 Extra=Count-1;
    Stats->MaxHealth=FMath::Max(1.f,BaseMaxHealth*(1.f+FMath::Max(0.f,HealthPerExtraPlayer)*Extra));Stats->Health=Stats->MaxHealth;Stats->AttackPower=FMath::Max(0.f,BaseAttackPower*(1.f+FMath::Max(0.f,DamagePerExtraPlayer)*Extra));
}
void UARPGBossComponent::CheckLeash()
{
    if(!GetOwner()||!GetOwner()->HasAuthority()||!bEncounterActive||!Definition||Definition->LeashDistance<=0.f)return;
    if(FVector::DistSquared2D(GetOwner()->GetActorLocation(),HomeLocation)>FMath::Square(Definition->LeashDistance))ResetEncounter();
}
void UARPGBossComponent::HandleBossDeath()
{
    if(!GetOwner()||!GetOwner()->HasAuthority()||bWaitingForWorldRespawn)return;bEncounterActive=false;if(GetWorld()){GetWorld()->GetTimerManager().ClearTimer(EnrageTimer);GetWorld()->GetTimerManager().ClearTimer(LeashTimer);}OnBossDefeated.Broadcast(GetOwner());
    if(Definition&&(Definition->BossType==EARPGBossType::World||Definition->bWorldBoss)&&Definition->MaxWorldRespawnSeconds>0.f&&GetWorld())
    {
        bWaitingForWorldRespawn=true;GetOwner()->SetActorHiddenInGame(true);GetOwner()->SetActorEnableCollision(false);
        const float A=FMath::Max(0.f,FMath::Min(Definition->MinWorldRespawnSeconds,Definition->MaxWorldRespawnSeconds));const float B=FMath::Max(A,FMath::Max(Definition->MinWorldRespawnSeconds,Definition->MaxWorldRespawnSeconds));
        GetWorld()->GetTimerManager().SetTimer(WorldRespawnTimer,this,&UARPGBossComponent::RespawnWorldBoss,FMath::FRandRange(A,B),false);
    }
}
void UARPGBossComponent::RespawnWorldBoss()
{
    if(!GetOwner()||!GetOwner()->HasAuthority())return;bWaitingForWorldRespawn=false;GetOwner()->SetActorLocationAndRotation(HomeLocation,HomeRotation,false,nullptr,ETeleportType::TeleportPhysics);GetOwner()->SetActorHiddenInGame(false);GetOwner()->SetActorEnableCollision(true);if(UARPGCombatComponent* Combat=GetOwner()->FindComponentByClass<UARPGCombatComponent>())Combat->RespawnAtTransform(FTransform(HomeRotation,HomeLocation));else if(UARPGStatsComponent*Stats=GetOwner()->FindComponentByClass<UARPGStatsComponent>())Stats->RestoreAllVitals();Contributions.Reset();CurrentPhase=NAME_None;bEnraged=false;
}
void UARPGBossComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(UARPGBossComponent,bEncounterActive);DOREPLIFETIME(UARPGBossComponent,bEnraged);DOREPLIFETIME(UARPGBossComponent,bWaitingForWorldRespawn);DOREPLIFETIME(UARPGBossComponent,CurrentPhase);
}
