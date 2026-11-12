// KaijuGame

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HeavyMovementComponent.generated.h"

/** Custom movement sub-modes used with MOVE_Custom. */
UENUM(BlueprintType)
enum class EHeavyMovementMode : uint8
{
	None  UMETA(Hidden),
	Hover UMETA(DisplayName = "Hover"),
};

/**
 * Grounded hover movement for Heavy (GDD 8.2).
 *
 * Runs as MOVE_Custom / EHeavyMovementMode::Hover. Each step:
 *  - a downward sphere sweep finds the ground below the capsule,
 *  - a spring/damper eases the capsule toward HoverHeight above that ground,
 *  - horizontal velocity accelerates toward the camera-relative input direction with a little inertia,
 *  - if no ground is within reach, gravity takes over until the sweep finds ground again.
 */
UCLASS(ClassGroup = (Movement), meta = (BlueprintSpawnableComponent))
class KAIJUGAME_API UHeavyMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UHeavyMovementComponent();

	// --- Horizontal movement ---------------------------------------------------------------

	/** Top hover speed at full stick deflection (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Hover|Horizontal", meta = (ClampMin = "0", Units = "cm/s"))
	float MaxHoverSpeed = 1200.f;

	/** How fast velocity moves toward the input direction (cm/s^2). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Hover|Horizontal", meta = (ClampMin = "0"))
	float HoverAcceleration = 2400.f;

	/** How fast Heavy slows down with no input (cm/s^2). Lower = more drift. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Hover|Horizontal", meta = (ClampMin = "0"))
	float HoverBrakingDeceleration = 1800.f;

	/**
	 * Acceleration multiplier when the input points against the current velocity (sharp turns / reversals).
	 * Below 1 = more Armored Core-style slide on direction changes, above 1 = snappier turns.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Hover|Horizontal", meta = (ClampMin = "0"))
	float DirectionChangeAccelerationScale = 1.f;

	/** Fraction of horizontal acceleration available while airborne (no ground within reach). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Hover|Horizontal", meta = (ClampMin = "0", ClampMax = "1"))
	float HoverAirControl = 0.4f;

	// --- Vertical (hover spring) -----------------------------------------------------------

	/** Gap between the bottom of the capsule and the ground (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Hover|Spring", meta = (ClampMin = "0", Units = "cm"))
	float HoverHeight = 120.f;

	/** Spring stiffness pulling Heavy to hover height (1/s^2). Higher = tighter terrain following. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Hover|Spring", meta = (ClampMin = "0"))
	float HoverStiffness = 120.f;

	/** Spring damping (1/s). About 2*sqrt(Stiffness) is critically damped; lower = bouncier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Hover|Spring", meta = (ClampMin = "0"))
	float HoverDamping = 18.f;

	/** Cap on vertical speed produced by the hover spring (cm/s), so steps and ledges don't launch Heavy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Hover|Spring", meta = (ClampMin = "0", Units = "cm/s"))
	float MaxHoverVerticalSpeed = 1500.f;

	/** How far below hover height the ground sweep still counts as "grounded" (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Hover|Ground Trace", meta = (ClampMin = "0", Units = "cm"))
	float HoverTraceExtraDistance = 200.f;

	/** Radius of the downward ground sweep (cm). Larger smooths over small bumps and gaps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Hover|Ground Trace", meta = (ClampMin = "1", Units = "cm"))
	float HoverTraceRadius = 40.f;

	/** Steepest surface Heavy hovers over, in degrees. Steeper hits are ignored (Heavy falls / slides). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Hover|Ground Trace", meta = (ClampMin = "0", ClampMax = "89"))
	float HoverMaxSlopeAngle = 60.f;

	/** Draw the ground sweep in the world. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Hover|Debug")
	bool bDrawHoverDebug = false;

	// --- State (read-only) -----------------------------------------------------------------

	/** True when the ground sweep found hoverable ground this step. */
	UFUNCTION(BlueprintPure, Category = "Heavy|Hover")
	bool IsHoverGrounded() const { return bHoverGrounded; }

	/** Distance from capsule bottom to the ground, or -1 when no ground is in reach. */
	UFUNCTION(BlueprintPure, Category = "Heavy|Hover")
	float GetHoverGroundDistance() const { return HoverGroundDistance; }

	UFUNCTION(BlueprintPure, Category = "Heavy|Hover")
	bool IsHovering() const { return MovementMode == MOVE_Custom && CustomMovementMode == static_cast<uint8>(EHeavyMovementMode::Hover); }

	UFUNCTION(BlueprintCallable, Category = "Heavy|Hover")
	void StartHovering();

	// --- UCharacterMovementComponent -------------------------------------------------------
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;
	virtual float GetMaxBrakingDeceleration() const override;
	virtual bool ShouldRemainVertical() const override;

protected:
	virtual void SetDefaultMovementMode() override;
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;

	void PhysHover(float DeltaTime, int32 Iterations);

	/** Sweeps down for ground; fills bHoverGrounded / HoverGroundDistance and returns the ground height. */
	bool TraceHoverGround(float& OutGroundZ);

private:
	bool bHoverGrounded = false;
	float HoverGroundDistance = -1.f;
};
