// Copyright (c) Improbable Worlds Ltd, All Rights Reserved


#include "RepMovComponent.h"
#include "PhysXVehicleManager.h"
#include <Runtime\Engine\Public\Net\UnrealNetwork.h>
#include "GameFramework/Actor.h"
#include <Runtime\Engine\Public\Net\UnrealNetwork.h>
#include "CustomWheeledVehicle.h"
#include "DrawDebugHelpers.h"

class ACustomWheeledVehicle;

void URepMovComponent::PreTick(float DeltaTime)
{
	if (GetOwnerRole() == ROLE_AutonomousProxy || GetOwner()->GetRemoteRole() == ROLE_SimulatedProxy)
	{
		LastMove = CreateMove(DeltaTime);

		UE_LOG(LogTemp, Warning, TEXT("Pre Simulate Move: %s"), *(GetOwner()->GetActorLocation().ToString()));
		SimulateMove(LastMove);
		UE_LOG(LogTemp, Warning, TEXT("Post Simulate Move: %s"), *(GetOwner()->GetActorLocation().ToString()));

		LastMove.BrakeInput = BrakeInput;
		LastMove.CurrentGear = GetCurrentGear();
		LastMove.HandbrakeInput = HandbrakeInput;
		LastMove.SteeringInput = SteeringInput;
		LastMove.ThrottleInput = ThrottleInput;

		UnacknowledgedMoves.Add(LastMove);
		//Server_SendMove(LastMove);
	}

	// We are the server and in control of the pawn.
	if (GetOwner()->GetRemoteRole() == ROLE_SimulatedProxy)
	{
		UpdateServerState(LastMove);
	}

	if (GetOwnerRole() == ROLE_SimulatedProxy)
	{
		ClientTick(DeltaTime);
	}
}

void URepMovComponent::SimulateMove(const FReplicatedVehicleState& Move)
{
	SteeringInput = Move.SteeringInput;
	ThrottleInput = Move.ThrottleInput;
	BrakeInput = Move.BrakeInput;
	HandbrakeInput = Move.HandbrakeInput;
	if (!GetUseAutoGears())
	{
		SetTargetGear(Move.CurrentGear, true);
	}

	// movement updates and replication
	if (PVehicle && UpdatedComponent)
	{
		APawn* MyOwner = Cast<APawn>(UpdatedComponent->GetOwner());
		if (MyOwner)
		{
			UpdateState(Move.DeltaTime);
		}
	}

	if (VehicleSetupTag != FPhysXVehicleManager::VehicleSetupTag)
	{
		RecreatePhysicsState();
	}

	TickVehicle(Move.DeltaTime);

	UWorld* World = GetWorld();
	FPhysScene* PhysScene = World->GetPhysicsScene();
	FPhysXVehicleManager* VehicleManager = FPhysXVehicleManager::GetVehicleManagerFromScene(PhysScene);
	VehicleManager->Update(PhysScene, Move.DeltaTime);
}

FReplicatedVehicleState URepMovComponent::CreateMove(float DeltaTime)
{
	FReplicatedVehicleState Move;
	Move.DeltaTime = DeltaTime;
	Move.SteeringInput = SteeringInput;
	Move.ThrottleInput = ThrottleInput;
	Move.BrakeInput = BrakeInput;
	Move.HandbrakeInput = HandbrakeInput;
	Move.CurrentGear = GetCurrentGear();
	Move.Time = GetWorld()->TimeSeconds;

	return Move;
}

void URepMovComponent::UpdateState(float DeltaTime)
{
	// update input values
	AController* Controller = GetController();

	// TODO: IsLocallyControlled will fail if the owner is unpossessed (i.e. Controller == nullptr);
	// Should we remove input instead of relying on replicated state in that case?
	if (Controller && Controller->IsLocalController())
	{
		if (bReverseAsBrake)
		{
			//for reverse as state we want to automatically shift between reverse and first gear
			if (FMath::Abs(GetForwardSpeed()) < WrongDirectionThreshold)	//we only shift between reverse and first if the car is slow enough. This isn't 100% correct since we really only care about engine speed, but good enough
			{
				if (RawThrottleInput < -KINDA_SMALL_NUMBER && GetCurrentGear() >= 0 && GetTargetGear() >= 0)
				{
					SetTargetGear(-1, true);
				}
				else if (RawThrottleInput > KINDA_SMALL_NUMBER && GetCurrentGear() <= 0 && GetTargetGear() <= 0)
				{
					SetTargetGear(1, true);
				}
			}
		}


		if (bUseRVOAvoidance)
		{
			CalculateAvoidanceVelocity(DeltaTime);
			UpdateAvoidance(DeltaTime);
		}

		SteeringInput = SteeringInputRate.InterpInputValue(DeltaTime, SteeringInput, CalcSteeringInput());
		ThrottleInput = ThrottleInputRate.InterpInputValue(DeltaTime, ThrottleInput, CalcThrottleInput());
		BrakeInput = BrakeInputRate.InterpInputValue(DeltaTime, BrakeInput, CalcBrakeInput());
		HandbrakeInput = HandbrakeInputRate.InterpInputValue(DeltaTime, HandbrakeInput, CalcHandbrakeInput());

		// and send to server
		ServerUpdateState(SteeringInput, ThrottleInput, BrakeInput, HandbrakeInput, GetCurrentGear());

		if (PawnOwner && PawnOwner->IsNetMode(NM_Client))
		{
			MarkForClientCameraUpdate();
		}
	}
	else
	{
		// use replicated values for remote pawns
		SteeringInput = ReplicatedState.SteeringInput;
		ThrottleInput = ReplicatedState.ThrottleInput;
		BrakeInput = ReplicatedState.BrakeInput;
		HandbrakeInput = ReplicatedState.HandbrakeInput;
		SetTargetGear(ReplicatedState.CurrentGear, true);
	}
}

void URepMovComponent::Server_SendMove_Implementation(FReplicatedVehicleState Move)
{
	ClientSimulatedTime += Move.DeltaTime;
	SimulateMove(Move);
	UpdateServerState(Move);
}

bool URepMovComponent::Server_SendMove_Validate(FReplicatedVehicleState Move)
{
	float ProposedTime = ClientSimulatedTime + Move.DeltaTime;
	bool ClientNotRunningAhead = ProposedTime < GetWorld()->TimeSeconds;
	if (!ClientNotRunningAhead) {
		UE_LOG(LogTemp, Error, TEXT("Client is running too fast."))
			return false;
	}
	/*
	if (!Move.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Received invalid move."))
			return false;
	}
	*/
	return true;
}

void URepMovComponent::UpdateServerState(const FReplicatedVehicleState& Move)
{
	ServerState.LastMove = Move;
	ServerState.Tranform = GetOwner()->GetActorTransform();
	ServerState.Velocity = GetVelocity();
}

void URepMovComponent::ClientTick(float DeltaTime)
{
}

void URepMovComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	//Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(URepMovComponent, ServerState);
}

void URepMovComponent::OnRep_ServerState()
{
	switch (GetOwnerRole())
	{
	case ROLE_AutonomousProxy:
		AutonomousProxy_OnRep_ServerState();
		break;
	case ROLE_SimulatedProxy:
		SimulatedProxy_OnRep_ServerState();
		break;
	default:
		break;
	}
}

void URepMovComponent::AutonomousProxy_OnRep_ServerState()
{
	DrawDebugBox(GetWorld(), ServerState.Tranform.GetLocation(), FVector(50.f, 50.f, 50.f), FColor::Blue, false, 5.f, ECC_WorldStatic, 1.f);
	UE_LOG(LogTemp, Warning, TEXT("Recieved on client: %s"), *(ServerState.Tranform.GetLocation().ToString()));
}

void URepMovComponent::SimulatedProxy_OnRep_ServerState()
{
	
}


void URepMovComponent::ClearAcknowledgeMoves(FReplicatedVehicleState LastMoveI)
{
	TArray<FReplicatedVehicleState> NewMoves;

	for (const FReplicatedVehicleState& Move : UnacknowledgedMoves)
	{
		if (Move.Time > LastMoveI.Time)
		{
			NewMoves.Add(Move);
		}
	}

	UnacknowledgedMoves = NewMoves;
}


FHermiteCubicSpline URepMovComponent::CreateSpline()
{
	FHermiteCubicSpline Spline;
	Spline.TargetLocation = ServerState.Tranform.GetLocation();
	Spline.StartLocation = ClientStartTransform.GetLocation();
	Spline.StartDerivative = ClientStartVelocity * VelocityToDerivative();
	Spline.TargetDerivative = ServerState.Velocity * VelocityToDerivative();
	return Spline;
}

void URepMovComponent::InterpolateLocation(const FHermiteCubicSpline& Spline, float LerpRatio)
{
	FVector NewLocation = Spline.InterpolateLocation(LerpRatio);
	GetOwner()->SetActorLocation(NewLocation, false, nullptr, ETeleportType::ResetPhysics);
}

void URepMovComponent::InterpolateVelocity(const FHermiteCubicSpline& Spline, float LerpRatio)
{
	FVector NewDerivative = Spline.InterpolateDerivative(LerpRatio);
	FVector NewVelocity = NewDerivative / VelocityToDerivative();
	//SetVelocity(NewVelocity);
}

void URepMovComponent::InterpolateRotation(float LerpRatio)
{
	FQuat TargetRotation = ServerState.Tranform.GetRotation();
	FQuat StartRotation = ClientStartTransform.GetRotation();

	FQuat NewRotation = FQuat::Slerp(StartRotation, TargetRotation, LerpRatio);

	GetOwner()->SetActorRotation(NewRotation, ETeleportType::ResetPhysics);
}

float URepMovComponent::VelocityToDerivative()
{
	return ClientTimeBetweenLastUpdates * 100;
}
