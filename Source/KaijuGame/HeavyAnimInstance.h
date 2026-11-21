// KaijuGame

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "HeavyAnimInstance.generated.h"

class ACharacter;
class UAnimSequenceBase;
class UHeavyMovementComponent;

/**
 * C++ base for Heavy's Animation Blueprint (ABP_Heavy).
 *
 * Reads UHeavyMovementComponent each frame and exposes simple values for the AnimGraph.
 * Drives the 2D blend space BS_Heavy_Movement:
 *  - X = LocomotionAlpha: 0 idle, 1 walk at normal top speed, 2 run at sprint speed. Normalised against the
 *    movement component's speeds, so retuning MaxHoverSpeed / SprintSpeed never requires editing the blend space.
 *  - Y = AirAlpha: -1 falling, 0 grounded, +1 thrusting / rising.
 *
 * One-shots (jump, forward dash, landing impact) are played from C++ as dynamic montages on OneShotSlotName,
 * so the AnimGraph only needs a Slot node after the blend space - no montage assets.
 *
 * Steering lean: SpineBoneRotation / HeadBoneRotation turn the chest and head toward the direction Heavy is
 * steering (the head leads), lean the chest forward with speed and thrust, and bank into turns. Both are already
 * converted to component space: use them on Transform (Modify) Bone nodes set to
 * Rotation Mode = Add to Existing, Rotation Space = Component Space.
 */
UCLASS(Transient, Blueprintable)
class KAIJUGAME_API UHeavyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// --- Tuning: blend space ---------------------------------------------------------------

	/** How quickly LocomotionAlpha follows the real speed (higher = snappier transitions). 0 = no smoothing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim", meta = (ClampMin = "0"))
	float LocomotionInterpSpeed = 8.f;

	/** Below this horizontal speed (cm/s) Heavy counts as standing still. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim", meta = (ClampMin = "0", Units = "cm/s"))
	float IdleSpeedThreshold = 20.f;

	/** Cap for LocomotionAlpha. 2 = full run; the dash and liftoff boosts can exceed sprint speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim", meta = (ClampMin = "0"))
	float MaxLocomotionAlpha = 2.f;

	/** How quickly AirAlpha moves between ground (0), thrust/rise (+1) and fall (-1). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim", meta = (ClampMin = "0"))
	float AirInterpSpeed = 6.f;

	/**
	 * If true, rising without thrust (e.g. still carrying the jump's momentum after Heavy_Jump ends) shows the
	 * thrust pose. If false, only real thrust shows the thrust pose and everything else in the air is the fall pose.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim")
	bool bRisingUsesThrustPose = false;

	/** With bRisingUsesThrustPose, Heavy shows the thrust pose while rising faster than this (cm/s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim", meta = (Units = "cm/s", EditCondition = "bRisingUsesThrustPose"))
	float RisingSpeedThreshold = 100.f;

	/** If true, an air dash shows the ground row (run pose) instead of the air poses. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim")
	bool bAirDashUsesGroundPose = true;

	// --- Tuning: lock-on strafe ------------------------------------------------------------

	/** How quickly the graph blends between free locomotion and the strafe blend space when locking / unlocking. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim|LockOn", meta = (ClampMin = "0"))
	float LockOnBlendSpeed = 6.f;

	/** How quickly StrafeX / StrafeY follow the real local velocity. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim|LockOn", meta = (ClampMin = "0"))
	float StrafeInterpSpeed = 8.f;

	// --- Tuning: one-shots -----------------------------------------------------------------

	/** Slot the one-shots play on. ABP_Heavy's AnimGraph needs a Slot node with this name. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim|OneShots")
	FName OneShotSlotName = TEXT("DefaultSlot");

	/** Played on liftoff (jump). Empty = none. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim|OneShots")
	TObjectPtr<UAnimSequenceBase> JumpAnim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim|OneShots", meta = (ClampMin = "0.05"))
	float JumpAnimPlayRate = 1.f;

	/** Played when a dash points roughly where Heavy faces. Other directions wait for lock-on dash anims. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim|OneShots")
	TObjectPtr<UAnimSequenceBase> ForwardDashAnim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim|OneShots", meta = (ClampMin = "0.05"))
	float ForwardDashAnimPlayRate = 1.f;

	/** Dash counts as "forward" when the cosine between dash direction and facing is at least this (0.5 = within 60 deg). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim|OneShots", meta = (ClampMin = "-1", ClampMax = "1"))
	float ForwardDashMinDot = 0.5f;

	/** Played when the hover catches Heavy after a fall. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim|OneShots")
	TObjectPtr<UAnimSequenceBase> LandingAnim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim|OneShots", meta = (ClampMin = "0.05"))
	float LandingAnimPlayRate = 1.f;

	/** Landing impact only plays if Heavy was falling at least this fast (cm/s), so small drops stay smooth. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim|OneShots", meta = (ClampMin = "0", Units = "cm/s"))
	float MinLandingImpactSpeed = 400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim|OneShots", meta = (ClampMin = "0"))
	float OneShotBlendIn = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim|OneShots", meta = (ClampMin = "0"))
	float OneShotBlendOut = 0.2f;

	// --- Tuning: steering lean (chest + head) ----------------------------------------------

	/** Share of the steering angle the chest turns (0.3 = turns 30% of the way toward the steering direction). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim|Lean", meta = (ClampMin = "0", ClampMax = "1"))
	float SpineYawShare = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim|Lean", meta = (ClampMin = "0", ClampMax = "90", Units = "deg"))
	float MaxSpineYaw = 20.f;

	/** Share of the steering angle the head turns (on top of the chest). The head leads the turn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim|Lean", meta = (ClampMin = "0", ClampMax = "1"))
	float HeadYawShare = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim|Lean", meta = (ClampMin = "0", ClampMax = "90", Units = "deg"))
	float MaxHeadYaw = 35.f;

	/** Chest forward lean at full run (scales with LocomotionAlpha). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim|Lean", meta = (ClampMin = "0", ClampMax = "45", Units = "deg"))
	float RunForwardLean = 8.f;

	/** Extra chest forward lean while thrusting with steering input. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim|Lean", meta = (ClampMin = "0", ClampMax = "45", Units = "deg"))
	float ThrustForwardLean = 10.f;

	/** Chest bank into a turn, at a 90 degree steering angle and full speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim|Lean", meta = (ClampMin = "0", ClampMax = "45", Units = "deg"))
	float MaxSpineBank = 8.f;

	/** How much of the chest's forward lean the head undoes, to keep the gaze level (0 = none, 1 = fully level). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim|Lean", meta = (ClampMin = "0", ClampMax = "1"))
	float HeadLevelling = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim|Lean", meta = (ClampMin = "0"))
	float SteerLeanInterpSpeed = 6.f;

	/** Flip if the chest leans backward instead of forward. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim|Lean")
	bool bInvertSteerPitch = false;

	/** Flip if the chest banks out of turns instead of into them. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim|Lean")
	bool bInvertSteerBank = false;

	/** Master switch for the steering lean. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim|Lean")
	bool bEnableSteerLean = true;

	// --- Values for the AnimGraph ----------------------------------------------------------

	/** Blend space X: 0 idle, 1 walk, 2 run. Smoothed. */
	UPROPERTY(BlueprintReadOnly, Category = "Heavy|Anim")
	float LocomotionAlpha = 0.f;

	/** Blend space Y: -1 falling, 0 on the ground, +1 thrusting / rising. Smoothed. */
	UPROPERTY(BlueprintReadOnly, Category = "Heavy|Anim")
	float AirAlpha = 0.f;

	/** Strafe blend space X: sideways speed, -1 full strafe left .. +1 full strafe right (normal top speed). */
	UPROPERTY(BlueprintReadOnly, Category = "Heavy|Anim")
	float StrafeX = 0.f;

	/** Strafe blend space Y: -1 walking backwards, 0 idle, 1 walk forward, 2 run forward. */
	UPROPERTY(BlueprintReadOnly, Category = "Heavy|Anim")
	float StrafeY = 0.f;

	/**
	 * Blend weight for the strafe blend space (Blend node alpha: 0 = BS_Heavy_Movement, 1 = BS_Heavy_Strafe).
	 * Rises when locked on and fades out while airborne so the air poses still play.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Heavy|Anim")
	float StrafeAlpha = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Heavy|Anim")
	bool bIsLockedOn = false;

	/** Chest (Spine2_Jnt) rotation, component space, for Transform (Modify) Bone "Add to Existing". */
	UPROPERTY(BlueprintReadOnly, Category = "Heavy|Anim")
	FRotator SpineBoneRotation = FRotator::ZeroRotator;

	/** Head (Neck_Jnt) rotation, component space, for Transform (Modify) Bone "Add to Existing". */
	UPROPERTY(BlueprintReadOnly, Category = "Heavy|Anim")
	FRotator HeadBoneRotation = FRotator::ZeroRotator;

	/** Signed angle (deg) from Heavy's facing to the steering input. + = right. 0 without input. */
	UPROPERTY(BlueprintReadOnly, Category = "Heavy|Anim")
	float SteerAngle = 0.f;

	/** Horizontal speed (cm/s). */
	UPROPERTY(BlueprintReadOnly, Category = "Heavy|Anim")
	float GroundSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Heavy|Anim")
	float VerticalSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Heavy|Anim")
	bool bIsMoving = false;

	UPROPERTY(BlueprintReadOnly, Category = "Heavy|Anim")
	bool bIsSprinting = false;

	UPROPERTY(BlueprintReadOnly, Category = "Heavy|Anim")
	bool bIsDashing = false;

	UPROPERTY(BlueprintReadOnly, Category = "Heavy|Anim")
	bool bIsGrounded = true;

	UPROPERTY(BlueprintReadOnly, Category = "Heavy|Anim")
	bool bIsThrusting = false;

	/** True while the jump one-shot is playing (not counting its blend-out). */
	UFUNCTION(BlueprintPure, Category = "Heavy|Anim")
	bool IsPlayingJumpAnim() const;

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UFUNCTION()
	void HandleLiftoff();

	UFUNCTION()
	void HandleDashStarted();

	UFUNCTION()
	void HandleHoverLanded();

private:
	void UpdateSteerLean(float DeltaSeconds);
	UAnimMontage* PlayOneShot(UAnimSequenceBase* Anim, float PlayRate);

	/** Dynamic montage created for the current/last jump. */
	TWeakObjectPtr<UAnimMontage> JumpMontage;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwnerCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UHeavyMovementComponent> HeavyMovement;

	/** Actor-space lean (pitch/yaw/roll), smoothed, before conversion to component space. */
	FRotator SpineLeanActor = FRotator::ZeroRotator;
	FRotator HeadLeanActor = FRotator::ZeroRotator;

	/** Vertical speed from the previous update, used to judge landing impact. */
	float LastAirVerticalSpeed = 0.f;

	/** Smoothed 0..1 lock-on weight (before the airborne fade). */
	float LockOnAlpha = 0.f;
};
