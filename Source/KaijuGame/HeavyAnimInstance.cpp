// KaijuGame

#include "HeavyAnimInstance.h"

#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "HeavyMovementComponent.h"

void UHeavyAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	OwnerCharacter = Cast<ACharacter>(TryGetPawnOwner());
	HeavyMovement = OwnerCharacter ? Cast<UHeavyMovementComponent>(OwnerCharacter->GetCharacterMovement()) : nullptr;

	if (HeavyMovement)
	{
		HeavyMovement->OnLiftoff.AddUniqueDynamic(this, &UHeavyAnimInstance::HandleLiftoff);
		HeavyMovement->OnDashStarted.AddUniqueDynamic(this, &UHeavyAnimInstance::HandleDashStarted);
		HeavyMovement->OnHoverLanded.AddUniqueDynamic(this, &UHeavyAnimInstance::HandleHoverLanded);
	}
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

	if (!bIsGrounded)
	{
		LastAirVerticalSpeed = VerticalSpeed;
	}

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

	// Air axis: thrusting or rising -> +1, falling -> -1, grounded (or an air dash lunge) -> 0.
	float TargetAir = 0.f;
	if (!bIsGrounded && !(bIsDashing && bAirDashUsesGroundPose))
	{
		TargetAir = (bIsThrusting || VerticalSpeed > RisingSpeedThreshold) ? 1.f : -1.f;
	}

	AirAlpha = AirInterpSpeed > 0.f
		? FMath::FInterpTo(AirAlpha, TargetAir, DeltaSeconds, AirInterpSpeed)
		: TargetAir;

	UpdateSteerLean(DeltaSeconds);
}

void UHeavyAnimInstance::UpdateSteerLean(float DeltaSeconds)
{
	// Steering direction = movement input (camera-relative acceleration), compared with where Heavy faces.
	// Heavy turns toward input at a limited rate, so during a turn the chest and head lead the body.
	const FVector InputAccel = HeavyMovement->GetCurrentAcceleration();
	const float InputStrength = FMath::Clamp(InputAccel.Size2D() / FMath::Max(1.f, HeavyMovement->GetMaxAcceleration()), 0.f, 1.f);

	float TargetSteer = 0.f;
	if (InputStrength > KINDA_SMALL_NUMBER)
	{
		const FVector Local = OwnerCharacter->GetActorQuat().UnrotateVector(InputAccel.GetSafeNormal2D());
		TargetSteer = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));
	}
	SteerAngle = TargetSteer;

	FRotator TargetSpine = FRotator::ZeroRotator;
	FRotator TargetHead = FRotator::ZeroRotator;

	if (bEnableSteerLean)
	{
		// Beyond 90 degrees (steering backwards) twisting further looks broken; cap the look-ahead there.
		const float Steer = FMath::Clamp(TargetSteer, -90.f, 90.f);
		const float SpeedFactor = FMath::Clamp(LocomotionAlpha / FMath::Max(MaxLocomotionAlpha, 0.01f), 0.f, 1.f);

		TargetSpine.Yaw = FMath::Clamp(Steer * SpineYawShare, -MaxSpineYaw, MaxSpineYaw);
		TargetHead.Yaw = FMath::Clamp(Steer * HeadYawShare, -MaxHeadYaw, MaxHeadYaw);

		// Forward lean (negative pitch = nose down): grows with speed, more while thrusting with input.
		float ForwardLean = RunForwardLean * SpeedFactor;
		if (bIsThrusting)
		{
			ForwardLean += ThrustForwardLean * InputStrength;
		}
		TargetSpine.Pitch = -ForwardLean * (bInvertSteerPitch ? -1.f : 1.f);
		TargetHead.Pitch = -TargetSpine.Pitch * HeadLevelling;

		// Bank into the turn: steering right drops the right shoulder.
		TargetSpine.Roll = (Steer / 90.f) * MaxSpineBank * SpeedFactor * (bInvertSteerBank ? -1.f : 1.f);
	}

	if (SteerLeanInterpSpeed > 0.f)
	{
		SpineLeanActor = FMath::RInterpTo(SpineLeanActor, TargetSpine, DeltaSeconds, SteerLeanInterpSpeed);
		HeadLeanActor = FMath::RInterpTo(HeadLeanActor, TargetHead, DeltaSeconds, SteerLeanInterpSpeed);
	}
	else
	{
		SpineLeanActor = TargetSpine;
		HeadLeanActor = TargetHead;
	}

	// The lean is authored in actor space (X forward, Y right, Z up). The mesh is rotated relative to the actor
	// (e.g. yaw -90 for a +Y-facing mesh), so convert into the mesh's component space for the Modify Bone nodes.
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	const FQuat MeshRel = Mesh ? Mesh->GetRelativeRotation().Quaternion() : FQuat::Identity;
	const FQuat MeshRelInv = MeshRel.Inverse();
	SpineBoneRotation = (MeshRelInv * SpineLeanActor.Quaternion() * MeshRel).Rotator();
	HeadBoneRotation = (MeshRelInv * HeadLeanActor.Quaternion() * MeshRel).Rotator();
}

void UHeavyAnimInstance::PlayOneShot(UAnimSequenceBase* Anim, float PlayRate)
{
	if (Anim)
	{
		PlaySlotAnimationAsDynamicMontage(Anim, OneShotSlotName, OneShotBlendIn, OneShotBlendOut, PlayRate);
	}
}

void UHeavyAnimInstance::HandleLiftoff()
{
	PlayOneShot(JumpAnim, JumpAnimPlayRate);
}

void UHeavyAnimInstance::HandleDashStarted()
{
	if (!OwnerCharacter || !HeavyMovement)
	{
		return;
	}
	const FVector Facing = OwnerCharacter->GetActorForwardVector().GetSafeNormal2D();
	if (FVector::DotProduct(Facing, HeavyMovement->GetDashDirection()) >= ForwardDashMinDot)
	{
		PlayOneShot(ForwardDashAnim, ForwardDashAnimPlayRate);
	}
	else
	{
		// Side/back dash: no clip yet (comes with lock-on). Cut any jump/landing one-shot so the run pose shows.
		StopSlotAnimation(OneShotBlendOut, OneShotSlotName);
	}
}

void UHeavyAnimInstance::HandleHoverLanded()
{
	if (-LastAirVerticalSpeed >= MinLandingImpactSpeed)
	{
		PlayOneShot(LandingAnim, LandingAnimPlayRate);
	}
	LastAirVerticalSpeed = 0.f;
}
