// KaijuGame

#include "HeavyHUDWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "HeavyCharacter.h"
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
	UpdateLockOnMarker();
	if (SpeedText)
	{
		const APawn* Pawn = GetOwningPlayerPawn();
		const float Speed = Pawn ? Pawn->GetVelocity().Size2D() : 0.f; // cm/s
		SpeedText->SetText(FText::FromString(bSpeedInKmh
			? FString::Printf(TEXT("%.0f km/h"), Speed * 0.036f)
			: FString::Printf(TEXT("%.1f m/s"), Speed * 0.01f)));
	}
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

void UHeavyHUDWidget::UpdateLockOnMarker()
{
	if (!LockOnMarker)
	{
		return;
	}
	const AHeavyCharacter* Heavy = Cast<AHeavyCharacter>(GetOwningPlayerPawn());
	APlayerController* PC = GetOwningPlayer();
	FVector2D ScreenPos = FVector2D::ZeroVector;
	const bool bShow = Heavy && PC && Heavy->IsLockedOn()
		&& UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Heavy->GetLockOnTargetLocation(), ScreenPos, false);

	LockOnMarker->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bShow)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(LockOnMarker->Slot))
		{
			CanvasSlot->SetPosition(ScreenPos);
		}
	}
}
