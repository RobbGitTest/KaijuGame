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

	// Air axis: thrusting (or, optionally, rising) -> +1, otherwise in the air -> -1, grounded (or an air dash lunge) -> 0.
	float TargetAir = 0.f;
	if (!bIsGrounded && !(bIsDashing && bAirDashUsesGroundPose))
	{
		const bool bRisingPose = bRisingUsesThrustPose && VerticalSpeed > RisingSpeedThreshold;
		TargetAir = (bIsThrusting || bRisingPose) ? 1.f : -1.f;
	}

	AirAlpha = AirInterpSpeed > 0.f
		? FMath::FInterpTo(AirAlpha, TargetAir, DeltaSeconds, AirInterpSpeed)
		: TargetAir;

	// Lock-on strafe: velocity in Heavy's local frame, normalised like LocomotionAlpha.
	bIsLockedOn = HeavyMovement->IsLockedOn();
	const FVector LocalVel = OwnerCharacter->GetActorQuat().UnrotateVector(FVector(Velocity.X, Velocity.Y, 0.f));
	const float TargetStrafeX = FMath::Clamp(LocalVel.Y / WalkSpeed, -1.f, 1.f);
	float TargetStrafeY = LocalVel.X <= WalkSpeed
		? LocalVel.X / WalkSpeed
		: 1.f + (LocalVel.X - WalkSpeed) / (RunSpeed - WalkSpeed);
	TargetStrafeY = FMath::Clamp(TargetStrafeY, -1.f, FMath::Min(2.f, MaxLocomotionAlpha));
	if (StrafeInterpSpeed > 0.f)
	{
		StrafeX = FMath::FInterpTo(StrafeX, TargetStrafeX, DeltaSeconds, StrafeInterpSpeed);
		StrafeY = FMath::FInterpTo(StrafeY, TargetStrafeY, DeltaSeconds, StrafeInterpSpeed);
	}
	else
	{
		StrafeX = TargetStrafeX;
		StrafeY = TargetStrafeY;
	}

	LockOnAlpha = LockOnBlendSpeed > 0.f
		? FMath::FInterpTo(LockOnAlpha, bIsLockedOn ? 1.f : 0.f, DeltaSeconds, LockOnBlendSpeed)
		: (bIsLockedOn ? 1.f : 0.f);
	StrafeAlpha = LockOnAlpha * (1.f - FMath::Clamp(FMath::Abs(AirAlpha), 0.f, 1.f));

	UpdateSteerLean(DeltaSeconds);
}

void UHeavyAnimInstance::UpdateSteerLean(float DeltaSeconds)
{
	// Steering direction = movement input (camera-relative acceleration), compared with where Heavy faces.
	// Heavy turns toward input at a limited rate, so during a turn the chest and head lead the body.
	const FVector InputAccel = HeavyMovement->GetCurrentAcceleration();
	const float InputStrength = FMath::Clamp(InputAccel.Size2D() / FMath::Max(1.f, HeavyMovement->GetMaxAcceleration()), 0.f, 1.f);

	// Locked on (and not mid locked-dash): the head and chest lead toward the target instead of the input,
	// since input only strafes.
	const bool bTrackTarget = bIsLockedOn && !HeavyMovement->IsHoldingDashFacing() && HeavyMovement->GetLockOnTarget();
	FVector LookDir = FVector::ZeroVector;
	if (bTrackTarget)
	{
		LookDir = (HeavyMovement->GetLockOnTarget()->GetActorLocation() - OwnerCharacter->GetActorLocation()).GetSafeNormal2D();
	}
	else if (InputStrength > KINDA_SMALL_NUMBER)
	{
		LookDir = InputAccel.GetSafeNormal2D();
	}

	float TargetSteer = 0.f;
	if (!LookDir.IsNearlyZero())
	{
		const FVector Local = OwnerCharacter->GetActorQuat().UnrotateVector(LookDir);
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

		// Bank into the turn: steering right drops the right shoulder. Not while strafing (no turn to bank into).
		if (!bTrackTarget)
		{
			TargetSpine.Roll = (Steer / 90.f) * MaxSpineBank * SpeedFactor * (bInvertSteerBank ? -1.f : 1.f);
		}
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

UAnimMontage* UHeavyAnimInstance::PlayOneShot(UAnimSequenceBase* Anim, float PlayRate)
{
	return Anim ? PlaySlotAnimationAsDynamicMontage(Anim, OneShotSlotName, OneShotBlendIn, OneShotBlendOut, PlayRate) : nullptr;
}

void UHeavyAnimInstance::HandleLiftoff()
{
	JumpMontage = PlayOneShot(JumpAnim, JumpAnimPlayRate);
}

bool UHeavyAnimInstance::IsPlayingJumpAnim() const
{
	// Montage_IsPlaying is false once the montage is blending out or was interrupted (dash / landing).
	return JumpMontage.IsValid() && Montage_IsPlaying(JumpMontage.Get());
}

void UHeavyAnimInstance::HandleDashStarted()
{
	if (!OwnerCharacter || !HeavyMovement)
	{
		return;
	}
	// Locked on with input: Heavy snaps to face the dash direction, so the forward dash always fits. Keep facing it
	// until the clip (minus its blend-out) is done, then the movement component turns Heavy back to the target.
	if (HeavyMovement->IsLockedOn() && !HeavyMovement->WasLastDashBackstep())
	{
		if (ForwardDashAnim)
		{
			const float Rate = FMath::Max(ForwardDashAnimPlayRate, 0.05f);
			HeavyMovement->HoldDashFacing(FMath::Max(0.f, ForwardDashAnim->GetPlayLength() / Rate - OneShotBlendOut));
			PlayOneShot(ForwardDashAnim, ForwardDashAnimPlayRate);
		}
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
