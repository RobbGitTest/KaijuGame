// KaijuGame

#include "TargetDummy.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ATargetDummy::ATargetDummy()
{
	PrimaryActorTick.bCanEverTick = true;
	Tags.Add(TEXT("LockOnTarget"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	// Basic shapes are 100 cm; pillar 300 cm wide x 800 cm tall, standing on the root.
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetRelativeLocation(FVector(0.f, 0.f, 400.f));
	Body->SetRelativeScale3D(FVector(3.f, 3.f, 8.f));
	if (CylinderMesh.Succeeded())
	{
		Body->SetStaticMesh(CylinderMesh.Object);
	}

	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(Root);
	Head->SetRelativeLocation(FVector(0.f, 0.f, 950.f));
	Head->SetRelativeScale3D(FVector(3.5f));
	if (SphereMesh.Succeeded())
	{
		Head->SetStaticMesh(SphereMesh.Object);
	}

	LockOnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("LockOnPoint"));
	LockOnPoint->SetupAttachment(Root);
	LockOnPoint->SetRelativeLocation(FVector(0.f, 0.f, 600.f));
	LockOnPoint->ComponentTags.Add(TEXT("LockOnPoint"));
}

void ATargetDummy::BeginPlay()
{
	Super::BeginPlay();
	StartLocation = GetActorLocation();
	MoveTime = 0.f;
}

void ATargetDummy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bMoveBackAndForth || MovePeriod <= 0.f)
	{
		return;
	}

	// Smooth ping-pong: 0 at the start, 1 at the far end, eased at both ends.
	MoveTime += DeltaSeconds;
	const float Alpha = 0.5f * (1.f - FMath::Cos(UE_TWO_PI * MoveTime / MovePeriod));
	const FVector WorldOffset = GetActorTransform().TransformVectorNoScale(MoveOffset);
	SetActorLocation(StartLocation + WorldOffset * Alpha);
}
