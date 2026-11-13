// KaijuGame

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HeavyHUDWidget.generated.h"

class UProgressBar;
class UHeavyMovementComponent;

/**
 * Base class for Heavy's HUD (WBP_HeavyHUD). Drives the fuel bar from the owning pawn's
 * UHeavyMovementComponent every frame. Layout and styling live in the Widget Blueprint.
 */
UCLASS(Abstract)
class KAIJUGAME_API UHeavyHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Progress bar named "FuelBar" in the Widget Blueprint. */
	UPROPERTY(BlueprintReadOnly, Category = "Heavy|HUD", meta = (BindWidget))
	TObjectPtr<UProgressBar> FuelBar;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|HUD")
	FLinearColor FuelColor = FLinearColor(0.1f, 0.6f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|HUD")
	FLinearColor ThrustingColor = FLinearColor(0.4f, 0.9f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|HUD")
	FLinearColor LowFuelColor = FLinearColor(1.f, 0.25f, 0.1f);

	/** Below this fraction the bar turns LowFuelColor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|HUD", meta = (ClampMin = "0", ClampMax = "1"))
	float LowFuelThreshold = 0.25f;

	UFUNCTION(BlueprintPure, Category = "Heavy|HUD")
	UHeavyMovementComponent* GetHeavyMovement() const;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
};
