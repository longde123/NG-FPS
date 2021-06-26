// Copyright (c) Improbable Worlds Ltd, All Rights Reserved


#include "NetPhysVehicleMovementComponent.h"
#include "TP_VehiclePawn.h"


#include "DrawDebugHelpers.h" 
#include "Components/SkeletalMeshComponent.h" 
#include "Engine/World.h" 
#include "Engine/Player.h" 
#include "Engine/NetDriver.h" 
#include "Engine/NetworkObjectList.h" 
#include "GameFramework/GameNetworkManager.h" 
#include "GameFramework/PlayerController.h" 
#include "GameFramework/GameState.h"


DECLARE_CYCLE_STAT(TEXT("VehicleMovement"), STAT_VehicleMovement, STATGROUP_NetPhysVehicle);
DECLARE_CYCLE_STAT(TEXT("VehicleMovement Tick"), STAT_VehicleMovementTick, STATGROUP_NetPhysVehicle);
DECLARE_CYCLE_STAT(TEXT("SmoothCorrection"), STAT_VehicleMovementSmoothCorrection, STATGROUP_NetPhysVehicle);
DECLARE_CYCLE_STAT(TEXT("SmoothClientPosition_Interp"), STAT_VehicleMovementSmoothClientPosition_Interp, STATGROUP_NetPhysVehicle);
DECLARE_CYCLE_STAT(TEXT("SmoothClientPosition Visual"), STAT_VehicleMovementSmoothclientPosition_Visual, STATGROUP_NetPhysVehicle);
DECLARE_CYCLE_STAT(TEXT("PerformMovement"), STAT_VehicleMovementPerformMovement, STATGROUP_NetPhysVehicle);
DECLARE_CYCLE_STAT(TEXT("ReplicateMoveToServer"), STAT_VehicleMovementReplicateMoveToServer, STATGROUP_NetPhysVehicle);
DECLARE_CYCLE_STAT(TEXT("CombineMove"), STAT_VehicleMovementCombineNetMove, STATGROUP_NetPhysVehicle);
DECLARE_CYCLE_STAT(TEXT("ServerMove"), STAT_VehicleMovementServerMove, STATGROUP_NetPhysVehicle);

const float UNetPhysVehicleMovementComponent::MIN_TICK_TIME = 1e-6f;

UNetPhysVehicleMovementComponent::UNetPhysVehicleMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NetworkSmoothingMode = ENetPhysSmoothingMode::Exponential;

	NetworkSmoothLocationTime = 0.0100f;
	NetworkSmoothRotationTime = 0.050f;
	NetworkMinTimeBetweenClientAckGoodMoves = 0.10f;
	NetworkMinTimeBetweenClientAdjustments = 0.10f;
	NetworkMinTimeBetweenClientAdjustmentsLargeCorrection = 0.05f;
	NetworkLargeClientCorrectionDistance = 15.0f;

	ListenServerNetworkSimulatedSmoothLocationTime = 0.040f;
	ListenServerNetworkSimulatedSmoothRotationTime = 0.033f;

	NetworkSimulatedSmoothLocationTime = 0.100f;
	NetworkSimulatedSmoothRotationTime = 0.050f;

	MinTimeBetweenTimeStampResets = 4.f * 60.f;
	LastTimeStampResetServerTime = 0.f;

	ServerLastClientGoodMoveAckTime = -1.f;
	ServerLastClientAdjustmentTime = -1.f;
}

namespace VehicleMovementCVars
{
	// Listen server smoothing
	static int32 NetEnableListenServerSmoothing = 1;
	FAutoConsoleVariableRef CVarNetEnableListenServerSmoothing(
		TEXT("p.NetEnableListenServerSmoothing"),
		NetEnableListenServerSmoothing,
		TEXT("Whether to enable mesh smoothing on listen servers for the local view of remote clients.\n")
		TEXT("0: Disable, 1: Enable"),
		ECVF_Default);

	// Latent proxy prediction
	static int32 NetEnableSkipProxyPredictionOnNetUpdate = 1;
	FAutoConsoleVariableRef CVarNetEnableSkipProxyPredictionOnNetUpdate(
		TEXT("p.NetEnableSkipProxyPredictionOnNetUpdate"),
		NetEnableSkipProxyPredictionOnNetUpdate,
		TEXT("Whether to allow proxies to skip prediction on frames with a network position update, if bNetworkSkipProxyPredictionOnNetUpdate is also true on the movement component.\n")
		TEXT("0: Disable, 1: Enable"),
		ECVF_Default);

	// Logging when character is stuck. Off by default in shipping.
#if UE_BUILD_SHIPPING
	static float StuckWarningPeriod = -1.f;
#else
	static float StuckWarningPeriod = 1.f;
#endif

	FAutoConsoleVariableRef CVarStuckWarningPeriod(
		TEXT("p.CharacterStuckWarningPeriod"),
		StuckWarningPeriod,
		TEXT("How often (in seconds) we are allowed to log a message about being stuck in geometry.\n")
		TEXT("<0: Disable, >=0: Enable and log this often, in seconds."),
		ECVF_Default);

	static int32 NetEnableMoveCombining = 1;
	FAutoConsoleVariableRef CVarNetEnableMoveCombining(
		TEXT("p.NetEnableMoveCombining"),
		NetEnableMoveCombining,
		TEXT("Whether to enable move combining on the client to reduce bandwidth by combining similar moves.\n")
		TEXT("0: Disable, 1: Enable"),
		ECVF_Default);

	static int32 NetEnableMoveCombiningOnStaticBaseChange = 1;
	FAutoConsoleVariableRef CVarNetEnableMoveCombiningOnStaticBaseChange(
		TEXT("p.NetEnableMoveCombiningOnStaticBaseChange"),
		NetEnableMoveCombiningOnStaticBaseChange,
		TEXT("Whether to allow combining client moves when moving between static geometry.\n")
		TEXT("0: Disable, 1: Enable"),
		ECVF_Default);

	static float NetMoveCombiningAttachedLocationTolerance = 0.01f;
	FAutoConsoleVariableRef CVarNetMoveCombiningAttachedLocationTolerance(
		TEXT("p.NetMoveCombiningAttachedLocationTolerance"),
		NetMoveCombiningAttachedLocationTolerance,
		TEXT("Tolerance for relative location attachment change when combining moves. Small tolerances allow for very slight jitter due to transform updates."),
		ECVF_Default);

	static float NetMoveCombiningAttachedRotationTolerance = 0.01f;
	FAutoConsoleVariableRef CVarNetMoveCombiningAttachedRotationTolerance(
		TEXT("p.NetMoveCombiningAttachedRotationTolerance"),
		NetMoveCombiningAttachedRotationTolerance,
		TEXT("Tolerance for relative rotation attachment change when combining moves. Small tolerances allow for very slight jitter due to transform updates."),
		ECVF_Default);

	static float NetStationaryRotationTolerance = 0.1f;
	FAutoConsoleVariableRef CVarNetStationaryRotationTolerance(
		TEXT("p.NetStationaryRotationTolerance"),
		NetStationaryRotationTolerance,
		TEXT("Tolerance for GetClientNetSendDeltaTime() to remain throttled when small control rotation changes occur."),
		ECVF_Default);

	static int32 NetUseClientTimestampForReplicatedTransform = 1;
	FAutoConsoleVariableRef CVarNetUseClientTimestampForReplicatedTransform(
		TEXT("p.NetUseClientTimestampForReplicatedTransform"),
		NetUseClientTimestampForReplicatedTransform,
		TEXT("If enabled, use client timestamp changes to track the replicated transform timestamp, otherwise uses server tick time as the timestamp.\n")
		TEXT("Game session usually needs to be restarted if this is changed at runtime.\n")
		TEXT("0: Disable, 1: Enable"),
		ECVF_Default);

	static int32 ReplayUseInterpolation = 0;
	FAutoConsoleVariableRef CVarReplayUseInterpolation(
		TEXT("p.ReplayUseInterpolation"),
		ReplayUseInterpolation,
		TEXT(""),
		ECVF_Default);

	static int32 ReplayLerpAcceleration = 0;
	FAutoConsoleVariableRef CVarReplayLerpAcceleration(
		TEXT("p.ReplayLerpAcceleration"),
		ReplayLerpAcceleration,
		TEXT(""),
		ECVF_Default);

	static int32 FixReplayOverSampling = 1;
	FAutoConsoleVariableRef CVarFixReplayOverSampling(
		TEXT("p.FixReplayOverSampling"),
		FixReplayOverSampling,
		TEXT("If 1, remove invalid replay samples that can occur due to oversampling (sampling at higher rate than physics is being ticked)"),
		ECVF_Default);

	static int32 ForceJumpPeakSubstep = 1;
	FAutoConsoleVariableRef CVarForceJumpPeakSubstep(
		TEXT("p.ForceJumpPeakSubstep"),
		ForceJumpPeakSubstep,
		TEXT("If 1, force a jump substep to always reach the peak position of a jump, which can often be cut off as framerate lowers."),
		ECVF_Default);

	static float NetServerMoveTimestampExpiredWarningThreshold = 1.0f;
	FAutoConsoleVariableRef CVarNetServerMoveTimestampExpiredWarningThreshold(
		TEXT("net.NetServerMoveTimestampExpiredWarningThreshold"),
		NetServerMoveTimestampExpiredWarningThreshold,
		TEXT("Tolerance for ServerMove() to warn when client moves are expired more than this time threshold behind the server."),
		ECVF_Default);

#if !UE_BUILD_SHIPPING

	int32 NetShowCorrections = 0;
	FAutoConsoleVariableRef CVarNetShowCorrections(
		TEXT("p.NetShowCorrections"),
		NetShowCorrections,
		TEXT("Whether to draw client position corrections (red is incorrect, green is corrected).\n")
		TEXT("0: Disable, 1: Enable"),
		ECVF_Cheat);

	float NetCorrectionLifetime = 4.f;
	FAutoConsoleVariableRef CVarNetCorrectionLifetime(
		TEXT("p.NetCorrectionLifetime"),
		NetCorrectionLifetime,
		TEXT("How long a visualized network correction persists.\n")
		TEXT("Time in seconds each visualized network correction persists."),
		ECVF_Cheat);

#endif // !UE_BUILD_SHIPPING


#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	static float NetForceClientAdjustmentPercent = 0.f;
	FAutoConsoleVariableRef CVarNetForceClientAdjustmentPercent(
		TEXT("p.NetForceClientAdjustmentPercent"),
		NetForceClientAdjustmentPercent,
		TEXT("Percent of ServerCheckClientError checks to return true regardless of actual error.\n")
		TEXT("Useful for testing client correction code.\n")
		TEXT("<=0: Disable, 0.05: 5% of checks will return failed, 1.0: Always send client adjustments"),
		ECVF_Cheat);

	static float NetForceClientServerMoveLossPercent = 0.f;
	FAutoConsoleVariableRef CVarNetForceClientServerMoveLossPercent(
		TEXT("p.NetForceClientServerMoveLossPercent"),
		NetForceClientServerMoveLossPercent,
		TEXT("Percent of ServerMove calls for client to not send.\n")
		TEXT("Useful for testing server force correction code.\n")
		TEXT("<=0: Disable, 0.05: 5% of checks will return failed, 1.0: never send server moves"),
		ECVF_Cheat);

	static float NetForceClientServerMoveLossDuration = 0.f;
	FAutoConsoleVariableRef CVarNetForceClientServerMoveLossDuration(
		TEXT("p.NetForceClientServerMoveLossDuration"),
		NetForceClientServerMoveLossDuration,
		TEXT("Duration in seconds for client to drop ServerMove calls when NetForceClientServerMoveLossPercent check passes.\n")
		TEXT("Useful for testing server force correction code.\n")
		TEXT("Duration of zero means single frame loss."),
		ECVF_Cheat);

	static int32 VisualizeMovement = 0;
	FAutoConsoleVariableRef CVarVisualizeMovement(
		TEXT("p.VisualizeMovement"),
		VisualizeMovement,
		TEXT("Whether to draw in-world debug information for character movement.\n")
		TEXT("0: Disable, 1: Enable"),
		ECVF_Cheat);

	static int32 NetVisualizeSimulatedCorrections = 0;
	FAutoConsoleVariableRef CVarNetVisualizeSimulatedCorrections(
		TEXT("p.NetVisualizeSimulatedCorrections"),
		NetVisualizeSimulatedCorrections,
		TEXT("")
		TEXT("0: Disable, 1: Enable"),
		ECVF_Cheat);

	static int32 DebugTimeDiscrepancy = 0;
	FAutoConsoleVariableRef CVarDebugTimeDiscrepancy(
		TEXT("p.DebugTimeDiscrepancy"),
		DebugTimeDiscrepancy,
		TEXT("Whether to log detailed Movement Time Discrepancy values for testing")
		TEXT("0: Disable, 1: Enable Detection logging, 2: Enable Detection and Resolution logging"),
		ECVF_Cheat);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UNetPhysVehicleMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	VehicleOwner = CastChecked<class ATP_VehiclePawn>(GetOwner());
}

void UNetPhysVehicleMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	SCOPED_NAMED_EVENT(UNetPhysVehicleMovementComponent_TickComponent, FColor::Yellow);
	SCOPE_CYCLE_COUNTER(STAT_VehicleMovement);
	SCOPE_CYCLE_COUNTER(STAT_VehicleMovementTick);

	if (!HasValidData() || ShouldSkipUpdate(DeltaTime))
	{
		return;
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// It is possible for the super call to invalidate the data we need for this component, so check it again
	if (!HasValidData())
	{
		return;
	}


	if (VehicleOwner->Role > ROLE_SimulatedProxy)
	{
		const bool bIsClient = (VehicleOwner->Role == ROLE_AutonomousProxy && IsNetMode(NM_Client));
		if (bIsClient)
		{
			// We may have received an update from the server... 
			// Replays moves that have not yet been acknowledged by the server 
			ClientUpdatePositionAfterServerUpdate();
		}
		if (VehicleOwner->Role == ROLE_Authority)
		{
			// Move the pawn if we are the server 
			PerformMovement(DeltaTime);
		}

		else if (bIsClient)
		{
			// Send the current movement to the server so it can give us a correction
			ReplicateMoveToServer(DeltaTime);
		}
	}
	else if (VehicleOwner->GetRemoteRole() == ROLE_AutonomousProxy) {
		//Smooth on listen server for local view of remote client
		if (VehicleMovementCVars::NetEnableListenServerSmoothing && !bNetworkSmoothingComplete && IsNetMode(NM_ListenServer))
		{
			SmoothClientPosition(DeltaTime);
		}
	}

	else if (VehicleOwner->Role == ROLE_SimulatedProxy && !bNetworkSmoothingComplete)
	{
		// Smooth the client position to any move that has been sent to us from the server 
		// Internally calls SmoothClientPosition_Interpolate which updates the values in the client data. And then calls SmoothClient Position_UpdateVisuals which actually moves the vehicle to the updated data set in SmoothClient Position_Interpolate 
		SmoothClientPosition(DeltaTime);
	}

}

bool UNetPhysVehicleMovementComponent::HasValidData() const
{
	return (UpdatedComponent && UpdatedPrimitive && VehicleOwner);
}

bool UNetPhysVehicleMovementComponent::ClientUpdatePositionAfterServerUpdate()
{
	if (!HasValidData())
	{
		return false;
	}

	FNetPhysNetworkPredictionData_Client_Vehicle* ClientData = GetPredictionData_Client_Vehicle();
	check(ClientData);

	if (!ClientData->bUpdatePosition || bIgnoreClientMovementErrorChecksAndCorrection)
	{
		return false;
	}

	ClientData->bUpdatePosition = false;

	if (ClientData->SavedMoves.Num() == 0)
	{
		return false;
	}

	// Replay moves that have not yet been acked 
	for (int32 i = 0; i < ClientData->SavedMoves.Num(); i++)
	{
		FSavedMove_Vehicle* const CurrentMove = ClientData->SavedMoves[i].Get();
		checkSlow(CurrentMove != nullptr);

		CurrentMove->PrepMoveFor(VehicleOwner);
		MoveAutonomous(CurrentMove->TimeStamp, CurrentMove->DeltaTime, CurrentMove->GetCompressedFlags());
		CurrentMove->PostUpdate(VehicleOwner);
	}
	if (FSavedMove_Vehicle* const PendingMove = ClientData->PendingMove.Get())
	{
		PendingMove->bForceNoCombine = true;
	}

	return (ClientData->SavedMoves.Num() > 0);
}

void UNetPhysVehicleMovementComponent::SmoothCorrection(const FVector& OldLocation, const FQuat& OldRotation, const FVector& NewLocation, const FQuat& NewRotation)
{
	if (!HasValidData())
	{
		return;
	}

	// IMPROBABLE-BEGIN - Treat as if smoothing is disabled on non-authoritative server
	if (GetNetMode() == NM_DedicatedServer)
	{
		//UE_LOG(LogCharacterNetSmoothing, Verbose, TEXT("Server Proxy SmoothCorrection to Pos=%s Rot=%s"), *NewLocation.ToString(), *NewRotation.ToString());
		UpdatedComponent->SetWorldLocationAndRotation(NewLocation, NewRotation, false, nullptr, ETeleportType::TeleportPhysics);
		bNetworkSmoothingComplete = true;
		return;
	}
	// IMPROBABLE-END

	checkSlow(GetNetMode() != NM_DedicatedServer);
	checkSlow(GetNetMode() != NM_Standalone);

	// Only client proxies or remote clients on a listen server should run this code.
	const bool bIsSimulatedProxy = (VehicleOwner->GetLocalRole() == ROLE_SimulatedProxy);
	const bool bIsRemoteAutoProxy = (VehicleOwner->GetRemoteRole() == ROLE_AutonomousProxy);
	ensure(bIsSimulatedProxy || bIsRemoteAutoProxy);

	// Getting a correction means new data, so smoothing needs to run.
	bNetworkSmoothingComplete = false;


	if (NetworkSmoothingMode == ENetPhysSmoothingMode::Disabled)
	{
		UpdatedComponent->SetWorldLocationAndRotation(NewLocation, NewRotation, false, nullptr, ETeleportType::TeleportPhysics);
		bNetworkSmoothingComplete = true;
	}
	else if (FNetPhysNetworkPredictionData_Client_Vehicle* ClientData = GetPredictionData_Client_Vehicle())
	{
		const UWorld* MyWorld = GetWorld();
		if (!ensure(MyWorld != nullptr))
		{
			return;
		}

		//new try
		//FVector NewToOldVector = (OldLocation - NewLocation);
		//ClientData->LocationOffset = ClientData->LocationOffset + NewToOldVector;
		//

		if (NetworkSmoothingMode == ENetPhysSmoothingMode::Linear)
		{
			ClientData->OriginalLocationOffset = ClientData->LocationOffset;

			// Remember the current and target rotation, we're going to lerp between them
			ClientData->OriginalRotationOffset = OldRotation;
			ClientData->RotationTarget = NewRotation;

			// Note: we don't change rotation, we lerp towards it in SmoothClientPosition.
			if (NewLocation != OldLocation)
			{
				const FScopedPreventAttachedComponentMove PreventMeshMove(VehicleOwner->GetMesh());
				UpdatedComponent->SetWorldLocation(NewLocation, false, nullptr, ETeleportType::ResetPhysics);
			}
		}
		else
		{
			// Calc rotation needed to keep current world rotation after UpdatedComponent moves.
			// Take difference between where we were rotated before, and where we're going
			ClientData->RotationOffset = (NewRotation.Inverse() * OldRotation) * ClientData->RotationOffset;
			ClientData->RotationTarget = FQuat::Identity;

			const FScopedPreventAttachedComponentMove PreventMeshMove(VehicleOwner->GetMesh());
			UpdatedComponent->SetWorldLocationAndRotation(NewLocation, NewRotation, false, nullptr, ETeleportType::ResetPhysics);
		}

		// If running ahead, pull back slightly. This will cause the next delta to seem slightly longer, and cause us to lerp to it slightly slower.
		if (ClientData->ClientTimeStamp > ClientData->ServerTimeStamp)
		{
			const double OldClientTimeStamp = ClientData->ClientTimeStamp;
			ClientData->ClientTimeStamp = FMath::LerpStable(ClientData->ServerTimeStamp, OldClientTimeStamp, 0.5);

			//UE_LOG(LogCharacterNetSmoothing, VeryVerbose, TEXT("SmoothCorrection: Pull back client from ClientTimeStamp: %.6f to %.6f, ServerTimeStamp: %.6f for %s"),OldClientTimeStamp, ClientData->ClientTimeStamp, ClientData->ServerTimeStamp, *GetNameSafe(VehicleOwner));
		}

		// Using server timestamp lets us know how much time actually elapsed, regardless of packet lag variance.
		double OldServerTimeStamp = ClientData->ServerTimeStamp;
		if (bIsSimulatedProxy)
		{
			// This value is normally only updated on the server, however some code paths might try to read it instead of the replicated value so copy it for proxies as well.
			//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			//ServerLastTransformUpdateTimeStamp = VehicleOwner->GetReplicatedServerLastTransformUpdateTimeStamp();
		}
		ClientData->ServerTimeStamp = ServerLastTransformUpdateTimeStamp;

		// Initial update has no delta.
		if (ClientData->LastCorrectionTime == 0)
		{
			ClientData->ClientTimeStamp = ClientData->ServerTimeStamp;
			OldServerTimeStamp = ClientData->ServerTimeStamp;
		}

		// Don't let the client fall too far behind or run ahead of new server time.
		const double ServerDeltaTime = ClientData->ServerTimeStamp - OldServerTimeStamp;
		const double MaxOffset = 0.5f;
		const double MinOffset = FMath::Min(double(0.1f), MaxOffset);

		// MaxDelta is the farthest behind we're allowed to be after receiving a new server time.
		const double MaxDelta = FMath::Clamp(ServerDeltaTime * 1.25, MinOffset, MaxOffset);
		ClientData->ClientTimeStamp = FMath::Clamp(ClientData->ClientTimeStamp, ClientData->ServerTimeStamp - MaxDelta, ClientData->ServerTimeStamp);

		// Compute actual delta between new server timestamp and client simulation.
		ClientData->LastCorrectionDelta = ClientData->ServerTimeStamp - ClientData->ClientTimeStamp;
		ClientData->LastCorrectionTime = MyWorld->GetTimeSeconds();

	}
}

FNetworkPredictionData_Client* UNetPhysVehicleMovementComponent::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UNetPhysVehicleMovementComponent* MutableThis = const_cast<UNetPhysVehicleMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetPhysNetworkPredictionData_Client_Vehicle(*this);
	}

	return ClientPredictionData;
}

FNetworkPredictionData_Server* UNetPhysVehicleMovementComponent::GetPredictionData_Server() const
{
	if (ServerPredictionData == nullptr)
	{
		UNetPhysVehicleMovementComponent* MutableThis = const_cast<UNetPhysVehicleMovementComponent*>(this);
		MutableThis->ServerPredictionData = new FNetPhysNetworkPredictionData_Server_Vehicle(*this);
	}

	return ServerPredictionData;
}

FNetPhysNetworkPredictionData_Client_Vehicle* UNetPhysVehicleMovementComponent::GetPredictionData_Client_Vehicle() const
{
	// Should only be called on client or listen server (for remote clients) in network games
	checkSlow(VehicleOwner != NULL);
	checkSlow(VehicleOwner->GetLocalRole() < ROLE_Authority || (VehicleOwner->GetRemoteRole() == ROLE_AutonomousProxy && GetNetMode() == NM_ListenServer));
	checkSlow(GetNetMode() == NM_Client || GetNetMode() == NM_ListenServer);

	if (ClientPredictionData == nullptr)
	{
		UNetPhysVehicleMovementComponent* MutableThis = const_cast<UNetPhysVehicleMovementComponent*>(this);
		MutableThis->ClientPredictionData = static_cast<class FNetPhysNetworkPredictionData_Client_Vehicle*>(GetPredictionData_Client());
	}

	return ClientPredictionData;
}

FNetPhysNetworkPredictionData_Server_Vehicle* UNetPhysVehicleMovementComponent::GetPredictionData_Server_Vehicle() const
{
	// Should only be called on server in network games
	checkSlow(VehicleOwner != NULL);
	checkSlow(VehicleOwner->GetLocalRole() == ROLE_Authority);
	checkSlow(GetNetMode() < NM_Client);

	if (ServerPredictionData == nullptr)
	{
		UNetPhysVehicleMovementComponent* MutableThis = const_cast<UNetPhysVehicleMovementComponent*>(this);
		MutableThis->ServerPredictionData = static_cast<class FNetPhysNetworkPredictionData_Server_Vehicle*>(GetPredictionData_Server());
	}

	return ServerPredictionData;
}

FNetPhysNetworkPredictionData_Client_Vehicle::FNetPhysNetworkPredictionData_Client_Vehicle(const UNetPhysVehicleMovementComponent& ClientMovement)
	: ClientUpdateTime(0.f),
	CurrentTimeStamp(0.f),
	MaxFreeMoveCount(96),
	MaxSavedMoveCount(96),
	bUpdatePosition(false),
	OriginalLocationOffset(ForceInitToZero),
	LocationOffset(ForceInitToZero),
	OriginalRotationOffset(ForceInitToZero),
	RotationOffset(ForceInitToZero),
	RotationTarget(ForceInitToZero),
	LastCorrectionTime(0.f),
	LastCorrectionDelta(0.f),
	ClientTimeStamp(0.f),
	ServerTimeStamp(0.f),
	SmoothNetUpdateTime(0.f),
	SmoothNetUpdateRotationTime(0.f),
	MaxMoveDeltaTime(0.125f)
{
	const bool bIsListenServer = (ClientMovement.GetNetMode() == NM_ListenServer);
	SmoothNetUpdateTime = (bIsListenServer ? ClientMovement.ListenServerNetworkSimulatedSmoothLocationTime : ClientMovement.NetworkSimulatedSmoothLocationTime);
	SmoothNetUpdateRotationTime = (bIsListenServer ? ClientMovement.ListenServerNetworkSimulatedSmoothRotationTime : ClientMovement.NetworkSimulatedSmoothRotationTime);

	
	// Copy the game network manager max move delta value
	const AGameNetworkManager* GameNetworkManager = (const AGameNetworkManager*)(AGameNetworkManager::StaticClass()->GetDefaultObject());
	if (GameNetworkManager)
	{
		MaxMoveDeltaTime = GameNetworkManager->MaxMoveDeltaTime;
		//MaxClientSmoothingDeltaTime = FMath::Max(GameNetworkManager->MaxClientSmoothingDeltaTime, MaxMoveDeltaTime * 2.0f);
	}

	if (ClientMovement.GetOwnerRole() == ROLE_AutonomousProxy)
	{
		SavedMoves.Reserve(MaxSavedMoveCount);
		FreeMoves.Reserve(MaxFreeMoveCount);
	}
}

FNetPhysNetworkPredictionData_Client_Vehicle::~FNetPhysNetworkPredictionData_Client_Vehicle()
{
	SavedMoves.Empty();
	FreeMoves.Empty();
	PendingMove = NULL;
	LastAckedMove = NULL;
}

int32 FNetPhysNetworkPredictionData_Client_Vehicle::GetSavedMoveIndex(float TimeStamp) const
{
	if (SavedMoves.Num() > 0)
	{
		// If LastAckedMove isn't using an old TimeStamp (before reset), we can prevent the iteration if incoming TimeStamp is outdated
		if (LastAckedMove.IsValid() && !LastAckedMove->bOldTimeStampBeforeReset && (TimeStamp <= LastAckedMove->TimeStamp))
		{
			return INDEX_NONE;
		}

		// Otherwise see if we can find this move.
		for (int32 Index = 0; Index < SavedMoves.Num(); Index++)
		{
			const FSavedMove_Vehicle* CurrentMove = SavedMoves[Index].Get();
			checkSlow(CurrentMove != nullptr);
			if (CurrentMove->TimeStamp == TimeStamp)
			{
				return Index;
			}
		}
	}
	return INDEX_NONE;
}

FNetPhysNetworkPredictionData_Server_Vehicle::FNetPhysNetworkPredictionData_Server_Vehicle(const UNetPhysVehicleMovementComponent& ServerMovement)
	: CurrentClientTimeStamp(0.f)
	, ServerAccumulatedClientTimeStamp(0.0)
	, LastUpdateTime(0.f)
	, ServerTimeStampLastServerMove(0.f)
	, MaxMoveDeltaTime(0.125f)
	, bForceClientUpdate(false)
	, LifetimeRawTimeDiscrepancy(0.f)
	, TimeDiscrepancy(0.f)
	, bResolvingTimeDiscrepancy(false)
	, TimeDiscrepancyResolutionMoveDeltaOverride(0.f)
	, TimeDiscrepancyAccumulatedClientDeltasSinceLastServerTick(0.f)
	, WorldCreationTime(0.f)

{
	const AGameNetworkManager* GameNetworkManager = (const AGameNetworkManager*)(AGameNetworkManager::StaticClass()->GetDefaultObject());
	if (GameNetworkManager)
	{
		MaxMoveDeltaTime = GameNetworkManager->MaxMoveDeltaTime;
		if (GameNetworkManager->MaxMoveDeltaTime > GameNetworkManager->MAXCLIENTUPDATEINTERVAL)
		{
			UE_LOG(LogNetPlayerMovement, Warning, TEXT("GameNetworkManager::MaxMoveDeltaTime (%f) is greater than GameNetworkManager::MAXCLIENTUPDATEINTERVAL (%f)! Server will interfere with move deltas that large!"), GameNetworkManager->MaxMoveDeltaTime, GameNetworkManager->MAXCLIENTUPDATEINTERVAL);
		}
	}

	const UWorld* World = ServerMovement.GetWorld();
	if (World)
	{
		WorldCreationTime = World->GetTimeSeconds();
		ServerTimeStamp = World->GetTimeSeconds();
	}
}

FNetPhysNetworkPredictionData_Server_Vehicle::~FNetPhysNetworkPredictionData_Server_Vehicle()
{
}

void UNetPhysVehicleMovementComponent::PerformMovement(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_VehicleMovementPerformMovement);

	const UWorld* MyWorld = GetWorld();
	if (!HasValidData() || MyWorld== nullptr)
	{
		return;
	}

	const bool bHasAuthority = VehicleOwner && VehicleOwner->HasAuthority();

	// If we move we want to avoid a long delay before Treplication catches up to notice this change, especially if it's throttling our rate. 
	if (bHasAuthority & UNetDriver::IsAdaptiveNetUpdateFrequencyEnabled() && UpdatedComponent)
	{
		UNetDriver* NetDriver=MyWorld->GetNetDriver();
		if (NetDriver && NetDriver->IsServer())
		{
			FNetworkObjectInfo* NetActor=NetDriver->FindOrAddNetworkObjectInfo(VehicleOwner);

			if (NetActor && MyWorld->GetTimeSeconds() <= NetActor->NextUpdateTime && NetDriver->IsNetworkActorUpdateFrequencyThrottled(*NetActor))
			{
				const bool bLocationChanged = (UpdatedComponent->GetComponentLocation() != LastUpdateLocation);
				const bool bRotationChanged = (UpdatedComponent->GetComponentQuat() != LastUpdateRotation);
				if (bLocationChanged || bRotationChanged)
				{
					NetDriver->CancelAdaptiveReplication(*NetActor);
				}
			}
		}
	}

	const FVector NewLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
	const FQuat NewRotation = UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;


	if (bHasAuthority && UpdatedComponent && !IsNetMode(NM_Client))
	{
		const bool bLocationChanged = (NewLocation != LastUpdateLocation);
		const bool bRotationChanged = (NewRotation != LastUpdateRotation);
		// If the transform updated after the movement, update transform timestamp for client correction and smoothing 
		if (bLocationChanged || bRotationChanged)
		{
			const bool bIsRemotePlayer = (VehicleOwner->GetRemoteRole() == ROLE_AutonomousProxy);
			const FNetPhysNetworkPredictionData_Server_Vehicle* ServerData = bIsRemotePlayer ? GetPredictionData_Server_Vehicle() : nullptr;

			if (bIsRemotePlayer && ServerData && VehicleMovementCVars::NetUseClientTimestampForReplicatedTransform)
			{
				ServerLastTransformUpdateTimeStamp = float(ServerData->ServerAccumulatedClientTimeStamp);
			}
			else
			{
				ServerLastTransformUpdateTimeStamp = MyWorld->GetTimeSeconds();
			}
		}
	}

	LastUpdateLocation = NewLocation;
	LastUpdateRotation = NewRotation;
}

void UNetPhysVehicleMovementComponent::CallServerMove(const class FSavedMove_Vehicle* NewMove, const class FSavedMove_Vehicle* OldMove)
{
	check(NewMove != nullptr);

	// Compress the yaw and pitch down to 5 bytes 
	const uint32 ClientYawPitchInt = PackYawAndPitchTo32(NewMove->SavedControlRotation.Yaw, NewMove->SavedControlRotation.Pitch);
	const uint8 ClientRollByte = FRotator::CompressAxisToByte(NewMove->SavedControlRotation.Roll);

	// Send an old move it if exists 
	if (OldMove != nullptr)
	{
		ServerMoveOld(OldMove->TimeStamp, OldMove->GetCompressedFlags());
	}

	FNetPhysNetworkPredictionData_Client_Vehicle* ClientData = GetPredictionData_Client_Vehicle();
	check(ClientData != nullptr);

	// If we have a pending move, send two moves at the same time 
	// Custom
	if (const FSavedMove_Vehicle* const PendingMove = ClientData->PendingMove.Get())
	{
		const uint32 OldClientYawPitch32 = PackYawAndPitchTo32(PendingMove->SavedControlRotation.Yaw, PendingMove->SavedControlRotation.Pitch);
		ServerMoveDual(
			PendingMove->TimeStamp,
			PendingMove->GetCompressedFlags(),
			OldClientYawPitch32,
			NewMove->TimeStamp,
			NewMove->SavedLocation,
			NewMove->GetCompressedFlags(),
			ClientRollByte,
			ClientYawPitchInt
		);
	}
	else {
		ServerMove(
			NewMove->TimeStamp,
			NewMove->SavedLocation,
			NewMove->GetCompressedFlags(),
			ClientRollByte,
			ClientYawPitchInt
		);
	}
	MarkForClientCameraUpdate();
}

void UNetPhysVehicleMovementComponent::SmoothClientPosition(float DeltaSeconds)
{
	if (!HasValidData() || NetworkSmoothingMode == ENetPhysSmoothingMode::Disabled)
	{
		return;
	}

	// We should not be running this on a server that is not a listen server 
	checkSlow(GetNetMode() != NM_DedicatedServer);
	checkSlow(GetNetMode() != NM_Standalone);

	// Only client proxies or remote clients on a listen server should run this code 
	const bool bSimulatedProxy = (VehicleOwner->GetLocalRole() == ROLE_SimulatedProxy);
	const bool bRemoteAutoProxy = (VehicleOwner->GetRemoteRole() == ROLE_AutonomousProxy);

	if (!ensure(bSimulatedProxy || bRemoteAutoProxy))
	{
		return;
	}

	SmoothClientPosition_Interpolate (DeltaSeconds);
	SmoothClientPosition_UpdateVisuals(DeltaSeconds);
}

void UNetPhysVehicleMovementComponent::SmoothClientPosition_Interpolate(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_VehicleMovementSmoothClientPosition_Interp);

	FNetPhysNetworkPredictionData_Client_Vehicle* ClientData = GetPredictionData_Client_Vehicle();
	if (ClientData) {
		if (NetworkSmoothingMode == ENetPhysSmoothingMode::Linear)
		{
			//Increment client position.
			ClientData->ClientTimeStamp += DeltaSeconds;

			float LerpPercent = 0.f;
			const float LerpLimit = 1.15f;
			const float TargetDelta = ClientData->LastCorrectionDelta;
			if (TargetDelta > SMALL_NUMBER)
			{
				// Don't let the client get too far ahead (happens on spikes). But we do want a buffer for variable network conditions. 
				const float MaxClientTimeAheadPercent = 0.25f;
				const float MaxTimeAhead = TargetDelta * MaxClientTimeAheadPercent;
				ClientData->ClientTimeStamp = FMath::Min<float>(ClientData->ClientTimeStamp, ClientData->ServerTimeStamp + MaxTimeAhead);

				UE_LOG(LogTemp, Warning, TEXT("%.6f"), ClientData->ClientTimeStamp);

				// Compute interpolation alpha based on our client position within the server delta. We should take TargetDelta seconds to reach alpha of 1. 
				const float RemainingTime = ClientData->ServerTimeStamp - ClientData->ClientTimeStamp;
				const float CurrentSmoothTime = TargetDelta - RemainingTime;
				LerpPercent = FMath::Clamp(CurrentSmoothTime / TargetDelta, 0.0f, LerpLimit);
			}
			else
			{
				LerpPercent = 1.0f;
			}

			if (LerpPercent >= 1.0f - KINDA_SMALL_NUMBER)
			{
				if (VehicleOwner->ReplicatedMovement.LinearVelocity.IsNearlyZero())
				{
					ClientData->LocationOffset = FVector::ZeroVector;
					ClientData->ClientTimeStamp = ClientData->ServerTimeStamp;
					bNetworkSmoothingComplete = true;

					UE_LOG(LogTemp, Warning, TEXT("%.6f"), ClientData->ClientTimeStamp);
				}
				else
				{
					// Allow limited forward prediction. 
					ClientData->LocationOffset = FMath::LerpStable(ClientData->OriginalLocationOffset, FVector::ZeroVector, LerpPercent);
					bNetworkSmoothingComplete = (LerpPercent >= LerpLimit);
				}
				ClientData->RotationOffset = ClientData->RotationTarget;
			}
			else
			{
				ClientData->LocationOffset = FMath::LerpStable(ClientData->OriginalLocationOffset, FVector::ZeroVector, LerpPercent);
				ClientData->RotationOffset = FQuat::FastLerp(ClientData->OriginalRotationOffset, ClientData->RotationTarget, LerpPercent).GetNormalized();
			}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (VehicleMovementCVars::NetVisualizeSimulatedCorrections >= 1)
			{
				const FColor DebugColor = FColor::White;
				const FVector DebugLocation = UpdatedComponent->GetComponentLocation() + FVector(0.f, 0.f, 150.f);
				FString DebugText = FString::Printf(TEXT("Lerp: %2.2f"), LerpPercent);
				DrawDebugString(GetWorld(), DebugLocation, DebugText, nullptr, DebugColor, 0.f, true);
			}

#endif
		}
		else if (NetworkSmoothingMode == ENetPhysSmoothingMode::Exponential)
		{
			// Smooth interpolaton of translation to avoid popping of other client pawns unless unser a low tick rate. 
			// Faster interpolation if stopped
			//const float SmoothLocationTime = VehicleOwner->ReplicatedMovement.LinearVelocity.IsZero() ? 0.5f * NetworkSmoothLocationTime : NetworkSmoothLocationTime;

			const float SmoothLocationTime = VehicleOwner->ReplicatedMovement.LinearVelocity.IsZero() ? 0.5f * ClientData->SmoothNetUpdateTime : ClientData->SmoothNetUpdateTime;
			UE_LOG(LogTemp, Warning, TEXT("%.6f , %.6f, %.6f"), DeltaSeconds, SmoothLocationTime, ClientData->SmoothNetUpdateTime);

			if (DeltaSeconds < SmoothLocationTime)
			{
				// Slowly decay translation offset 
				ClientData->LocationOffset = (ClientData->LocationOffset * (1.f - DeltaSeconds / SmoothLocationTime));
			}
			else
			{
				ClientData->LocationOffset = FVector::ZeroVector;
			}

			// Smooth the rotation 
			const FQuat RotationTarget = ClientData->RotationTarget;
			if (DeltaSeconds < NetworkSmoothRotationTime)
			{
				// Slowly decay rotation offset 
				ClientData->RotationOffset = FQuat::FastLerp(ClientData->RotationOffset, RotationTarget, DeltaSeconds /
					NetworkSmoothRotationTime).GetNormalized();
			}
			else
			{
				ClientData->RotationOffset = RotationTarget;
			}

			// Check if the lerp is complete 
			if (ClientData->LocationOffset.IsNearlyZero(1e-2f) && ClientData->RotationOffset.Equals(RotationTarget, 1e-5f))
			{
				bNetworkSmoothingComplete = true;
				ClientData->LocationOffset = FVector::ZeroVector;
				ClientData->RotationOffset = RotationTarget;
			}
		}

#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (VehicleMovementCVars::NetVisualizeSimulatedCorrections >= 1)
		{
			const FVector Debuglocation = UpdatedComponent->GetComponentLocation();
			DrawDebugBox(GetWorld(), Debug Location, FVector(45, 45, 45), Updated Component->GetComponentQuat(), FColor(0, 255, 0));
		}
#endif
	}
}

void UNetPhysVehicleMovementComponent::SmoothClientPosition_UpdateVisuals(float DeltaSeconds)
{
	//Incomplete - because of ETeleportType - need to resetPhysics

	FNetPhysNetworkPredictionData_Client_Vehicle* ClientData = GetPredictionData_Client_Vehicle();
	USkeletalMeshComponent* MeshComp = VehicleOwner->GetMesh();

	if (ClientData != nullptr && MeshComp != nullptr)
	{
		const FVector NewLocation = MeshComp->GetComponentLocation() + ClientData->LocationOffset;
		const FQuat NewRotation = ClientData->RotationOffset;
		MeshComp->SetWorldLocation(NewLocation, false, nullptr, ETeleportType::ResetPhysics);
		MeshComp->SetWorldRotation(NewRotation, false, nullptr, ETeleportType::ResetPhysics);

	}
}

void UNetPhysVehicleMovementComponent::ReplicateMoveToServer(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_VehicleMovementReplicateMoveToServer);
	check(VehicleOwner != nullptr);

	// Can only start sending moves if our controllers are synced up over the network, otherwise we flood the reliable buffer. 
	APlayerController* PC = Cast<APlayerController>(VehicleOwner->GetController());
	if (PC && PC->AcknowledgedPawn != VehicleOwner)
	{
		return;
	}

	// Bail out if our vehicle's controller doesn't have a Player. This may be the case when the local player 
	// has switched to another controller, such as a debug camera controller. 
	if (PC && PC->Player == nullptr)
	{
		return;
	}

	FNetPhysNetworkPredictionData_Client_Vehicle* ClientData = GetPredictionData_Client_Vehicle();
	if (!ClientData)
	{
		return;
	}
	// Update our delta time for physics simulation. 
	DeltaSeconds = ClientData->UpdateTimeStampAndDeltaTime(DeltaSeconds, *VehicleOwner, *this);

	// Find the oldest (unacknowledged) important move (Old Move).
	// Don't include the last move because it may be combined with the next new move. 
	FVehicleSavedMovePtr OldMove = NULL;
	if (ClientData->LastAckedMove.IsValid())
	{
		const int32 NumSavedMoves = ClientData->SavedMoves.Num();
		for (int32 Idx = 0; Idx < NumSavedMoves - 1; Idx++)
		{
			const FVehicleSavedMovePtr& CurrentMove = ClientData->SavedMoves[Idx];
			if (CurrentMove->IsImportantMove(ClientData->LastAckedMove))
			{
				OldMove = CurrentMove;
				break;
			}
		}
	}

	// Get a Saved Move object to store the movement in 
	FVehicleSavedMovePtr NewMovePtr = ClientData->CreateSavedMove();
	FSavedMove_Vehicle* const NewMove = NewMovePtr.Get();
	if (NewMove == nullptr)
	{
		return;
	}

	// Initialize the start of the move
	NewMove->SetMoveFor(VehicleOwner, DeltaSeconds, *ClientData);
	//CUSTOM - NOT IN ORIGINAL FOR PENDING MOVE
	const UWorld* MyWorld = GetWorld();

	// Check if two moves can be combined if theres a pending move 
	if (const FSavedMove_Vehicle* PendingMove = ClientData->PendingMove.Get())
	{
		if (!PendingMove->bOldTimeStampBeforeReset && PendingMove->CanCombineWith(NewMovePtr, VehicleOwner, ClientData->MaxMoveDeltaTime * VehicleOwner->GetActorTimeDilation(*MyWorld)))
		{
				// Check to make sure we are not colliding with anything when moving back 
				if (!OverlapTest(PendingMove->StartLocation, PendingMove->StartRotation.Quaternion(), UpdatedComponent->GetCollisionObjectType(), VehicleOwner->GetPawnCollisionShape(), VehicleOwner))
				{
					// Accumulate transform updates till scope ends 
					FScopedMovementUpdate ScopedMovementUpdate(UpdatedComponent, EScopedUpdate::DeferredUpdates);
					NewMove->CombineWith(PendingMove, VehicleOwner, PC, PendingMove->StartLocation);

					if (PC != nullptr)
					{
						VehicleOwner->FaceRotation(PC->GetControlRotation(), NewMove->DeltaTime);
					}

					NewMove->SetInitialPosition(VehicleOwner);

					// Remove pending move from move list 
					if (ClientData->SavedMoves.Num() > 0 && ClientData->SavedMoves.Last() == ClientData->PendingMove)
					{
						ClientData->SavedMoves.Pop(false);
					}

					ClientData->FreeMove(ClientData->PendingMove);
					ClientData->PendingMove = nullptr;
					PendingMove = nullptr;
				}
		}
		else
		{
			//UE_LOG(LogVehiclelet, Log, TEXT("Not combining move [would collide at the start location]"));
		}
	}

	// Perform movement on the client 
	PerformMovement(NewMove->DeltaTime);

	// Set the final move data after PerformMovement call since it may move the pawn 
	NewMove->PostUpdate(VehicleOwner);

	// Add the new move to the list 
	if (VehicleOwner->IsReplicatingMovement())
	{
		check(NewMove == NewMovePtr.Get());
		ClientData->SavedMoves.Push(NewMovePtr);

		const bool bCanDelayMove = true;
		// TODO: implement CanDelay SendingMove) 
		if (bCanDelayMove && !ClientData->PendingMove.IsValid())
		{
			// Decide if we should delay the move 
			const float NetMoveDelta = FMath::Clamp(GetClientNetSendDeltaTime(PC, ClientData, NewMovePtr), 1.f / 120.f, 1.f / 5.f);
			if ((MyWorld->TimeSeconds - ClientData->ClientUpdateTime) * MyWorld->GetWorldSettings()->GetEffectiveTimeDilation() < NetMoveDelta)
			{
				// Delay sending this move by placing it in the pending move 
				ClientData->PendingMove = NewMovePtr;
				return;
			}
		}

		ClientData->ClientUpdateTime = MyWorld->TimeSeconds;
		CallServerMove(NewMove, OldMove.Get());
	}

	ClientData->PendingMove = NULL;
}

FVehicleSavedMovePtr FNetPhysNetworkPredictionData_Client_Vehicle::CreateSavedMove()
{
	if (SavedMoves.Num() >= MaxSavedMoveCount)
	{
		UE_LOG(LogNetPlayerMovement, Warning, TEXT("CreateSavedMove: Hit limit of %d saved moves (timing out or very bad ping?)"), SavedMoves.Num());
		// Free all saved moves
		for (int32 i = 0; i < SavedMoves.Num(); i++)
		{
			FreeMove(SavedMoves[i]);
		}
		SavedMoves.Reset();
	}

	if (FreeMoves.Num() == 0)
	{
		// No free moves, allocate a new one.
		FVehicleSavedMovePtr NewMove = AllocateNewMove();
		checkSlow(NewMove.IsValid());
		NewMove->Clear();
		return NewMove;
	}
	else
	{
		// Pull from the free pool
		const bool bAllowShrinking = false;
		FVehicleSavedMovePtr FirstFree = FreeMoves.Pop(bAllowShrinking);
		FirstFree->Clear();
		return FirstFree;
	}
}

FVehicleSavedMovePtr FNetPhysNetworkPredictionData_Client_Vehicle::AllocateNewMove()
{
	return FVehicleSavedMovePtr(new FSavedMove_Vehicle());
}

void UNetPhysVehicleMovementComponent::ServerMove(float TimeStamp, FVector_NetQuantize100 Location, uint8 Flags, uint8 Roll, uint32 View)
{
	VehicleOwner->ServerMove(TimeStamp, Location, Flags, Roll, View);
}

bool UNetPhysVehicleMovementComponent::ServerMove_Validate(float TimeStamp, FVector_NetQuantize100 Location, uint8 Flags, uint8 Roll, uint32 View)
{
	return true;
}

void UNetPhysVehicleMovementComponent::ServerMove_Implementation(float TimeStamp, FVector_NetQuantize100 Location, uint8 Flags, uint8 Roll, uint32 View)
{
	if (!HasValidData() || !IsActive())
	{
		return;
	}
	FNetPhysNetworkPredictionData_Server_Vehicle* ServerData = GetPredictionData_Server_Vehicle();
	check(ServerData != nullptr);

	if (!VerifyClientTimeStamp(TimeStamp, *ServerData))
	{
		//UE_LOG(LogVehicleNet, Log, TEXT("ServerMove: TimeStamp expired. %f, Current TimeStamp: %f"), TimeStamp, ServerData->CurrentClientTimeStamp);
		return;
	}
	bool bServerReadyForClient = true;
	APlayerController* PC = Cast <APlayerController>(VehicleOwner->GetController());
	if (PC == nullptr)
	{
		bServerReadyForClient = PC->NotifyServerReceivedClientData(VehicleOwner, TimeStamp);
	}

	//View components
	const uint16 ViewPitch = (View & 65535);
	const uint16 ViewYaw = (View >> 16);

	//Save move parameters
	const UWorld* MyWorld = GetWorld();

	const float DeltaTime = ServerData->GetServerMoveDeltaTime(TimeStamp, VehicleOwner->GetActorTimeDilation(*MyWorld));

	ServerData->CurrentClientTimeStamp = TimeStamp;
	ServerData->ServerAccumulatedClientTimeStamp += DeltaTime;
	ServerData->ServerTimeStamp = MyWorld->GetTimeSeconds();
	ServerData->ServerTimeStampLastServerMove = ServerData->ServerTimeStamp;
	FRotator ViewRot;
	ViewRot.Pitch = FRotator::DecompressAxisFromShort(ViewPitch);
	ViewRot.Yaw = FRotator::DecompressAxisFromShort(ViewYaw);
	ViewRot.Roll = FRotator::DecompressAxisFromByte(Roll);

	if (PC != nullptr)
	{
		PC->SetControlRotation(ViewRot);
	}

	if (!bServerReadyForClient)
	{
		return;
	}
	// Perform actual movement 
	if ((MyWorld->GetWorldSettings()->Pauser == NULL) && (DeltaTime > 0.f))
	{
		if (PC)
		{
			PC->UpdateRotation(DeltaTime);
		}
		ServerMoveHandleClientError(TimeStamp, DeltaTime, Location);
	}
}

void UNetPhysVehicleMovementComponent::ServerMoveHandleClientError(float ClientTimeStamp, float DeltaTime, const FVector& Location) 
{
	if (Location == FVector(1.f, 2.f, 3.f)) // first part of double servermove
	{
		return;
	}
	FNetPhysNetworkPredictionData_Server_Vehicle* ServerData = GetPredictionData_Server_Vehicle(); 
	check(ServerData != nullptr);

	// Don't prevent more recent updates from being sent if received this frame. 
	// We're going to send out an update anyway, might as well be the most recent one. 
	APlayerController* PC = Cast<APlayerController>(VehicleOwner->GetController()); 
	if ((ServerData->LastUpdateTime != GetWorld()->TimeSeconds)) {
		const AGameNetworkManager* GameNetworkManager = (const AGameNetworkManager*)(AGameNetworkManager::StaticClass()->GetDefaultObject());
		if (GameNetworkManager->WithinUpdateDelayBounds(PC, ServerData->LastUpdateTime))
		{
			return;
		}
	}
	
	const FVector ClientLoc = FRepMovement::RebaseOntoZeroOrigin(Location, this);

	// Compute the client error from the server's position 
	// If client has accumulated a noticeable positional error, correct them. 
	bNetworkLargeClientCorrection = ServerData->bForceClientUpdate; 
	if (ServerData->bForceClientUpdate || ServerCheckClientError(ClientTimeStamp, DeltaTime, ClientLoc, Location)) {
		ServerData->PendingAdjustment.NewLinear = UpdatedPrimitive->GetPhysicsLinearVelocity();
		ServerData->PendingAdjustment.NewAngular = UpdatedPrimitive->GetPhysicsAngularVelocityInDegrees();
		ServerData->PendingAdjustment.NewLoc - FRepMovement::RebaseOntoZeroOrigin(UpdatedComponent->GetComponentLocation(), this);
		ServerData->PendingAdjustment.NewRot - UpdatedComponent->GetComponentRotation();

#if !UE_BUILD_SHIPPING
		if (VehicleMovementCVars::NetShowCorrections != 0)
		{
			const FVector LocDiff = UpdatedPrimitive->GetComponentLocation() - ClientLoc;
			//UE_LOG(LogVehicleNet, Warning, TEXT("*** Server: Error for Xs at Time-%.3f is %3.3f ClientLoc(s) ServerLoc(s)"), *VehicleOwner->GetName(),ClientTimeStamp, LocDiff.Size(), * ClientLoc.ToString(), * UpdatedPrimitive->GetComponentLocation().ToString(); 
			const float DebugLifetime = VehicleMovementCVars::NetCorrectionLifetime;
			DrawDebugSphere(GetWorld(), UpdatedComponent->GetComponentLocation(), 50.F, 16.f, FColor(100, 255, 102), true, DebugLifetime, 0, 1.5f);
			DrawDebugSphere(GetWorld(), ClientLoc, 50.f, 16.f, FColor(255, 100, 180), true, DebugLifetime, 0, 1.5f);
		}
#endif

		ServerData->LastUpdateTime = GetWorld()->TimeSeconds;
		ServerData->PendingAdjustment.DeltaTime = DeltaTime; 
		ServerData->PendingAdjustment.TimeStamp = ClientTimeStamp; 
		ServerData->PendingAdjustment.bAckGoodMove = false;
	}

	else
	{
		// TODO: implement client auth movement
		ServerData->PendingAdjustment.TimeStamp = ClientTimeStamp; 
		ServerData->PendingAdjustment.bAckGoodMove = true;
	}
	ServerData->bForceClientUpdate = false;
}

bool UNetPhysVehicleMovementComponent::ServerCheckClientError(float ClientTimeStamp, float DeltaTime, const FVector& ClientWorldLocation, const FVector& Location)
{
	// Check location difference against global setting
if (!bIgnoreClientMovementErrorChecksAndCorrection)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (VehicleMovementCVars::NetForceClientAdjustmentPercent > SMALL_NUMBER)
	{
		UE_LOG(LogNetPlayerMovement, VeryVerbose, TEXT("** ServerCheckClientError forced by p.NetForceClientAdjustmentPercent"));
		return true;
	}
#endif
}
else
{
#if !UE_BUILD_SHIPPING
	if (VehicleMovementCVars::NetShowCorrections != 0)
	{
		//UE_LOG(LogNetPlayerMovement, Warning, TEXT("*** Server: %s is set to ignore error checks and corrections."), *GetNameSafe(CharacterOwner));
	}
#endif // !UE_BUILD_SHIPPING
}
return false;
}

void UNetPhysVehicleMovementComponent::ServerMoveOld(float OldTimeStamp, uint8 OldFlags) 
{
	VehicleOwner->ServerMoveOld(OldTimeStamp, OldFlags);
};

void UNetPhysVehicleMovementComponent::ServerMoveOld_Implementation(float OldTimeStamp, uint8 OldFlags) 
{
	if (!HasValidData() || !IsActive())
	{
		return;
	}

	FNetPhysNetworkPredictionData_Server_Vehicle* ServerData = GetPredictionData_Server_Vehicle();
	check(ServerData);

	if (!VerifyClientTimeStamp(OldTimeStamp, *ServerData))
	{
		//UE_LOG(LogNetPlayerMovement, VeryVerbose, TEXT("ServerMoveOld: TimeStamp expired. %f, CurrentTimeStamp: %f, Character: %s"), OldTimeStamp, ServerData->CurrentClientTimeStamp, *GetNameSafe(CharacterOwner));
		return;
	}

	UE_LOG(LogNetPlayerMovement, Verbose, TEXT("Recovered move from OldTimeStamp %f, DeltaTime: %f"), OldTimeStamp, OldTimeStamp - ServerData->CurrentClientTimeStamp);

	const UWorld* MyWorld = GetWorld();
	const float DeltaTime = ServerData->GetServerMoveDeltaTime(OldTimeStamp, VehicleOwner->GetActorTimeDilation(*MyWorld));
	if (DeltaTime > 0.f)
	{
		ServerData->CurrentClientTimeStamp = OldTimeStamp;
		ServerData->ServerAccumulatedClientTimeStamp += DeltaTime;
		ServerData->ServerTimeStamp = MyWorld->GetTimeSeconds();
		ServerData->ServerTimeStampLastServerMove = ServerData->ServerTimeStamp;

		MoveAutonomous(OldTimeStamp, DeltaTime, OldFlags);
		//HandoverClientTimeStamp = ServerData->CurrentClientTimeStamp;  // IMPROBABLE-CHANGE Try to replicate latest client timestamp
	}
	else
	{
		UE_LOG(LogNetPlayerMovement, Warning, TEXT("OldTimeStamp(%f) results in zero or negative actual DeltaTime(%f). Theoretical DeltaTime(%f)"),
			OldTimeStamp, DeltaTime, OldTimeStamp - ServerData->CurrentClientTimeStamp);
	}
};

bool UNetPhysVehicleMovementComponent::ServerMoveOld_Validate(float oldTimeStamp, uint8 OldFlags) 
{ 
	return true; 
};

void UNetPhysVehicleMovementComponent::ClientAckGoodMove(float TimeStamp)
{
	VehicleOwner->ClientAckGoodMove(TimeStamp);
}

void UNetPhysVehicleMovementComponent::ClientAckGoodMove_Implementation(float TimeStamp)
{
	if (!HasValidData() || !IsActive())
	{
		return;
	}

	FNetPhysNetworkPredictionData_Client_Vehicle* ClientData = GetPredictionData_Client_Vehicle();
	check(ClientData);

	// Ack move if it has not expired.
	int32 MoveIndex = ClientData->GetSavedMoveIndex(TimeStamp);
	if (MoveIndex == INDEX_NONE)
	{
		if (ClientData->LastAckedMove.IsValid())
		{
			UE_LOG(LogNetPlayerMovement, Log, TEXT("ClientAckGoodMove_Implementation could not find Move for TimeStamp: %f, LastAckedTimeStamp: %f, CurrentTimeStamp: %f"), TimeStamp, ClientData->LastAckedMove->TimeStamp, ClientData->CurrentTimeStamp);
		}
		return;
	}

	ClientData->AckMove(MoveIndex);
}

void UNetPhysVehicleMovementComponent::ServerMoveDual(float TimeStamp0, uint8 PendingFlags, uint32 View0, float TimeStamp, FVector_NetQuantize100 Location, uint8 NewFlags, uint8 Roll, uint32 View)
{
		VehicleOwner->ServerMoveDual(TimeStamp0, PendingFlags, View0, TimeStamp, Location, NewFlags, Roll, View);
}

void UNetPhysVehicleMovementComponent::ServerMoveDual_Implementation(float TimeStamp0, uint8 PendingFlags, uint32 View0, float TimeStamp, FVector_NetQuantize100 Location, uint8 NewFlags, uint8 Roll, uint32 View)
{
	ServerMove_Implementation(TimeStamp0, FVector(1.f, 2.f, 3.f), PendingFlags, Roll, View0);
	ServerMove_Implementation(TimeStamp, Location, NewFlags, Roll, View);
}

bool UNetPhysVehicleMovementComponent::ServerMoveDual_Validate(float TimeStamp0, uint8 PendingFlags, uint32 View0, float TimeStamp, FVector_NetQuantize100 Location, uint8 NewFlags, uint8 Roll, uint32 View)
{
	return true;
}

void UNetPhysVehicleMovementComponent::SendClientAdjustment()
{
	if (!HasValidData())
	{
		return;
	}

	FNetPhysNetworkPredictionData_Server_Vehicle* ServerData = GetPredictionData_Server_Vehicle();
	check(ServerData != nullptr);

	if (ServerData->PendingAdjustment.TimeStamp <= 0.f)
	{
		return;
	}
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	if (ServerData->PendingAdjustment.bAckGoodMove)
	{
		// just notify client this move was received 
		if (CurrentTime - ServerLastClientGoodMoveAckTime > NetworkMinTimeBetweenClientAckGoodMoves) {
			ServerLastClientGoodMoveAckTime = CurrentTime;
			ClientAckGoodMove(ServerData->PendingAdjustment.TimeStamp);

		}
	}
	else
	{
		// We won't be back in here until the next client move and potential correction is received, so use the correct time now. 
		// Protect against bad data by taking appropriate min/max of editable values. 
		const float AdjustmentTimeThreshold = bNetworkLargeClientCorrection ?
			FMath::Min(NetworkMinTimeBetweenClientAdjustmentsLargeCorrection, NetworkMinTimeBetweenClientAdjustments) :
			FMath::Max(NetworkMinTimeBetweenClientAdjustmentsLargeCorrection, NetworkMinTimeBetweenClientAdjustments);

		// Check if correction is throttled based on time limit between updates. 
		if (CurrentTime - ServerLastClientAdjustmentTime > AdjustmentTimeThreshold)
		{
			ServerLastClientAdjustmentTime = CurrentTime;
			if (ServerData->PendingAdjustment.NewLinear.IsZero()) {
				ClientVeryShortAdjustPosition(
					ServerData->PendingAdjustment.TimeStamp,
					ServerData->PendingAdjustment.NewLoc
				);
			}
			else
			{
				ClientAdjustPosition(
					ServerData->PendingAdjustment.TimeStamp,
					ServerData->PendingAdjustment.NewLoc,
					ServerData->PendingAdjustment.NewLinear,
					ServerData->PendingAdjustment.NewAngular
				);
			}
		}
	}
	ServerData->PendingAdjustment.TimeStamp = 0;
	ServerData->PendingAdjustment.bAckGoodMove = false;
	ServerData->bForceClientUpdate = false;
}

void UNetPhysVehicleMovementComponent::ClientAdjustPosition(float TimeStamp, FVector NewLoc, FVector NewLinear, FVector NewAngular)
{
	VehicleOwner->ClientAdjustPosition(TimeStamp, NewLoc, NewLinear, NewAngular);
}

void UNetPhysVehicleMovementComponent::ClientAdjustPosition_Implementation(float TimeStamp, FVector NewLoc, FVector NewLinear, FVector NewAngular)
{
	if (!HasValidData() || !IsActive())
	{
		return;
	}

	FNetPhysNetworkPredictionData_Client_Vehicle* ClientData = GetPredictionData_Client_Vehicle();
	check(ClientData != nullptr);

	// Ack move if it has not expired 
	int32 MoveIndex = ClientData->GetSavedMoveIndex(TimeStamp);
	if (MoveIndex == INDEX_NONE)
	{
		if (ClientData->LastAckedMove.IsValid())
		{
			//UE_LOG(LogVehicleNet, Log, TEXT("ClientAdjustPosition_Implementation could not find Move for TimeStamp: %f, LastAckedTimeStamp: %,Current TimeStamp : % f"), TimeStamp, ClientData->LastAckedMove->TimeStamp, Client Data->Current TimeStamp);
		}
		return;

	}
	
	ClientData->AckMove(MoveIndex);
	// Trust the server data
	FVector WorldLocation = FRepMovement::RebaseOntoLocalOrigin(NewLoc, this);
	UpdatedPrimitive->SetWorldLocation(WorldLocation, false, nullptr, ETeleportType::ResetPhysics); 
	UpdatedPrimitive->SetPhysicsAngularVelocityInDegrees(NewAngular); 
	UpdatedPrimitive->SetPhysicsLinearVelocity(NewLinear);
	LastUpdateLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector; 
	LastUpdateRotation = UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity; 
	ClientData->bUpdatePosition = true;
}

bool UNetPhysVehicleMovementComponent::ClientAdjustPosition_Validate(float TimeStamp, FVector NewLoc, FVector NewLinear, FVector NewAngular)
{
	return true;
}

void UNetPhysVehicleMovementComponent::ClientVeryShortAdjustPosition(float TimeStamp, FVector NewLoc) 
{
	VehicleOwner->ClientVeryShortAdjustPosition(TimeStamp, NewLoc);
}

void UNetPhysVehicleMovementComponent::ClientVeryShortAdjustPosition_Implementation(float TimeStamp, FVector NewLoc)
{
	if (HasValidData())
	{
		ClientAdjustPosition(TimeStamp, NewLoc, FVector::ZeroVector, FVector::ZeroVector);
	}
}

bool UNetPhysVehicleMovementComponent::ClientVeryShortAdjustPosition_Validate(float TimeStamp, FVector NewLoc)
{
	return true;
}

float FNetPhysNetworkPredictionData_Client_Vehicle::UpdateTimeStampAndDeltaTime(float DeltaTime, ATP_VehiclePawn& VehicleOwner, class UNetPhysVehicleMovementComponent& VehicleMovementComponent) 
{
	// Reset TimeStamp regularly to combat float accuracy decreasing over time.
	if (CurrentTimeStamp > VehicleMovementComponent.MinTimeBetweenTimeStampResets)
	{
		UE_LOG(LogNetPlayerMovement, Log, TEXT("Resetting Client's TimeStamp %f"), CurrentTimeStamp);
		CurrentTimeStamp -= VehicleMovementComponent.MinTimeBetweenTimeStampResets;

		// Mark all buffered moves as having old time stamps, so we make sure to not resend them.
		// That would confuse the server.
		for (int32 MoveIndex = 0; MoveIndex < SavedMoves.Num(); MoveIndex++)
		{
			const FVehicleSavedMovePtr& CurrentMove = SavedMoves[MoveIndex];
			SavedMoves[MoveIndex]->bOldTimeStampBeforeReset = true;
		}
		// Do LastAckedMove as well. No need to do PendingMove as that move is part of the SavedMoves array.
		if (LastAckedMove.IsValid())
		{
			LastAckedMove->bOldTimeStampBeforeReset = true;
		}

		// Also apply the reset to any active root motions.
		//Replace??
		//VehicleMovementComponent.CurrentRootMotion.ApplyTimeStampReset(VehicleMovementComponent.MinTimeBetweenTimeStampResets);
	}

	// Update Current TimeStamp.
	CurrentTimeStamp += DeltaTime;
	float ClientDeltaTime = DeltaTime;

	// Server uses TimeStamps to derive DeltaTime which introduces some rounding errors.
	// Make sure we do the same, so MoveAutonomous uses the same inputs and is deterministic!!
	if (SavedMoves.Num() > 0)
	{
		const FVehicleSavedMovePtr& PreviousMove = SavedMoves.Last();
		if (!PreviousMove->bOldTimeStampBeforeReset)
		{
			// How server will calculate its deltatime to update physics.
			const float ServerDeltaTime = CurrentTimeStamp - PreviousMove->TimeStamp;
			// Have client always use the Server's DeltaTime. Otherwise our physics simulation will differ and we'll trigger too many position corrections and increase our network traffic.
			ClientDeltaTime = ServerDeltaTime;
		}
	}

	return FMath::Min(ClientDeltaTime, MaxMoveDeltaTime * VehicleOwner.GetActorTimeDilation());
};

void FNetPhysNetworkPredictionData_Client_Vehicle::FreeMove(const FVehicleSavedMovePtr& Move) 
{
	if (Move.IsValid())
	{
		// Only keep a pool of a limited number of moves.
		if (FreeMoves.Num() < MaxFreeMoveCount)
		{
			FreeMoves.Push(Move);
		}

		// Shouldn't keep a reference to the move on the free list.
		if (PendingMove == Move)
		{
			PendingMove = NULL;
		}
		if (LastAckedMove == Move)
		{
			LastAckedMove = NULL;
		}
	}
};

void FSavedMove_Vehicle::Clear()
{
	bOldTimeStampBeforeReset = false;
	bForceNoCombine = false;

	TimeStamp = 0.f;
	DeltaTime = 0.f;
	CustomTimeDilation = 0.f;

	StartLocation = FVector::ZeroVector;
	StartRotation = FRotator::ZeroRotator;
	StartControlRotation = FRotator::ZeroRotator;
	StartLinearVelocity = FVector::ZeroVector;
	StartAngularVelocity = FVector::ZeroVector;

	SavedLocation = FVector::ZeroVector;
	SavedRotation = FRotator::ZeroRotator;
	SavedControlRotation = FRotator::ZeroRotator;
	SavedLinearVelocity = FVector::ZeroVector;
	SavedAngularVelocity = FVector::ZeroVector;
};

void FSavedMove_Vehicle::SetMoveFor(ATP_VehiclePawn* P, float InDeltaTime, class FNetPhysNetworkPredictionData_Client_Vehicle& ClientData)
{
	VehicleOwner = P;
	DeltaTime = InDeltaTime;

	SetInitialPosition(P);

	TimeStamp = ClientData.CurrentTimeStamp;
};

void FSavedMove_Vehicle::PostUpdate(ATP_VehiclePawn* P)
{
	USkeletalMeshComponent* MeshComp = P->GetMesh();
	SavedLocation = MeshComp->GetComponentLocation();
	SavedRotation = MeshComp->GetComponentRotation();
	SavedAngularVelocity = MeshComp->GetPhysicsAngularVelocityInDegrees();
	SavedLinearVelocity = MeshComp->GetPhysicsLinearVelocity();
	SavedControlRotation = P->GetControlRotation().Clamp();
};

void FSavedMove_Vehicle::CombineWith(const FSavedMove_Vehicle* OldMove, ATP_VehiclePawn* InVehicle, APlayerController* PC, const FVector& OldStartLocation)
{
	USkeletalMeshComponent* MeshComp = InVehicle->GetMesh();
	MeshComp->SetWorldLocationAndRotation(OldStartLocation, OldMove->StartRotation, false, nullptr, ETeleportType::ResetPhysics);
	MeshComp->SetPhysicsAngularVelocityInDegrees(OldMove->StartAngularVelocity);
	MeshComp->SetPhysicsLinearVelocity(OldMove->StartLinearVelocity);
	// Adjust the duration of the moves since they are combined moves 
	DeltaTime += OldMove->DeltaTime;
};

void FSavedMove_Vehicle::SetInitialPosition(ATP_VehiclePawn* P)
{
	USkeletalMeshComponent* MeshComp = P->GetMesh();
	StartLocation = MeshComp->GetComponentLocation();
	StartRotation = MeshComp->GetComponentRotation();
	StartAngularVelocity = MeshComp->GetPhysicsAngularVelocityInDegrees();
	StartLinearVelocity = MeshComp->GetPhysicsLinearVelocity();
	CustomTimeDilation = P->CustomTimeDilation;
	StartControlRotation = P->GetControlRotation();
};

bool FSavedMove_Vehicle::IsImportantMove(const FVehicleSavedMovePtr& LastAckedMovePtr) const
{
	const FSavedMove_Vehicle* LastAckedMove = LastAckedMovePtr.Get();

	if (GetCompressedFlags() != LastAckedMove->GetCompressedFlags())
	{
		return true;
	}
	return false;
};

bool FSavedMove_Vehicle::CanCombineWith(const FVehicleSavedMovePtr& NewMovePtr, ATP_VehiclePawn* P, float MaxDelta) const
{
	const FSavedMove_Vehicle* NewMove = NewMovePtr.Get();

	if (bForceNoCombine || NewMove->bForceNoCombine)
	{
		return false;
	}

	if (StartLinearVelocity.IsZero() != NewMove->StartLinearVelocity.IsZero())
	{
		return false;
	}
	if (GetCompressedFlags() != NewMove->GetCompressedFlags())
	{
		return false;
	}
	if (CustomTimeDilation != NewMove->CustomTimeDilation)
	{
		return false;
	}
	return true;

};

bool UNetPhysVehicleMovementComponent::IsClientTimeStampValid(float TimeStamp, const FNetPhysNetworkPredictionData_Server_Vehicle& ServerData, bool& bTimeStampResetDetected) const
{
	if (TimeStamp <= 0.f || !FMath::IsFinite(TimeStamp))
	{
		return false;
	}

	// Very large deltas happen around a TimeStamp reset.
	const float DeltaTimeStamp = (TimeStamp - ServerData.CurrentClientTimeStamp);
	if (FMath::Abs(DeltaTimeStamp) > (MinTimeBetweenTimeStampResets * 0.5f))
	{
		// Client is resetting TimeStamp to increase accuracy.
		bTimeStampResetDetected = true;
		if (DeltaTimeStamp < 0.f)
		{
			// Validate that elapsed time since last reset is reasonable, otherwise client could be manipulating resets.
			if (GetWorld()->TimeSince(LastTimeStampResetServerTime) < (MinTimeBetweenTimeStampResets * 0.5f))
			{
				// Reset too recently
				return false;
			}
			else
			{
				// TimeStamp accepted with reset
				return true;
			}
		}
		else
		{
			// We already reset the TimeStamp, but we just got an old outdated move before the switch, not valid.
			return false;
		}
	}

	// If TimeStamp is in the past, move is outdated, not valid.
	if (TimeStamp <= ServerData.CurrentClientTimeStamp)
	{
		return false;
	}

	// Precision issues (or reordered timestamps from old moves) can cause very small or zero deltas which cause problems.
	if (DeltaTimeStamp < UNetPhysVehicleMovementComponent::MIN_TICK_TIME)
	{
		return false;
	}

	// TimeStamp valid.
	return true;
}

void UNetPhysVehicleMovementComponent::ProcessClientTimeStampForTimeDiscrepancy(float ClientTimeStamp, FNetPhysNetworkPredictionData_Server_Vehicle& ServerData)
{
	// Should only be called on server in network games
	check(VehicleOwner != NULL);
	check(VehicleOwner->GetLocalRole() == ROLE_Authority);
	checkSlow(GetNetMode() < NM_Client);

	// Movement time discrepancy detection and resolution (potentially caused by client speed hacks, time manipulation)
	// Track client reported time deltas through ServerMove RPCs vs actual server time, when error accumulates enough
	// trigger prevention measures where client must "pay back" the time difference
	const bool bServerMoveHasOccurred = ServerData.ServerTimeStampLastServerMove != 0.f;
	const AGameNetworkManager* GameNetworkManager = (const AGameNetworkManager*)(AGameNetworkManager::StaticClass()->GetDefaultObject());
	if (GameNetworkManager != nullptr && GameNetworkManager->bMovementTimeDiscrepancyDetection && bServerMoveHasOccurred)
	{
		const float WorldTimeSeconds = GetWorld()->GetTimeSeconds();
		const float ServerDelta = (WorldTimeSeconds - ServerData.ServerTimeStamp) * VehicleOwner->CustomTimeDilation;
		const float ClientDelta = ClientTimeStamp - ServerData.CurrentClientTimeStamp;
		const float ClientError = ClientDelta - ServerDelta; // Difference between how much time client has ticked since last move vs server

		// Accumulate raw total discrepancy, unfiltered/unbound (for tracking more long-term trends over the lifetime of the CharacterMovementComponent)
		ServerData.LifetimeRawTimeDiscrepancy += ClientError;

		//
		// 1. Determine total effective discrepancy 
		//
		// NewTimeDiscrepancy is bounded and has a DriftAllowance to limit momentary burst packet loss or 
		// low framerate from having significant impacts, which could cause needing multiple seconds worth of 
		// slow-down/speed-up even though it wasn't intentional time manipulation
		float NewTimeDiscrepancy = ServerData.TimeDiscrepancy + ClientError;
		{
			// Apply drift allowance - forgiving percent difference per time for error
			const float DriftAllowance = GameNetworkManager->MovementTimeDiscrepancyDriftAllowance;
			if (DriftAllowance > 0.f)
			{
				if (NewTimeDiscrepancy > 0.f)
				{
					NewTimeDiscrepancy = FMath::Max(NewTimeDiscrepancy - ServerDelta * DriftAllowance, 0.f);
				}
				else
				{
					NewTimeDiscrepancy = FMath::Min(NewTimeDiscrepancy + ServerDelta * DriftAllowance, 0.f);
				}
			}

			// Enforce bounds
			// Never go below MinTimeMargin - ClientError being negative means the client is BEHIND
			// the server (they are going slower).
			NewTimeDiscrepancy = FMath::Max(NewTimeDiscrepancy, GameNetworkManager->MovementTimeDiscrepancyMinTimeMargin);
		}

		// Determine EffectiveClientError, which is error for the currently-being-processed move after 
		// drift allowances/clamping/resolution mode modifications.
		// We need to know how much the current move contributed towards actionable error so that we don't
		// count error that the server never allowed to impact movement to matter
		float EffectiveClientError = ClientError;
		{
			const float NewTimeDiscrepancyRaw = ServerData.TimeDiscrepancy + ClientError;
			if (NewTimeDiscrepancyRaw != 0.f)
			{
				EffectiveClientError = ClientError * (NewTimeDiscrepancy / NewTimeDiscrepancyRaw);
			}
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// Per-frame spew of time discrepancy-related values - useful for investigating state of time discrepancy tracking
		if (VehicleMovementCVars::DebugTimeDiscrepancy > 0)
		{
			UE_LOG(LogNetPlayerMovement, Warning, TEXT("TimeDiscrepancyDetection: ClientError: %f, TimeDiscrepancy: %f, LifetimeRawTimeDiscrepancy: %f (Lifetime %f), Resolving: %d, ClientDelta: %f, ServerDelta: %f, ClientTimeStamp: %f"),
				ClientError, ServerData.TimeDiscrepancy, ServerData.LifetimeRawTimeDiscrepancy, WorldTimeSeconds - ServerData.WorldCreationTime, ServerData.bResolvingTimeDiscrepancy, ClientDelta, ServerDelta, ClientTimeStamp);
		}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

		//
		// 2. If we were in resolution mode, determine if we still need to be
		//
		ServerData.bResolvingTimeDiscrepancy = ServerData.bResolvingTimeDiscrepancy && (ServerData.TimeDiscrepancy > 0.f);

		//
		// 3. Determine if NewTimeDiscrepancy is significant enough to trigger detection, and if so, trigger resolution if enabled
		//
		if (!ServerData.bResolvingTimeDiscrepancy)
		{
			if (NewTimeDiscrepancy > GameNetworkManager->MovementTimeDiscrepancyMaxTimeMargin)
			{
				// Time discrepancy detected - client timestamp ahead of where the server thinks it should be!

				// Trigger logic for resolving time discrepancies
				if (GameNetworkManager->bMovementTimeDiscrepancyResolution)
				{
					// Trigger Resolution
					ServerData.bResolvingTimeDiscrepancy = true;

					// Transfer calculated error to official TimeDiscrepancy value, which is the time that will be resolved down
					// in this and subsequent moves until it reaches 0 (meaning we equalize the error)
					// Don't include contribution to error for this move, since we are now going to be in resolution mode
					// and the expected client error (though it did help trigger resolution) won't be allowed
					// to increase error this frame
					ServerData.TimeDiscrepancy = (NewTimeDiscrepancy - EffectiveClientError);
				}
				else
				{
					// We're detecting discrepancy but not handling resolving that through movement component.
					// Clear time stamp error accumulated that triggered detection so we start fresh (maybe it was triggered
					// during severe hitches/packet loss/other non-goodness)
					ServerData.TimeDiscrepancy = 0.f;
				}

				// Project-specific resolution (reporting/recording/analytics)
				OnTimeDiscrepancyDetected(NewTimeDiscrepancy, ServerData.LifetimeRawTimeDiscrepancy, WorldTimeSeconds - ServerData.WorldCreationTime, ClientError);
			}
			else
			{
				// When not in resolution mode and still within error tolerances, accrue total discrepancy
				ServerData.TimeDiscrepancy = NewTimeDiscrepancy;
			}
		}

		//
		// 4. If we are actively resolving time discrepancy, we do so by altering the DeltaTime for the current ServerMove
		//
		if (ServerData.bResolvingTimeDiscrepancy)
		{
			// Optionally force client corrections during time discrepancy resolution
			// This is useful when default project movement error checking is lenient or ClientAuthorativePosition is enabled
			// to ensure time discrepancy resolution is enforced
			if (GameNetworkManager->bMovementTimeDiscrepancyForceCorrectionsDuringResolution)
			{
				ServerData.bForceClientUpdate = true;
			}

			// Movement time discrepancy resolution
			// When the server has detected a significant time difference between what the client ServerMove RPCs are reporting
			// and the actual time that has passed on the server (pointing to potential speed hacks/time manipulation by client),
			// we enter a resolution mode where the usual "base delta's off of client's reported timestamps" is clamped down
			// to the server delta since last movement update, so that during resolution we're not allowing further advantage.
			// Out of that ServerDelta-based move delta, we also need the client to "pay back" the time stolen from initial 
			// time discrepancy detection (held in TimeDiscrepancy) at a specified rate (AGameNetworkManager::TimeDiscrepancyResolutionRate) 
			// to equalize movement time passed on client and server before we can consider the discrepancy "resolved"
			const float ServerCurrentTimeStamp = WorldTimeSeconds;
			const float ServerDeltaSinceLastMovementUpdate = (ServerCurrentTimeStamp - ServerData.ServerTimeStamp) * VehicleOwner->CustomTimeDilation;
			const bool bIsFirstServerMoveThisServerTick = ServerDeltaSinceLastMovementUpdate > 0.f;

			// Restrict ServerMoves to server deltas during time discrepancy resolution 
			// (basing moves off of trusted server time, not client timestamp deltas)
			const float BaseDeltaTime = ServerData.GetBaseServerMoveDeltaTime(ClientTimeStamp, VehicleOwner->GetActorTimeDilation());

			if (!bIsFirstServerMoveThisServerTick)
			{
				// Accumulate client deltas for multiple ServerMoves per server tick so that the next server tick
				// can pay back the full amount of that tick and not be bounded by a single small Move delta
				ServerData.TimeDiscrepancyAccumulatedClientDeltasSinceLastServerTick += BaseDeltaTime;
			}

			float ServerBoundDeltaTime = FMath::Min(BaseDeltaTime + ServerData.TimeDiscrepancyAccumulatedClientDeltasSinceLastServerTick, ServerDeltaSinceLastMovementUpdate);
			ServerBoundDeltaTime = FMath::Max(ServerBoundDeltaTime, 0.f); // No negative deltas allowed

			if (bIsFirstServerMoveThisServerTick)
			{
				// The first ServerMove for a server tick has used the accumulated client delta in the ServerBoundDeltaTime
				// calculation above, clear it out for next frame where we have multiple ServerMoves
				ServerData.TimeDiscrepancyAccumulatedClientDeltasSinceLastServerTick = 0.f;
			}

			// Calculate current move DeltaTime and PayBack time based on resolution rate
			const float ResolutionRate = FMath::Clamp(GameNetworkManager->MovementTimeDiscrepancyResolutionRate, 0.f, 1.f);
			float TimeToPayBack = FMath::Min(ServerBoundDeltaTime * ResolutionRate, ServerData.TimeDiscrepancy); // Make sure we only pay back the time we need to
			float DeltaTimeAfterPayback = ServerBoundDeltaTime - TimeToPayBack;

			// Adjust deltas so current move DeltaTime adheres to minimum tick time
			DeltaTimeAfterPayback = FMath::Max(DeltaTimeAfterPayback, UNetPhysVehicleMovementComponent::MIN_TICK_TIME);
			TimeToPayBack = ServerBoundDeltaTime - DeltaTimeAfterPayback;

			// Output of resolution: an overridden delta time that will be picked up for this ServerMove, and removing the time
			// we paid back by overriding the DeltaTime to TimeDiscrepancy (time needing resolved)
			ServerData.TimeDiscrepancyResolutionMoveDeltaOverride = DeltaTimeAfterPayback;
			ServerData.TimeDiscrepancy -= TimeToPayBack;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			// Per-frame spew of time discrepancy resolution related values - useful for investigating state of time discrepancy tracking
			if (VehicleMovementCVars::DebugTimeDiscrepancy > 1)
			{
				UE_LOG(LogNetPlayerMovement, Warning, TEXT("TimeDiscrepancyResolution: DeltaOverride: %f, TimeToPayBack: %f, BaseDelta: %f, ServerDeltaSinceLastMovementUpdate: %f, TimeDiscrepancyAccumulatedClientDeltasSinceLastServerTick: %f"),
					ServerData.TimeDiscrepancyResolutionMoveDeltaOverride, TimeToPayBack, BaseDeltaTime, ServerDeltaSinceLastMovementUpdate, ServerData.TimeDiscrepancyAccumulatedClientDeltasSinceLastServerTick);
			}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		}
	}
}

void UNetPhysVehicleMovementComponent::OnTimeDiscrepancyDetected(float CurrentTimeDiscrepancy, float LifetimeRawTimeDiscrepancy, float Lifetime, float CurrentMoveError)
{
	//LogIt
};

void UNetPhysVehicleMovementComponent::ForceClientAdjustment()
{
	ServerLastClientAdjustmentTime = -1.f;
};

bool UNetPhysVehicleMovementComponent::VerifyClientTimeStamp(float TimeStamp, FNetPhysNetworkPredictionData_Server_Vehicle& ServerData) 
{ 
	bool bTimeStampResetDetected = false;
	const bool bIsValid = IsClientTimeStampValid(TimeStamp, ServerData, bTimeStampResetDetected);
	if (bIsValid)
	{
		if (bTimeStampResetDetected)
		{
			UE_LOG(LogNetPlayerMovement, Log, TEXT("TimeStamp reset detected. CurrentTimeStamp: %f, new TimeStamp: %f"), ServerData.CurrentClientTimeStamp, TimeStamp);
			LastTimeStampResetServerTime = GetWorld()->GetTimeSeconds();
			ServerData.CurrentClientTimeStamp -= MinTimeBetweenTimeStampResets;

			// Also apply the reset to any active root motions.
			//CurrentRootMotion.ApplyTimeStampReset(MinTimeBetweenTimeStampResets);
		}
		else
		{
			UE_LOG(LogNetPlayerMovement, VeryVerbose, TEXT("TimeStamp %f Accepted! CurrentTimeStamp: %f"), TimeStamp, ServerData.CurrentClientTimeStamp);
			ProcessClientTimeStampForTimeDiscrepancy(TimeStamp, ServerData);
		}
		return true;
	}
	else
	{
		if (bTimeStampResetDetected)
		{
			UE_LOG(LogNetPlayerMovement, Log, TEXT("TimeStamp expired. Before TimeStamp Reset. CurrentTimeStamp: %f, TimeStamp: %f"), ServerData.CurrentClientTimeStamp, TimeStamp);
		}
		return false;
	}
};

void UNetPhysVehicleMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	//flags for - jumping, crouching ect
};

void UNetPhysVehicleMovementComponent::MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags)
{
	if (!HasValidData())
	{
		return;
	}

	UpdateFromCompressedFlags(CompressedFlags);

	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const FQuat OldRotation = UpdatedComponent->GetComponentQuat();


	PerformMovement(DeltaTime);

	// Check if data is valid as PerformMovement can mark character for pending kill
	if (!HasValidData())
	{
		return;
	}

	if (VehicleOwner && UpdatedComponent)
	{
		// Smooth local view of remote clients on listen servers
		if (VehicleMovementCVars::NetEnableListenServerSmoothing &&
			VehicleOwner->GetRemoteRole() == ROLE_AutonomousProxy &&
			IsNetMode(NM_ListenServer))
		{
			SmoothCorrection(OldLocation, OldRotation, UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentQuat());
		}
	}
};


bool UNetPhysVehicleMovementComponent::HasPredictionData_Client() const
{
	return (ClientPredictionData != nullptr) && HasValidData();
}

bool UNetPhysVehicleMovementComponent::HasPredictionData_Server() const
{
	return (ServerPredictionData != nullptr) && HasValidData();
}

void UNetPhysVehicleMovementComponent::ResetPredictionData_Client()
{
	ForceClientAdjustment();
	if (ClientPredictionData)
	{
		delete ClientPredictionData;
		ClientPredictionData = nullptr;
	}
}

void UNetPhysVehicleMovementComponent::ResetPredictionData_Server()
{
	ForceClientAdjustment();
	if (ServerPredictionData)
	{
		delete ServerPredictionData;
		ServerPredictionData = nullptr;
	}
}

bool UNetPhysVehicleMovementComponent::ForcePositionUpdate(float DeltaTime)
{
	if (!HasValidData() || UpdatedComponent->Mobility != EComponentMobility::Movable)
	{
		return false;
	}

	check(VehicleOwner->GetLocalRole() == ROLE_Authority);
	check(VehicleOwner->GetRemoteRole() == ROLE_AutonomousProxy);



	FNetPhysNetworkPredictionData_Server_Vehicle* ServerData = GetPredictionData_Server_Vehicle();

	// Increment client timestamp so we reject client moves after this new simulated time position.
	ServerData->CurrentClientTimeStamp += DeltaTime;

	// Increment server timestamp so ServerLastTransformUpdateTimeStamp gets changed if there is an actual movement.
	const double SavedServerTimestamp = ServerData->ServerAccumulatedClientTimeStamp;
	ServerData->ServerAccumulatedClientTimeStamp += DeltaTime;

	const bool bServerMoveHasOccurred = (ServerData->ServerTimeStampLastServerMove != 0.f);
	if (bServerMoveHasOccurred)
	{
		//UE_LOG(LogNetPlayerMovement, Log, TEXT("ForcePositionUpdate: %s (DeltaTime %.2f -> ServerTimeStamp %.2f)"), *GetNameSafe(CharacterOwner), DeltaTime, ServerData->CurrentClientTimeStamp);
	}

	// Force movement update.
	PerformMovement(DeltaTime);

	// TODO: smooth correction on listen server?
	return true;
};

