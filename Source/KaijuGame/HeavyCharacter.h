// KaijuGame

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HeavyCharacter.generated.h"

class UCameraComponent;
class UHeavyHUDWidget;
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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Called when the sprint/boost button is released within DashTapMaxDuration.
	 * Default implementation dashes (UHeavyMovementComponent::TryDash). Override in Blueprint to add effects
	 * (call the parent to keep the dash).
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Heavy|Sprint")
	void OnBoostTapped();

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

	/** Boolean. Space / A: press = liftoff, hold in the air = fuel thrust. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy|Input")
	TObjectPtr<UInputAction> JumpAction;

	/** Boolean. Left Shift / B: hold = sprint, quick tap = OnBoostTapped (future dash). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy|Input")
	TObjectPtr<UInputAction> BoostAction;

	/** A boost press released faster than this (s) counts as a tap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Input", meta = (ClampMin = "0", Units = "s"))
	float DashTapMaxDuration = 0.15f;

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

	/** Extra field of view (degrees) while sprinting at speed. 0 disables. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Camera", meta = (ClampMin = "0", ClampMax = "40"))
	float SprintFOVIncrease = 8.f;

	/** How quickly the FOV blends in/out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Camera", meta = (ClampMin = "0"))
	float FOVInterpSpeed = 4.f;

	// --- HUD -------------------------------------------------------------------------------

	/** Widget created for the local player (WBP_HeavyHUD). Leave empty for no HUD. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heavy|HUD")
	TSubclassOf<UHeavyHUDWidget> HUDWidgetClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Heavy|HUD")
	TObjectPtr<UHeavyHUDWidget> HUDWidget;

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
	void JumpPressed();
	void JumpReleased();
	void BoostPressed();
	void BoostReleased();

private:
	void UpdateLean(float DeltaSeconds);
	void UpdateCameraFOV(float DeltaSeconds);
	void CreateHUD(APlayerController* PC);
	void RemoveHUD();

	float BaseFOV = 90.f;
	float BoostPressTime = -1.f;

	FVector PreviousVelocity = FVector::ZeroVector;
	FVector SmoothedAcceleration = FVector::ZeroVector;
	FRotator CurrentLean = FRotator::ZeroRotator;
};
