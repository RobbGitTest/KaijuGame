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

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHeavyMovementEvent);

/**
 * Grounded hover movement for Heavy (GDD 8.2).
 *
 * Runs as MOVE_Custom / EHeavyMovementMode::Hover. Each step:
 *  - a downward sphere sweep finds the ground below the capsule,
 *  - a spring/damper eases the capsule toward HoverHeight above that ground,
 *  - horizontal velocity accelerates toward the camera-relative input direction with a little inertia,
 *  - if no ground is within reach, gravity takes over until the sweep finds ground again.
 *
 * On top of that:
 *  - Sprint: while held, top speed / acceleration switch to the sprint values. Free, no resource.
 *  - Liftoff: a free jet jump from the ground, launching up and along the input direction.
 *  - Thrust: holding the jump button in the air spends fuel to climb gently once Heavy stops rising.
 *    Fuel recharges on its own while grounded.
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

	// --- Sprint ----------------------------------------------------------------------------

	/** Top speed while the sprint button is held (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Sprint", meta = (ClampMin = "0", Units = "cm/s"))
	float SprintSpeed = 2000.f;

	/** Acceleration while sprinting (cm/s^2). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Sprint", meta = (ClampMin = "0"))
	float SprintAcceleration = 3000.f;

	// --- Liftoff (jump) --------------------------------------------------------------------

	/** Upward speed given by the liftoff (cm/s). Peak height is roughly v^2 / (2 * gravity). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Liftoff", meta = (ClampMin = "0", Units = "cm/s"))
	float LiftoffVerticalSpeed = 1400.f;

	/** Horizontal speed added along the movement input at liftoff (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Liftoff", meta = (ClampMin = "0", Units = "cm/s"))
	float LiftoffHorizontalBoost = 600.f;

	/** Cap on horizontal speed right after liftoff (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Liftoff", meta = (ClampMin = "0", Units = "cm/s"))
	float LiftoffMaxHorizontalSpeed = 2400.f;

	/**
	 * After a liftoff, the hover spring re-engages once Heavy is falling and the ground is within
	 * this distance below the capsule (cm). Larger = softer, earlier catch.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Liftoff", meta = (ClampMin = "0", Units = "cm"))
	float LandingCatchHeight = 150.f;

	// --- Air thrust / fuel -----------------------------------------------------------------

	/** Vertical speed the thrusters settle at while holding jump in the air (cm/s). Positive = gentle climb. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Thrust", meta = (Units = "cm/s"))
	float ThrustClimbSpeed = 250.f;

	/** How hard the thrusters push vertical speed toward ThrustClimbSpeed (cm/s^2). Higher = catches falls faster. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Thrust", meta = (ClampMin = "0"))
	float ThrustAcceleration = 3000.f;

	/** Horizontal air control while thrusting (replaces HoverAirControl). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Thrust", meta = (ClampMin = "0", ClampMax = "1"))
	float ThrustAirControl = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Thrust", meta = (ClampMin = "1"))
	float MaxFuel = 100.f;

	/** Fuel spent per second of thrust. MaxFuel / this = seconds of hover from a full tank. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Thrust", meta = (ClampMin = "0"))
	float FuelDrainPerSecond = 25.f;

	/** Fuel regained per second while grounded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Thrust", meta = (ClampMin = "0"))
	float FuelRechargePerSecond = 40.f;

	/** Seconds on the ground before the fast ground recharge kicks in (the air rate applies until then). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Thrust", meta = (ClampMin = "0", Units = "s"))
	float FuelRechargeDelay = 0.4f;

	/** Fuel regained per second while airborne and not thrusting (falling, rising after liftoff, dashing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Thrust", meta = (ClampMin = "0"))
	float FuelAirRechargePerSecond = 8.f;

	/** Seconds after thrust stops before the air recharge starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Thrust", meta = (ClampMin = "0", Units = "s"))
	float FuelAirRechargeDelay = 0.5f;

	// --- Dash ------------------------------------------------------------------------------

	/** Horizontal speed during the dash (cm/s). Distance is about DashSpeed * DashDuration plus the bleed-off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Dash", meta = (ClampMin = "0", Units = "cm/s"))
	float DashSpeed = 3500.f;

	/** How long the dash lunge lasts (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Dash", meta = (ClampMin = "0.01", Units = "s"))
	float DashDuration = 0.2f;

	/** After the lunge, speed above the normal/sprint top speed bleeds off at this rate (cm/s^2). Higher = shorter slide. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Dash", meta = (ClampMin = "0"))
	float DashExitDeceleration = 7000.f;

	/** Time from the start of one dash until the next is allowed (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Dash", meta = (ClampMin = "0", Units = "s"))
	float DashCooldown = 0.6f;

	/** Allow dashing while airborne. During an air dash gravity is paused (altitude is held). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Dash")
	bool bAllowAirDash = true;

	/** Air dashes allowed before touching the ground again. 0 = unlimited (cooldown only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Dash", meta = (ClampMin = "0", EditCondition = "bAllowAirDash"))
	int32 MaxAirDashes = 1;

	/**
	 * Invulnerability window from the start of the dash (s). Not used by anything yet:
	 * the future health component should check IsDashInvulnerable().
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Dash", meta = (ClampMin = "0", Units = "s"))
	float DashInvulnerabilityTime = 0.15f;

	/** Liftoff (jump) is blocked for this long after a dash starts (s). Should be at least DashDuration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Dash", meta = (ClampMin = "0", Units = "s"))
	float DashJumpLockout = 0.35f;

	/** Turn-rate multiplier while the dash rotation lock is active (0 = no turning, 1 = normal turning). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Dash", meta = (ClampMin = "0", ClampMax = "1"))
	float DashRotationRateScale = 0.1f;

	/** How long the turn-rate limit lasts after the dash itself ends (s), so Heavy doesn't snap around on exit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Dash", meta = (ClampMin = "0", Units = "s"))
	float DashRotationLockExtraTime = 0.1f;

	/** True while liftoff is blocked by a recent dash. */
	UFUNCTION(BlueprintPure, Category = "Heavy|Dash")
	bool IsDashJumpLocked() const { return GetTimeSinceDashStart() < DashJumpLockout; }

	/** True while turning is limited by a recent dash. */
	UFUNCTION(BlueprintPure, Category = "Heavy|Dash")
	bool IsDashRotationLocked() const { return GetTimeSinceDashStart() < DashDuration + DashRotationLockExtraTime; }

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

	// --- Sprint / liftoff / thrust API -----------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Heavy|Sprint")
	void SetSprinting(bool bNewSprinting) { bWantsToSprint = bNewSprinting; }

	UFUNCTION(BlueprintPure, Category = "Heavy|Sprint")
	bool IsSprinting() const { return bWantsToSprint; }

	/** Launches Heavy if it is hover-grounded. Returns true if the liftoff happened. */
	UFUNCTION(BlueprintCallable, Category = "Heavy|Liftoff")
	bool TryLiftoff();

	/** Whether the jump button is held (thrust engages in the air while this is true and fuel remains). */
	UFUNCTION(BlueprintCallable, Category = "Heavy|Thrust")
	void SetThrustHeld(bool bHeld) { bThrustHeld = bHeld; }

	UFUNCTION(BlueprintPure, Category = "Heavy|Thrust")
	bool IsThrusting() const { return bIsThrusting; }

	/** True from liftoff until the hover spring catches Heavy again. */
	UFUNCTION(BlueprintPure, Category = "Heavy|Liftoff")
	bool IsInLiftoff() const { return bInLiftoff; }

	UFUNCTION(BlueprintPure, Category = "Heavy|Thrust")
	float GetFuel() const { return Fuel; }

	UFUNCTION(BlueprintPure, Category = "Heavy|Thrust")
	float GetFuelPercent() const { return MaxFuel > 0.f ? Fuel / MaxFuel : 0.f; }

	/** True while fuel is going up (ground or air rate). */
	UFUNCTION(BlueprintPure, Category = "Heavy|Thrust")
	bool IsFuelRecharging() const { return bFuelRecharging; }

	/**
	 * Dash along WorldDirection (flattened). Zero direction = backward relative to FallbackForward.
	 * Returns false if on cooldown, already dashing, or air dashing isn't allowed right now.
	 */
	UFUNCTION(BlueprintCallable, Category = "Heavy|Dash")
	bool TryDash(FVector WorldDirection, FVector FallbackForward);

	UFUNCTION(BlueprintPure, Category = "Heavy|Dash")
	bool IsDashing() const { return DashTimeRemaining > 0.f; }

	/** World-space direction of the current / most recent dash (flat, normalised). */
	UFUNCTION(BlueprintPure, Category = "Heavy|Dash")
	FVector GetDashDirection() const { return DashDirection; }

	UFUNCTION(BlueprintPure, Category = "Heavy|Dash")
	bool CanDash() const;

	/** Seconds until the dash is ready (0 = ready). */
	UFUNCTION(BlueprintPure, Category = "Heavy|Dash")
	float GetDashCooldownRemaining() const;

	/** True during the first DashInvulnerabilityTime of a dash. For the future health/damage system. */
	UFUNCTION(BlueprintPure, Category = "Heavy|Dash")
	bool IsDashInvulnerable() const;

	UPROPERTY(BlueprintAssignable, Category = "Heavy|Events")
	FHeavyMovementEvent OnDashStarted;

	UPROPERTY(BlueprintAssignable, Category = "Heavy|Events")
	FHeavyMovementEvent OnDashEnded;

	/** Hooks for VFX / audio / animation. */
	UPROPERTY(BlueprintAssignable, Category = "Heavy|Events")
	FHeavyMovementEvent OnLiftoff;

	UPROPERTY(BlueprintAssignable, Category = "Heavy|Events")
	FHeavyMovementEvent OnHoverLanded;

	UPROPERTY(BlueprintAssignable, Category = "Heavy|Events")
	FHeavyMovementEvent OnThrustStarted;

	UPROPERTY(BlueprintAssignable, Category = "Heavy|Events")
	FHeavyMovementEvent OnThrustStopped;

	// --- UCharacterMovementComponent -------------------------------------------------------
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;
	virtual float GetMaxBrakingDeceleration() const override;
	virtual FRotator GetDeltaRotation(float DeltaTime) const override;
	virtual bool ShouldRemainVertical() const override;

protected:
	virtual void BeginPlay() override;
	virtual void SetDefaultMovementMode() override;
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;

	void PhysHover(float DeltaTime, int32 Iterations);

	/** Sweeps down for ground; fills HoverGroundDistance and returns true if hoverable ground is in reach. */
	bool TraceHoverGround(float& OutGroundZ);

	float GetCurrentMaxHoverSpeed() const { return bWantsToSprint ? SprintSpeed : MaxHoverSpeed; }
	float GetCurrentHoverAcceleration() const { return bWantsToSprint ? SprintAcceleration : HoverAcceleration; }

	void SetThrusting(bool bNewThrusting);
	void UpdateFuel(float DeltaTime);

	/** Seconds since the last dash started (huge if never dashed). */
	float GetTimeSinceDashStart() const;

private:
	bool bHoverGrounded = false;
	float HoverGroundDistance = -1.f;

	bool bWantsToSprint = false;
	bool bThrustHeld = false;
	bool bIsThrusting = false;
	bool bInLiftoff = false;

	float Fuel = 100.f;
	float GroundedTime = 0.f;
	float TimeSinceThrust = 0.f;
	bool bFuelRecharging = false;

	FVector DashDirection = FVector::ZeroVector;
	float DashTimeRemaining = 0.f;
	bool bDashRecovering = false;
	double LastDashStartTime = -1.0e9;
	int32 AirDashesUsed = 0;
};
