// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehicleMovementComponent4W.h"
#include "RepMovComponent.generated.h"

/**
 * 
 */

USTRUCT()
struct FWheeledState
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FReplicatedVehicleState LastMove;

	UPROPERTY()
	FTransform Tranform;

	UPROPERTY()
	FVector Velocity;
};

struct FHermiteCubicSpline
{
	FVector StartLocation, StartDerivative, TargetLocation, TargetDerivative;

	FVector InterpolateLocation(float LerpRatio) const
	{
		return FMath::CubicInterp(StartLocation, StartDerivative, TargetLocation, TargetDerivative, LerpRatio);
	}
	FVector InterpolateDerivative(float LerpRatio) const
	{
		return FMath::CubicInterpDerivative(StartLocation, StartDerivative, TargetLocation, TargetDerivative, LerpRatio);
	}
};

UCLASS()
class GDKSHOOTER_API URepMovComponent : public UWheeledVehicleMovementComponent4W
{
	GENERATED_BODY()

	virtual void PreTick(float DeltaTime) override;
	
	FReplicatedVehicleState LastMove;

	void SimulateMove(const FReplicatedVehicleState& Move);

private:
	FReplicatedVehicleState CreateMove(float DeltaTime);

protected:
	void ClearAcknowledgeMoves(FReplicatedVehicleState LastMove);

	FVector GetVelocity() { return  GetOwner()->GetVelocity(); };

	virtual void UpdateState(float DeltaTime) override;

	void ClientTick(float DeltaTime);

	FHermiteCubicSpline CreateSpline();

	void UpdateServerState(const FReplicatedVehicleState& Move);

	void InterpolateLocation(const FHermiteCubicSpline& Spline, float LerpRatio);
	void InterpolateVelocity(const FHermiteCubicSpline& Spline, float LerpRatio);
	void InterpolateRotation(float LerpRatio);
	float VelocityToDerivative();

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SendMove(FReplicatedVehicleState Move);

	UFUNCTION()
	void OnRep_ServerState();
	void AutonomousProxy_OnRep_ServerState();
	void SimulatedProxy_OnRep_ServerState();


	UPROPERTY(ReplicatedUsing = OnRep_ServerState)
	FWheeledState ServerState;

	
	TArray<FReplicatedVehicleState> UnacknowledgedMoves;

	float ClientTimeSinceUpdate;
	float ClientTimeBetweenLastUpdates;
	FTransform ClientStartTransform;
	FVector ClientStartVelocity;

	float ClientSimulatedTime;

	FVector ClientStartLocation;

};
