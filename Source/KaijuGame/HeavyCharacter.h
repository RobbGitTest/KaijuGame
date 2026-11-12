// KaijuGame

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HeavyCharacter.generated.h"

class UCameraComponent;
class UHeavyMovementComponent;
class UInputAction;
class UInputMappingContext;
class USpringArmComponent;
struct FInputActionValue;

/**
 * Heavy, the playable hover mech (Milestone 1: controller prototype).
 *
 * - Grounded hover via UHeavyMovementComponent.
 * - Free-movement scheme: Heavy turns to face camera-relative movement input.
 * - Third-person chase camera on a spring arm with light lag.
 * - Enhanced Input; all actions and the mapping context are assigned in the Blueprint subclass.
 * - Procedural banking/pitching of the mesh from acceleration (visual only, the capsule stays upright).
 */
UCLASS()
class KAIJUGAME_API AHeavyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AHeavyCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void NotifyControllerChanged() override;

	UFUNCTION(BlueprintPure, Category = "Heavy")
	UHeavyMovementComponent* GetHeavyMovement() const { return HeavyMovement; }

	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }

protected:
	virtual void BeginPlay() override;

	// --- Components ------------------------------------------------------------------------

	/** Pivot at the capsule centre that the mesh hangs from; banking/pitching rotates this. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> VisualRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(Transient)
	TObjectPtr<UHeavyMovementComponent> HeavyMovement;

	// --- Input -----------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy|Input")
	int32 MappingContextPriority = 0;

	/** Axis2D: X = right, Y = forward. WASD and left stick. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy|Input")
	TObjectPtr<UInputAction> MoveAction;

	/** Axis2D mouse delta. Applied as-is (times MouseLookSensitivity). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy|Input")
	TObjectPtr<UInputAction> LookAction;

	/** Axis2D right stick. Treated as a turn rate (times GamepadLookRate and delta time). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy|Input")
	TObjectPtr<UInputAction> LookStickAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Input", meta = (ClampMin = "0"))
	float MouseLookSensitivity = 1.f;

	/** Right-stick turn rate at full deflection (deg/s) for yaw and pitch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Input", meta = (ClampMin = "0"))
	FVector2D GamepadLookRate = FVector2D(180.f, 120.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Input")
	bool bInvertMouseY = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Input")
	bool bInvertGamepadY = false;

	// --- Camera ----------------------------------------------------------------------------

	/** Camera pitch limits (degrees) applied to the player camera manager. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Camera")
	float CameraPitchMin = -60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Camera")
	float CameraPitchMax = 45.f;

	// --- Visual lean -----------------------------------------------------------------------

	/** Max mesh roll into turns / lateral acceleration (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Lean", meta = (ClampMin = "0", ClampMax = "60"))
	float MaxBankAngle = 15.f;

	/** Max mesh pitch from forward/back acceleration (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Lean", meta = (ClampMin = "0", ClampMax = "60"))
	float MaxPitchAngle = 10.f;

	/** Acceleration (cm/s^2) that produces the full bank/pitch angle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Lean", meta = (ClampMin = "1"))
	float LeanReferenceAcceleration = 2400.f;

	/** How quickly the lean follows acceleration (higher = snappier). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Lean", meta = (ClampMin = "0"))
	float LeanInterpSpeed = 6.f;

	/** Flip these if the mesh leans the wrong way. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Lean")
	bool bInvertBank = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Lean")
	bool bInvertPitch = false;

	// --- Input handlers --------------------------------------------------------------------
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void LookStick(const FInputActionValue& Value);

private:
	void UpdateLean(float DeltaSeconds);

	FVector PreviousVelocity = FVector::ZeroVector;
	FVector SmoothedAcceleration = FVector::ZeroVector;
	FRotator CurrentLean = FRotator::ZeroRotator;
};
