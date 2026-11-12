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
#include "HeavyMovementComponent.h"
#include "InputActionValue.h"

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
	const FVector2D Axis = Value.Get<FVector2D>() * MouseLookSensitivity;
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(bInvertMouseY ? Axis.Y : -Axis.Y);
}

void AHeavyCharacter::LookStick(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	AddControllerYawInput(Axis.X * GamepadLookRate.X * DeltaSeconds);
	const float Pitch = Axis.Y * GamepadLookRate.Y * DeltaSeconds;
	AddControllerPitchInput(bInvertGamepadY ? Pitch : -Pitch);
}

void AHeavyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateLean(DeltaSeconds);
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
