// KaijuGame

#include "HeavyCharacter.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "HeavyAnimInstance.h"
#include "HeavyHUDWidget.h"
#include "HeavyMovementComponent.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraFunctionLibrary.h"

AHeavyCharacter::AHeavyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UHeavyMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	HeavyMovement = Cast<UHeavyMovementComponent>(GetCharacterMovement());

	// Capsule sized to the Heavy_Base mesh (~240 wide, ~230 tall). The capsule floats HoverHeight above the ground.
	GetCapsuleComponent()->InitCapsuleSize(80.f, 115.f);

	// Free-movement scheme: the camera does not turn the body, movement input does.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(GetCapsuleComponent());

	// Heavy_Base is authored facing +Y with its pivot at the feet.
	GetMesh()->SetupAttachment(VisualRoot);
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -115.f), FRotator(0.f, -90.f, 0.f));

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetCapsuleComponent());
	CameraBoom->SetRelativeLocation(FVector(0.f, 0.f, 80.f));
	CameraBoom->TargetArmLength = 900.f;          // Pulled back to sell Heavy's size (GDD 6).
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 150.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 10.f;
	CameraBoom->CameraLagMaxDistance = 300.f;
	CameraBoom->bEnableCameraRotationLag = false;
	CameraBoom->ProbeSize = 20.f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->SetFieldOfView(90.f);
}

void AHeavyCharacter::BeginPlay()
{
	Super::BeginPlay();
	PreviousVelocity = GetVelocity();
	BaseFOV = FollowCamera->FieldOfView;
	BaseArmLength = CameraBoom->TargetArmLength;
	InitWingBeams();
	InitThrusters();

	if (HeavyMovement)
	{
		HeavyMovement->OnDashStarted.AddUniqueDynamic(this, &AHeavyCharacter::HandleDashStarted);
	}
}

void AHeavyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveHUD();
	Super::EndPlay(EndPlayReason);
}

void AHeavyCharacter::CreateHUD(APlayerController* PC)
{
	if (HUDWidget || !HUDWidgetClass || !PC || !PC->IsLocalController())
	{
		return;
	}
	HUDWidget = CreateWidget<UHeavyHUDWidget>(PC, HUDWidgetClass);
	if (HUDWidget)
	{
		HUDWidget->AddToViewport();
	}
}

void AHeavyCharacter::RemoveHUD()
{
	if (HUDWidget)
	{
		HUDWidget->RemoveFromParent();
		HUDWidget = nullptr;
	}
}

void AHeavyCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, MappingContextPriority);
			}
		}

		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->ViewPitchMin = CameraPitchMin;
			PC->PlayerCameraManager->ViewPitchMax = CameraPitchMax;
		}

		CreateHUD(PC);
	}
	else
	{
		RemoveHUD();
	}
}

void AHeavyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: expected an EnhancedInputComponent (check DefaultInputComponentClass)."), *GetNameSafe(this));
		return;
	}

	if (MoveAction)
	{
		Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHeavyCharacter::Move);
	}
	if (LookAction)
	{
		Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &AHeavyCharacter::Look);
	}
	if (LookStickAction)
	{
		Input->BindAction(LookStickAction, ETriggerEvent::Triggered, this, &AHeavyCharacter::LookStick);
	}
	if (JumpAction)
	{
		Input->BindAction(JumpAction, ETriggerEvent::Started, this, &AHeavyCharacter::JumpPressed);
		Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &AHeavyCharacter::JumpReleased);
		Input->BindAction(JumpAction, ETriggerEvent::Canceled, this, &AHeavyCharacter::JumpReleased);
	}
	if (BoostAction)
	{
		Input->BindAction(BoostAction, ETriggerEvent::Started, this, &AHeavyCharacter::BoostPressed);
		Input->BindAction(BoostAction, ETriggerEvent::Completed, this, &AHeavyCharacter::BoostReleased);
		Input->BindAction(BoostAction, ETriggerEvent::Canceled, this, &AHeavyCharacter::BoostReleased);
	}
	if (LockOnAction)
	{
		Input->BindAction(LockOnAction, ETriggerEvent::Started, this, &AHeavyCharacter::LockOnPressed);
	}
}

void AHeavyCharacter::LockOnPressed()
{
	ToggleLockOn();
}

void AHeavyCharacter::ToggleLockOn()
{
	if (IsLockedOn())
	{
		ClearLockOn();
		return;
	}

	AActor* Target = FindBestLockOnTarget();
	if (!Target)
	{
		return;
	}
	LockOnTarget = Target;
	if (HeavyMovement)
	{
		HeavyMovement->SetLockOnTarget(Target);
		HeavyMovement->SetLockOnFocusLocation(GetTargetAimPoint(Target));
	}
}

void AHeavyCharacter::ClearLockOn()
{
	LockOnTarget.Reset();
	if (HeavyMovement)
	{
		HeavyMovement->SetLockOnTarget(nullptr);
	}
}

FVector AHeavyCharacter::GetTargetAimPoint(const AActor* Target) const
{
	if (!Target)
	{
		return FVector::ZeroVector;
	}
	TArray<USceneComponent*> Points;
	Target->GetComponents<USceneComponent>(Points);
	for (const USceneComponent* Point : Points)
	{
		if (Point && Point->ComponentHasTag(LockOnPointTag))
		{
			return Point->GetComponentLocation();
		}
	}
	FVector Origin, Extent;
	Target->GetActorBounds(true, Origin, Extent);
	return Origin;
}

FVector AHeavyCharacter::GetLockOnTargetLocation() const
{
	return GetTargetAimPoint(LockOnTarget.Get());
}

AActor* AHeavyCharacter::FindBestLockOnTarget() const
{
	TArray<AActor*> Candidates;
	UGameplayStatics::GetAllActorsWithTag(this, LockOnTargetTag, Candidates);

	const FVector ViewOrigin = GetActorLocation();
	const FVector ViewDir = Controller ? Controller->GetControlRotation().Vector() : GetActorForwardVector();
	const float CosView = FMath::Cos(FMath::DegreesToRadians(LockOnViewAngle));

	AActor* BestInView = nullptr;
	float BestInViewScore = TNumericLimits<float>::Max();
	AActor* Nearest = nullptr;
	float NearestDist = TNumericLimits<float>::Max();

	for (AActor* Candidate : Candidates)
	{
		if (!Candidate || Candidate == this || Candidate->IsHidden())
		{
			continue;
		}
		const FVector ToTarget = GetTargetAimPoint(Candidate) - ViewOrigin;
		const float Dist = ToTarget.Size();
		if (Dist > LockOnMaxDistance || Dist < KINDA_SMALL_NUMBER)
		{
			continue;
		}
		if (Dist < NearestDist)
		{
			NearestDist = Dist;
			Nearest = Candidate;
		}
		const float Cos = FVector::DotProduct(ToTarget / Dist, ViewDir);
		if (Cos >= CosView)
		{
			// Prefer what's closest to the centre of view; distance is a light tie-breaker.
			const float Score = (1.f - Cos) + 0.25f * (Dist / LockOnMaxDistance);
			if (Score < BestInViewScore)
			{
				BestInViewScore = Score;
				BestInView = Candidate;
			}
		}
	}
	return BestInView ? BestInView : (bLockOnAllowOutsideView ? Nearest : nullptr);
}

void AHeavyCharacter::UpdateLockOn(float DeltaSeconds)
{
	if (!LockOnTarget.IsValid())
	{
		if (HeavyMovement && HeavyMovement->IsLockedOn())
		{
			ClearLockOn(); // Target was destroyed.
		}
		return;
	}

	const FVector AimPoint = GetLockOnTargetLocation();
	if (FVector::Dist(AimPoint, GetActorLocation()) > LockOnBreakDistance)
	{
		ClearLockOn();
		return;
	}
	if (HeavyMovement)
	{
		HeavyMovement->SetLockOnFocusLocation(AimPoint);
	}

	// Camera: sit behind Heavy on the Heavy->target line and look at a point between them, so both stay framed.
	if (Controller)
	{
		const FVector Pivot = CameraBoom->GetComponentLocation();
		const FVector Focus = FMath::Lerp(Pivot, AimPoint, LockOnCameraFocusBias);
		const FVector Flat = (AimPoint - Pivot).GetSafeNormal2D();
		if (!Flat.IsNearlyZero())
		{
			FRotator Desired = Flat.Rotation();
			const FVector ToFocus = Focus - Pivot;
			const float FlatDist = FMath::Max(ToFocus.Size2D(), 1.f);
			Desired.Pitch = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(ToFocus.Z, FlatDist)) + LockOnCameraPitchOffset,
				LockOnCameraPitchMin, LockOnCameraPitchMax);
			const FRotator Current = Controller->GetControlRotation();
			const FRotator NewRot = LockOnCameraInterpSpeed > 0.f
				? FMath::RInterpTo(Current, Desired, DeltaSeconds, LockOnCameraInterpSpeed)
				: Desired;
			Controller->SetControlRotation(NewRot);
		}
	}
}

void AHeavyCharacter::UpdateArmLength(float DeltaSeconds)
{
	if (!CameraBoom)
	{
		return;
	}
	const float Target = BaseArmLength + (IsLockedOn() ? LockOnExtraArmLength : 0.f);
	CameraBoom->TargetArmLength = ArmLengthInterpSpeed > 0.f
		? FMath::FInterpTo(CameraBoom->TargetArmLength, Target, DeltaSeconds, ArmLengthInterpSpeed)
		: Target;
}

void AHeavyCharacter::HandleDashStarted()
{
	if (!DashVFX || !HeavyMovement)
	{
		return;
	}
	const FVector DashDir = HeavyMovement->GetDashDirection();
	const FQuat DashQuat = (DashDir.IsNearlyZero() ? GetActorForwardVector() : DashDir).ToOrientationQuat();
	const FRotator SpawnRot = (DashQuat * DashVFXRotation.Quaternion()).Rotator();
	const FVector SpawnLoc = GetActorLocation() + GetActorQuat().RotateVector(DashVFXOffset);

	if (bAttachDashVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(DashVFX, GetRootComponent(), NAME_None, SpawnLoc, SpawnRot,
			DashVFXScale, EAttachLocation::KeepWorldPosition, true, ENCPoolMethod::None);
	}
	else
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, DashVFX, SpawnLoc, SpawnRot, DashVFXScale);
	}
}

void AHeavyCharacter::JumpPressed()
{
	if (HeavyMovement)
	{
		HeavyMovement->TryLiftoff();      // Only launches when hover-grounded.
		HeavyMovement->SetThrustHeld(true); // Thrust kicks in once airborne and no longer rising fast.
	}
}

void AHeavyCharacter::JumpReleased()
{
	if (HeavyMovement)
	{
		HeavyMovement->SetThrustHeld(false);
	}
}

void AHeavyCharacter::BoostPressed()
{
	BoostPressTime = GetWorld()->GetTimeSeconds();
	if (HeavyMovement)
	{
		HeavyMovement->SetSprinting(true);
	}
}

void AHeavyCharacter::BoostReleased()
{
	if (HeavyMovement)
	{
		HeavyMovement->SetSprinting(false);
	}
	if (BoostPressTime >= 0.f && GetWorld()->GetTimeSeconds() - BoostPressTime <= DashTapMaxDuration)
	{
		OnBoostTapped();
	}
	BoostPressTime = -1.f;
}

void AHeavyCharacter::OnBoostTapped_Implementation()
{
	if (!HeavyMovement)
	{
		return;
	}
	// Dash along the current movement input; with no input, back-step away from the camera's facing.
	const FVector CameraForward = Controller ? FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f).Vector() : GetActorForwardVector();
	HeavyMovement->TryDash(HeavyMovement->GetCurrentAcceleration(), CameraForward);
}

void AHeavyCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (!Controller)
	{
		return;
	}

	// Camera-relative, flattened to the ground plane.
	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FRotationMatrix YawMatrix(YawRotation);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::Y), Axis.X);
}

void AHeavyCharacter::Look(const FInputActionValue& Value)
{
	if (IsLockedOn() && !bAllowLookWhileLockedOn)
	{
		return;
	}
	const FVector2D Axis = Value.Get<FVector2D>() * MouseLookSensitivity;
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(bInvertMouseY ? Axis.Y : -Axis.Y);
}

void AHeavyCharacter::LookStick(const FInputActionValue& Value)
{
	if (IsLockedOn() && !bAllowLookWhileLockedOn)
	{
		return;
	}
	const FVector2D Axis = Value.Get<FVector2D>();
	const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	AddControllerYawInput(Axis.X * GamepadLookRate.X * DeltaSeconds);
	const float Pitch = Axis.Y * GamepadLookRate.Y * DeltaSeconds;
	AddControllerPitchInput(bInvertGamepadY ? Pitch : -Pitch);
}

void AHeavyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateLockOn(DeltaSeconds);
	UpdateArmLength(DeltaSeconds);
	UpdateLean(DeltaSeconds);
	UpdateCameraFOV(DeltaSeconds);
	UpdateThrusters();
	UpdateWingBeams(DeltaSeconds);
}

void AHeavyCharacter::InitWingBeams()
{
	WingBeamComponents.Reset();
	TArray<UNiagaraComponent*> NiagaraComponents;
	GetComponents<UNiagaraComponent>(NiagaraComponents);
	for (UNiagaraComponent* Comp : NiagaraComponents)
	{
		if (Comp && ((!WingBeamNamePrefix.IsEmpty() && Comp->GetName().StartsWith(WingBeamNamePrefix))
			|| Comp->ComponentHasTag(WingBeamTag)))
		{
			WingBeamComponents.Add(Comp);
		}
	}

	// Start off regardless of the components' Auto Activate setting.
	bWingBeamsOn = false;
	WingBeamOffTimer = 0.f;
	for (UNiagaraComponent* Comp : WingBeamComponents)
	{
		Comp->DeactivateImmediate();
	}
}

void AHeavyCharacter::UpdateWingBeams(float DeltaSeconds)
{
	if (!HeavyMovement || WingBeamComponents.IsEmpty())
	{
		return;
	}

	const bool bDashTrigger = bWingBeamsOnDash && HeavyMovement->IsDashing();
	const bool bSprintTrigger = bWingBeamsOnSprint && HeavyMovement->IsSprinting()
		&& GetVelocity().Size2D() > WingBeamMinSprintSpeed;

	bool bJumpTrigger = false;
	if (bWingBeamsOnJump)
	{
		const UHeavyAnimInstance* Anim = GetMesh() ? Cast<UHeavyAnimInstance>(GetMesh()->GetAnimInstance()) : nullptr;
		bJumpTrigger = (Anim && Anim->JumpAnim)
			? Anim->IsPlayingJumpAnim()
			: HeavyMovement->IsInLiftoff() && GetVelocity().Z > 0.f;
	}

	if (bDashTrigger || bSprintTrigger || bJumpTrigger)
	{
		WingBeamOffTimer = WingBeamOffDelay;
		if (!bWingBeamsOn)
		{
			SetWingBeamsOn(true);
		}
	}
	else if (bWingBeamsOn)
	{
		WingBeamOffTimer -= DeltaSeconds;
		if (WingBeamOffTimer <= 0.f)
		{
			SetWingBeamsOn(false);
		}
	}
}

void AHeavyCharacter::SetWingBeamsOn(bool bOn)
{
	bWingBeamsOn = bOn;
	for (UNiagaraComponent* Comp : WingBeamComponents)
	{
		if (!Comp)
		{
			continue;
		}
		if (bOn)
		{
			Comp->Activate(true); // Reset so the beams restart cleanly.
		}
		else if (bWingBeamsInstantOff)
		{
			Comp->DeactivateImmediate();
		}
		else
		{
			Comp->Deactivate();
		}
	}
}

void AHeavyCharacter::InitThrusters()
{
	ThrusterComponents.Reset();
	HotThrusterColors.Reset();
	CoolThrusterColors.Reset();

	TArray<UNiagaraComponent*> NiagaraComponents;
	GetComponents<UNiagaraComponent>(NiagaraComponents);
	for (UNiagaraComponent* Comp : NiagaraComponents)
	{
		if (WingBeamComponents.Contains(Comp))
		{
			continue; // Wing beams are switched on/off separately.
		}
		const TArray<FLinearColor> Hot = UNiagaraDataInterfaceArrayFunctionLibrary::GetNiagaraArrayColor(Comp, RedColorArrayParam);
		if (Hot.IsEmpty())
		{
			continue; // Not a thruster system.
		}
		if (HotThrusterColors.IsEmpty())
		{
			// All thrusters share one system, so read the source palettes once.
			HotThrusterColors = Hot;
			CoolThrusterColors = UNiagaraDataInterfaceArrayFunctionLibrary::GetNiagaraArrayColor(Comp, BlueColorArrayParam);
		}
		ThrusterComponents.Add(Comp);
	}

	// Start cool, and force the write even though bThrustersHot already defaults to false.
	bThrustersHot = false;
	ApplyThrusterState(false);
}

void AHeavyCharacter::UpdateThrusters()
{
	if (!HeavyMovement || ThrusterComponents.IsEmpty())
	{
		return;
	}

	const FVector Velocity = GetVelocity();
	const bool bSprintingAtSpeed = HeavyMovement->IsSprinting() && Velocity.Size2D() > SprintHotMinSpeed;
	// IsInLiftoff() stays true until landing; only the rising part of the jump counts as firing.
	const bool bLiftingOff = HeavyMovement->IsInLiftoff() && Velocity.Z > 0.f;
	const bool bAirThrust = HeavyMovement->IsThrusting();
	const bool bDashing = HeavyMovement->IsDashing();

	const bool bHot = bSprintingAtSpeed || bLiftingOff || bAirThrust || bDashing;
	if (bHot != bThrustersHot)
	{
		bThrustersHot = bHot;
		ApplyThrusterState(bHot);
	}
}

void AHeavyCharacter::ApplyThrusterState(bool bHot)
{
	const TArray<FLinearColor>& Colors = bHot ? HotThrusterColors : CoolThrusterColors;
	const float Emissive = bHot ? HotThrusterEmissive : CoolThrusterEmissive;

	for (UNiagaraComponent* Comp : ThrusterComponents)
	{
		if (!Comp)
		{
			continue;
		}
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayColor(Comp, ActiveColorArrayParam, Colors);
		Comp->SetVariableFloat(EmissiveParam, Emissive);
	}
}

void AHeavyCharacter::UpdateCameraFOV(float DeltaSeconds)
{
	if (!FollowCamera || !HeavyMovement)
	{
		return;
	}
	// Widen only once actually moving faster than normal top speed, so a tap or standing sprint does nothing.
	const bool bFastSprint = HeavyMovement->IsSprinting() && GetVelocity().Size2D() > HeavyMovement->MaxHoverSpeed * 0.9f;
	const float TargetFOV = BaseFOV + (bFastSprint ? SprintFOVIncrease : 0.f);
	FollowCamera->SetFieldOfView(FMath::FInterpTo(FollowCamera->FieldOfView, TargetFOV, DeltaSeconds, FOVInterpSpeed));
}

void AHeavyCharacter::UpdateLean(float DeltaSeconds)
{
	if (DeltaSeconds <= UE_KINDA_SMALL_NUMBER || !VisualRoot)
	{
		return;
	}

	// Horizontal acceleration in Heavy's local frame (X forward, Y right).
	const FVector Velocity = GetVelocity();
	FVector Accel = (Velocity - PreviousVelocity) / DeltaSeconds;
	Accel.Z = 0.f;
	PreviousVelocity = Velocity;

	// Smooth the raw acceleration a little so frame-time noise doesn't jitter the mesh.
	SmoothedAcceleration = FMath::VInterpTo(SmoothedAcceleration, Accel, DeltaSeconds, 15.f);
	const FVector LocalAccel = GetActorQuat().UnrotateVector(SmoothedAcceleration);

	const float ForwardAlpha = FMath::Clamp(LocalAccel.X / LeanReferenceAcceleration, -1.f, 1.f);
	const float RightAlpha = FMath::Clamp(LocalAccel.Y / LeanReferenceAcceleration, -1.f, 1.f);

	// Accelerating forward dips the nose; lateral acceleration (incl. turning) banks into the turn.
	FRotator TargetLean;
	TargetLean.Pitch = -ForwardAlpha * MaxPitchAngle * (bInvertPitch ? -1.f : 1.f);
	TargetLean.Roll = RightAlpha * MaxBankAngle * (bInvertBank ? -1.f : 1.f);
	TargetLean.Yaw = 0.f;

	CurrentLean = FMath::RInterpTo(CurrentLean, TargetLean, DeltaSeconds, LeanInterpSpeed);
	VisualRoot->SetRelativeRotation(CurrentLean);
}
