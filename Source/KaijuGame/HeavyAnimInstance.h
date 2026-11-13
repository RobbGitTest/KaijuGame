// KaijuGame

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "HeavyAnimInstance.generated.h"

class ACharacter;
class UHeavyMovementComponent;

/**
 * C++ base for Heavy's Animation Blueprint (ABP_Heavy).
 *
 * Reads UHeavyMovementComponent each frame and exposes simple values for the AnimGraph.
 * LocomotionAlpha drives BS_Heavy_Locomotion: 0 = idle, 1 = walk at normal top speed,
 * 2 = run at sprint speed. It is normalised against the movement component's speeds,
 * so retuning MaxHoverSpeed / SprintSpeed never requires editing the blend space.
 */
UCLASS(Transient, Blueprintable)
class KAIJUGAME_API UHeavyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// --- Tuning ----------------------------------------------------------------------------

	/** How quickly LocomotionAlpha follows the real speed (higher = snappier transitions). 0 = no smoothing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim", meta = (ClampMin = "0"))
	float LocomotionInterpSpeed = 8.f;

	/** Below this horizontal speed (cm/s) Heavy counts as standing still. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim", meta = (ClampMin = "0", Units = "cm/s"))
	float IdleSpeedThreshold = 20.f;

	/** Cap for LocomotionAlpha. 2 = full run; the dash and liftoff boosts can exceed sprint speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Heavy|Anim", meta = (ClampMin = "0"))
	float MaxLocomotionAlpha = 2.f;

	// --- Values for the AnimGraph ----------------------------------------------------------

	/** Blend space input: 0 idle, 1 walk, 2 run. Smoothed. */
	UPROPERTY(BlueprintReadOnly, Category = "Heavy|Anim")
	float LocomotionAlpha = 0.f;

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

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwnerCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UHeavyMovementComponent> HeavyMovement;
};
