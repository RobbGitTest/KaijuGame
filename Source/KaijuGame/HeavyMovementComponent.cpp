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

float UHeavyMovementComponent::GetMaxSpeed() const
{
	return IsHovering() ? MaxHoverSpeed : Super::GetMaxSpeed();
}

float UHeavyMovementComponent::GetMaxAcceleration() const
{
	return IsHovering() ? HoverAcceleration : Super::GetMaxAcceleration();
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
	bHoverGrounded = false;
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
		bHoverGrounded = true;
	}

	if (bDrawHoverDebug)
	{
		const FColor Color = bHoverGrounded ? FColor::Green : FColor::Red;
		DrawDebugLine(GetWorld(), Start, bHit ? Hit.Location : End, Color, false, -1.f, 0, 2.f);
		if (bHit)
		{
			DrawDebugSphere(GetWorld(), Hit.Location, SphereRadius, 12, Color, false, -1.f);
		}
	}

	return bHoverGrounded;
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
		const bool bGrounded = TraceHoverGround(GroundZ);

		// --- Horizontal: accelerate toward camera-relative input with some inertia ---
		FVector Horizontal(Velocity.X, Velocity.Y, 0.f);
		const FVector InputDir = Acceleration.GetSafeNormal2D();
		const float Control = bGrounded ? 1.f : HoverAirControl;

		FVector DesiredVelocity = FVector::ZeroVector;
		float Rate = HoverBrakingDeceleration;
		if (!InputDir.IsNearlyZero())
		{
			DesiredVelocity = InputDir * MaxHoverSpeed * FMath::Clamp(AnalogInputModifier, 0.f, 1.f);
			Rate = HoverAcceleration;
			if (FVector::DotProduct(Horizontal, InputDir) < 0.f)
			{
				Rate *= DirectionChangeAccelerationScale;
			}
		}
		Horizontal = FMath::VInterpConstantTo(Horizontal, DesiredVelocity, Step, Rate * Control);

		// --- Vertical: spring/damper toward hover height, gravity when nothing is below ---
		float VerticalSpeed = Velocity.Z;
		if (bGrounded)
		{
			const float TargetZ = GroundZ + HoverHeight + HalfHeight;
			const float Error = TargetZ - UpdatedComponent->GetComponentLocation().Z;
			// Implicit damping keeps the spring stable at low frame rates.
			VerticalSpeed = (VerticalSpeed + Error * HoverStiffness * Step) / (1.f + HoverDamping * Step);
			VerticalSpeed = FMath::Clamp(VerticalSpeed, -MaxHoverVerticalSpeed, MaxHoverVerticalSpeed);
		}
		else
		{
			VerticalSpeed = FMath::Max(VerticalSpeed + GetGravityZ() * Step, -GetPhysicsVolume()->TerminalVelocity);
		}

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
