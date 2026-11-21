// KaijuGame

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TargetDummy.generated.h"

class UStaticMeshComponent;

/**
 * Test target for Heavy's lock-on. A tall pillar with a head marker.
 * Carries the "LockOnTarget" actor tag and a "LockOnPoint" component that the lock-on camera and reticle aim at.
 * Optionally moves back and forth between its start position and StartLocation + MoveOffset.
 */
UCLASS()
class KAIJUGAME_API ATargetDummy : public AActor
{
	GENERATED_BODY()

public:
	ATargetDummy();

	virtual void Tick(float DeltaSeconds) override;

	/** Move back and forth along MoveOffset. Off = stationary. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Movement")
	bool bMoveBackAndForth = false;

	/** End point of the patrol, relative to where the dummy was placed (drag the diamond widget in the viewport). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Movement", meta = (MakeEditWidget = true, EditCondition = "bMoveBackAndForth"))
	FVector MoveOffset = FVector(0.f, 3000.f, 0.f);

	/** Seconds for one full there-and-back trip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Movement", meta = (ClampMin = "0.1", Units = "s", EditCondition = "bMoveBackAndForth"))
	float MovePeriod = 6.f;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Head;

	/** Aim point for the lock-on (component tag "LockOnPoint"). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> LockOnPoint;

private:
	FVector StartLocation = FVector::ZeroVector;
	float MoveTime = 0.f;
};
