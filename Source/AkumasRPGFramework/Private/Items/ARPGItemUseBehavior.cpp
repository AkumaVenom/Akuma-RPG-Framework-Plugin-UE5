#include "Items/ARPGItemUseBehavior.h"

bool UARPGItemUseBehavior::CanUseItem_Implementation(const FARPGItemUseContext& Context, FText& OutFailureReason)
{
    OutFailureReason = FText::GetEmpty();
    return true;
}

bool UARPGItemUseBehavior::ExecuteItemUse_Implementation(const FARPGItemUseContext& Context)
{
    // A behavior class is intentionally not considered successful until its Blueprint/native subclass
    // actually applies something. This prevents an accidentally assigned empty behavior from consuming items.
    return false;
}

void UARPGItemUseBehavior::PlayItemUsePresentation_Implementation(const FARPGItemUseContext& Context)
{
}
