// KaijuGame

#include "HeavyMovementComponent.h"

#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PhysicsVolume.h"

UHeavyMovementComponent::UHeavyMovementComponent()
{
	// Face the movement input (free-movement scheme, GDD 8.1). Rotation is smoothed by RotationRate.
	bOrientRotationToMovement = true;
	bUseControllerDesiredRotation = false;
	RotationRate = FRotator(0.f, 360.f, 0.f);

	// Used if Heavy ever leaves hover (e.g. falling far off the map).
	MaxWalkSpeed = MaxHoverSpeed;
	AirControl = HoverAirControl;
	GravityScale = 1.5f;
}

void UHeavyMovementComponent::StartHovering()
{
	SetMovementMode(MOVE_Custom, static_cast<uint8>(EHeavyMovementMode::Hover));
}

void UHeavyMovementComponent::SetDefaultMovementMode()
{
	if (!CharacterOwner)
	{
		Super::SetDefaultMovementMode();
		return;
	}
	StartHovering();
}

void UHeavyMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	Fuel = MaxFuel;
}

float UHeavyMovementComponent::GetMaxSpeed() const
{
	return IsHovering() ? GetCurrentMaxHoverSpeed() : Super::GetMaxSpeed();
}

float UHeavyMovementComponent::GetMaxAcceleration() const
{
	return IsHovering() ? GetCurrentHoverAcceleration() : Super::GetMaxAcceleration();
}

bool UHeavyMovementComponent::TryLiftoff()
{
	if (!IsHovering() || !bHoverGrounded || bInLiftoff || IsDashJumpLocked())
	{
		return false;
	}

	// Launch along the current movement input (camera-relative, set by AddMovementInput).
	FVector Horizontal(Velocity.X, Velocity.Y, 0.f);
	const FVector InputDir = Acceleration.GetSafeNormal2D();
	if (!InputDir.IsNearlyZero())
	{
		Horizontal += InputDir * LiftoffHorizontalBoost * FMath::Clamp(AnalogInputModifier, 0.f, 1.f);
		Horizontal = Horizontal.GetClampedToMaxSize(LiftoffMaxHorizontalSpeed);
	}

	Velocity = FVector(Horizontal.X, Horizontal.Y, LiftoffVerticalSpeed);
	bInLiftoff = true;
	bHoverGrounded = false;
	GroundedTime = 0.f;
	OnLiftoff.Broadcast();
	return true;
}

void UHeavyMovementComponent::SetThrusting(bool bNewThrusting)
{
	if (bIsThrusting == bNewThrusting)
	{
		return;
	}
	bIsThrusting = bNewThrusting;
	if (bIsThrusting)
	{
		OnThrustStarted.Broadcast();
	}
	else
	{
		OnThrustStopped.Broadcast();
	}
}

void UHeavyMovementComponent::UpdateFuel(float DeltaTime)
{
	GroundedTime = bHoverGrounded ? GroundedTime + DeltaTime : 0.f;
	bFuelRecharging = false;

	if (bIsThrusting)
	{
		TimeSinceThrust = 0.f;
		Fuel = FMath::Max(0.f, Fuel - FuelDrainPerSecond * DeltaTime);
		return;
	}
	TimeSinceThrust += DeltaTime;

	// Fast recharge once settled on the ground; slow recharge any other time Heavy isn't thrusting.
	float Rate = 0.f;
	if (bHoverGrounded && GroundedTime >= FuelRechargeDelay)
	{
		Rate = FuelRechargePerSecond;
	}
	else if (TimeSinceThrust >= FuelAirRechargeDelay)
	{
		Rate = FuelAirRechargePerSecond;
	}

	if (Rate > 0.f && Fuel < MaxFuel)
	{
		Fuel = FMath::Min(MaxFuel, Fuel + Rate * DeltaTime);
		bFuelRecharging = true;
	}
}

float UHeavyMovementComponent::GetTimeSinceDashStart() const
{
	const UWorld* World = GetWorld();
	return World ? static_cast<float>(World->GetTimeSeconds() - LastDashStartTime) : UE_BIG_NUMBER;
}

FRotator UHeavyMovementComponent::GetDeltaRotation(float DeltaTime) const
{
	// During (and just after) a dash Heavy commits to its heading: turning toward new input is heavily slowed.
	const FRotator Delta = Super::GetDeltaRotation(DeltaTime);
	return IsDashRotationLocked() ? Delta * DashRotationRateScale : Delta;
}

bool UHeavyMovementComponent::CanDash() const
{
	if (!IsHovering() || IsDashing() || GetDashCooldownRemaining() > 0.f)
	{
		return false;
	}
	if (!bHoverGrounded)
	{
		if (!bAllowAirDash)
		{
			return false;
		}
		if (MaxAirDashes > 0 && AirDashesUsed >= MaxAirDashes)
		{
			return false;
		}
	}
	return true;
}

float UHeavyMovementComponent::GetDashCooldownRemaining() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.f;
	}
	return FMath::Max(0.f, static_cast<float>(LastDashStartTime + DashCooldown - World->GetTimeSeconds()));
}

bool UHeavyMovementComponent::IsDashInvulnerable() const
{
	const UWorld* World = GetWorld();
	return World && (World->GetTimeSeconds() - LastDashStartTime) <= DashInvulnerabilityTime;
}

bool UHeavyMovementComponent::TryDash(FVector WorldDirection, FVector FallbackForward)
{
	if (!CanDash())
	{
		return false;
	}

	FVector Dir = WorldDirection.GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		// No input: back-step away from where the camera looks.
		Dir = -FallbackForward.GetSafeNormal2D();
	}
	if (Dir.IsNearlyZero())
	{
		return false;
	}

	if (!bHoverGrounded)
	{
		++AirDashesUsed;
	}

	DashDirection = Dir;
	DashTimeRemaining = DashDuration;
	bDashRecovering = false;
	LastDashStartTime = GetWorld()->GetTimeSeconds();
	SetThrusting(false);
	OnDashStarted.Broadcast();
	return true;
}

float UHeavyMovementComponent::GetMaxBrakingDeceleration() const
{
	return IsHovering() ? HoverBrakingDeceleration : Super::GetMaxBrakingDeceleration();
}

bool UHeavyMovementComponent::ShouldRemainVertical() const
{
	// Keep the capsule upright while hovering; visual banking is done on the mesh, not the capsule.
	return IsHovering() || Super::ShouldRemainVertical();
}

void UHeavyMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	if (CustomMovementMode == static_cast<uint8>(EHeavyMovementMode::Hover))
	{
		PhysHover(DeltaTime, Iterations);
		return;
	}
	Super::PhysCustom(DeltaTime, Iterations);
}

bool UHeavyMovementComponent::TraceHoverGround(float& OutGroundZ)
{
	bool bGroundInReach = false;
	HoverGroundDistance = -1.f;

	const UCapsuleComponent* Capsule = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr;
	if (!Capsule)
	{
		return false;
	}

	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector Center = UpdatedComponent->GetComponentLocation();
	// Start the sphere inside the capsule so it never begins penetrating the ground.
	const float SphereRadius = FMath::Min(HoverTraceRadius, Capsule->GetScaledCapsuleRadius());
	const FVector Start = Center - FVector(0.f, 0.f, HalfHeight - SphereRadius);
	const float Reach = HoverHeight + HoverTraceExtraDistance;
	const FVector End = Start - FVector(0.f, 0.f, Reach);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(HeavyHoverTrace), false, CharacterOwner);
	FCollisionResponseParams ResponseParams;
	InitCollisionParams(Params, ResponseParams);

	FHitResult Hit;
	const bool bHit = GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, UpdatedComponent->GetCollisionObjectType(),
		FCollisionShape::MakeSphere(SphereRadius), Params, ResponseParams);

	const float MinNormalZ = FMath::Cos(FMath::DegreesToRadians(HoverMaxSlopeAngle));
	if (bHit && !Hit.bStartPenetrating && Hit.ImpactNormal.Z >= MinNormalZ)
	{
		OutGroundZ = Hit.ImpactPoint.Z;
		HoverGroundDistance = FMath::Max(0.f, (Center.Z - HalfHeight) - OutGroundZ);
		bGroundInReach = true;
	}

	if (bDrawHoverDebug)
	{
		const FColor Color = bGroundInReach ? (bInLiftoff ? FColor::Yellow : FColor::Green) : FColor::Red;
		DrawDebugLine(GetWorld(), Start, bHit ? Hit.Location : End, Color, false, -1.f, 0, 2.f);
		if (bHit)
		{
			DrawDebugSphere(GetWorld(), Hit.Location, SphereRadius, 12, Color, false, -1.f);
		}
	}

	return bGroundInReach;
}

void UHeavyMovementComponent::PhysHover(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME || !CharacterOwner || !UpdatedComponent)
	{
		return;
	}

	const float HalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	float RemainingTime = DeltaTime;

	while (RemainingTime >= MIN_TICK_TIME && Iterations < MaxSimulationIterations)
	{
		++Iterations;
		const float Step = GetSimulationTimeStep(RemainingTime, Iterations);
		RemainingTime -= Step;

		float GroundZ = 0.f;
		const bool bGroundInReach = TraceHoverGround(GroundZ);

		// After a liftoff, ignore the spring until Heavy is coming back down close to the ground.
		if (bInLiftoff && bGroundInReach && Velocity.Z <= 0.f && HoverGroundDistance <= LandingCatchHeight)
		{
			bInLiftoff = false;
		}

		const bool bWasGrounded = bHoverGrounded;
		bHoverGrounded = bGroundInReach && !bInLiftoff;
		const bool bGrounded = bHoverGrounded;
		if (bGrounded && !bWasGrounded)
		{
			OnHoverLanded.Broadcast();
		}
		if (bGrounded)
		{
			AirDashesUsed = 0;
		}

		// Dash timer.
		const bool bDashingThisStep = DashTimeRemaining > 0.f;
		if (bDashingThisStep)
		{
			DashTimeRemaining -= Step;
			if (DashTimeRemaining <= 0.f)
			{
				DashTimeRemaining = 0.f;
				bDashRecovering = true;
				OnDashEnded.Broadcast();
			}
		}

		// Thrust: holding jump in the air with fuel left, once Heavy is no longer rising faster than the climb speed.
		// Suspended during a dash (the dash holds altitude on its own).
		const bool bCanThrust = !bGrounded && bThrustHeld && Fuel > 0.f && !bDashingThisStep;
		SetThrusting(bCanThrust && (bIsThrusting || Velocity.Z <= ThrustClimbSpeed));

		// --- Horizontal: accelerate toward camera-relative input with some inertia ---
		FVector Horizontal(Velocity.X, Velocity.Y, 0.f);

		// After a dash, bleed only the speed above the current top speed so it flows into normal movement.
		if (bDashRecovering && !bDashingThisStep)
		{
			const float MaxSpeedNow = GetCurrentMaxHoverSpeed();
			const float Speed = Horizontal.Size();
			if (Speed > MaxSpeedNow)
			{
				Horizontal = Horizontal.GetSafeNormal() * FMath::FInterpConstantTo(Speed, MaxSpeedNow, Step, DashExitDeceleration);
			}
			else
			{
				bDashRecovering = false;
			}
		}
		const FVector InputDir = Acceleration.GetSafeNormal2D();
		const float Control = bGrounded ? 1.f : (bIsThrusting ? ThrustAirControl : HoverAirControl);

		FVector DesiredVelocity = FVector::ZeroVector;
		float Rate = HoverBrakingDeceleration;
		if (!InputDir.IsNearlyZero())
		{
			DesiredVelocity = InputDir * GetCurrentMaxHoverSpeed() * FMath::Clamp(AnalogInputModifier, 0.f, 1.f);
			Rate = GetCurrentHoverAcceleration();
			if (FVector::DotProduct(Horizontal, InputDir) < 0.f)
			{
				Rate *= DirectionChangeAccelerationScale;
			}
		}
		if (bDashingThisStep)
		{
			// Locked lunge: fixed speed along the dash direction, input ignored.
			Horizontal = DashDirection * DashSpeed;
		}
		else
		{
			Horizontal = FMath::VInterpConstantTo(Horizontal, DesiredVelocity, Step, Rate * Control);
		}

		// --- Vertical: spring/damper toward hover height, gravity when nothing is below ---
		float VerticalSpeed = Velocity.Z;
		if (bDashingThisStep && !bGrounded)
		{
			// Air dash holds altitude.
			VerticalSpeed = 0.f;
		}
		else if (bGrounded)
		{
			const float TargetZ = GroundZ + HoverHeight + HalfHeight;
			const float Error = TargetZ - UpdatedComponent->GetComponentLocation().Z;
			// Implicit damping keeps the spring stable at low frame rates.
			VerticalSpeed = (VerticalSpeed + Error * HoverStiffness * Step) / (1.f + HoverDamping * Step);
			VerticalSpeed = FMath::Clamp(VerticalSpeed, -MaxHoverVerticalSpeed, MaxHoverVerticalSpeed);
		}
		else if (bIsThrusting)
		{
			// Thrusters ease vertical speed toward a gentle climb (also arresting a fall).
			VerticalSpeed = FMath::FInterpConstantTo(VerticalSpeed, ThrustClimbSpeed, Step, ThrustAcceleration);
		}
		else
		{
			VerticalSpeed = FMath::Max(VerticalSpeed + GetGravityZ() * Step, -GetPhysicsVolume()->TerminalVelocity);
		}

		UpdateFuel(Step);

		Velocity = FVector(Horizontal.X, Horizontal.Y, VerticalSpeed);

		// --- Move and slide along anything we bump into ---
		const FVector OldLocation = UpdatedComponent->GetComponentLocation();
		const FVector Delta = Velocity * Step;
		FHitResult Hit(1.f);
		SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);

		if (Hit.IsValidBlockingHit())
		{
			HandleImpact(Hit, Step, Delta);
			SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
		}

		// Derive velocity from where we actually ended up, so walls bleed off speed.
		if (!bJustTeleported && Step > UE_KINDA_SMALL_NUMBER)
		{
			Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / Step;
		}
	}
}
