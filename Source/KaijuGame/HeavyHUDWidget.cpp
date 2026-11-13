// KaijuGame

#include "HeavyHUDWidget.h"

#include "Components/ProgressBar.h"
#include "GameFramework/Pawn.h"
#include "HeavyMovementComponent.h"

UHeavyMovementComponent* UHeavyHUDWidget::GetHeavyMovement() const
{
	const APawn* Pawn = GetOwningPlayerPawn();
	return Pawn ? Pawn->FindComponentByClass<UHeavyMovementComponent>() : nullptr;
}

void UHeavyHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const UHeavyMovementComponent* Movement = GetHeavyMovement();
	if (!FuelBar || !Movement)
	{
		return;
	}

	const float Percent = Movement->GetFuelPercent();
	FuelBar->SetPercent(Percent);

	FLinearColor Color = FuelColor;
	if (Percent <= LowFuelThreshold)
	{
		Color = LowFuelColor;
	}
	else if (Movement->IsThrusting())
	{
		Color = ThrustingColor;
	}
	FuelBar->SetFillColorAndOpacity(Color);
}
