// KaijuGame

#include "HeavyAnimInstance.h"

#include "GameFramework/Character.h"
#include "HeavyMovementComponent.h"

void UHeavyAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	OwnerCharacter = Cast<ACharacter>(TryGetPawnOwner());
	HeavyMovement = OwnerCharacter ? Cast<UHeavyMovementComponent>(OwnerCharacter->GetCharacterMovement()) : nullptr;
}

void UHeavyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!OwnerCharacter || !HeavyMovement)
	{
		// The editor preview has no pawn; also covers late possession.
		NativeInitializeAnimation();
		if (!OwnerCharacter || !HeavyMovement)
		{
			return;
		}
	}

	const FVector Velocity = OwnerCharacter->GetVelocity();
	GroundSpeed = Velocity.Size2D();
	VerticalSpeed = Velocity.Z;
	bIsMoving = GroundSpeed > IdleSpeedThreshold;
	bIsSprinting = HeavyMovement->IsSprinting();
	bIsDashing = HeavyMovement->IsDashing();
	bIsGrounded = HeavyMovement->IsHoverGrounded();
	bIsThrusting = HeavyMovement->IsThrusting();

	// Map speed onto the blend space: 0..MaxHoverSpeed -> 0..1 (idle->walk), MaxHoverSpeed..SprintSpeed -> 1..2 (walk->run).
	const float WalkSpeed = FMath::Max(1.f, HeavyMovement->MaxHoverSpeed);
	const float RunSpeed = FMath::Max(WalkSpeed + 1.f, HeavyMovement->SprintSpeed);
	float TargetAlpha = 0.f;
	if (bIsMoving)
	{
		TargetAlpha = GroundSpeed <= WalkSpeed
			? GroundSpeed / WalkSpeed
			: 1.f + (GroundSpeed - WalkSpeed) / (RunSpeed - WalkSpeed);
	}
	TargetAlpha = FMath::Clamp(TargetAlpha, 0.f, MaxLocomotionAlpha);

	LocomotionAlpha = LocomotionInterpSpeed > 0.f
		? FMath::FInterpTo(LocomotionAlpha, TargetAlpha, DeltaSeconds, LocomotionInterpSpeed)
		: TargetAlpha;
}
