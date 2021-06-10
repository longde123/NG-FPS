// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehicleMovementComponent4W.h"
#include "Interfaces/NetworkPredictionInterface.h" 
#include "NetPhysVehicleMovementComponent.generated.h"


DECLARE_STATS_GROUP(TEXT("VehicleMovementNetworking"), STATGROUP_NetPhysVehicle, STATCAT_Advanced);

typedef TSharedPtr<class FSavedMove_Vehicle> FVehicleSavedMovePtr;


/** Defines how to smooth corrections on the client sent from the server */
UENUM(Blueprintable)
enum class ENetPhysSmoothingMode : uint8
{
	Disabled,
	Linear,
	Exponential
};

class ATP_VehiclePawn;

UCLASS()
class GDKSHOOTER_API UNetPhysVehicleMovementComponent : public UWheeledVehicleMovementComponent4W, public INetworkPredictionInterface
{
	GENERATED_BODY()

public:

	UNetPhysVehicleMovementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	/** The vehicle that this movement component is driving */
	UPROPERTY(Transient, DuplicateTransient)
	ATP_VehiclePawn* VehicleOwner;

	// ~ begin UActorComponent interface 
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	// ~ end UActorComponent

	/** Gets if this component has all necessary data to function */
	virtual bool HasValidData() const;
	/** Minimum delta time considered when ticking. Delta times below this are not considered.
	This is a very small non-zero value to avoid potential divideby-zero in simulation code. */
	static const float MIN_TICK_TIME;
	// --
	// Networking implementation
	/** How we should smooth corrections on the client */
	UPROPERTY(Category = "Vehicle Movement (Networking)", EditAnywhere, BlueprintReadwrite)
		ENetPhysSmoothingMode NetworkSmoothingMode;

	/** Maximum distance character is allowed to lag behind server location when interpolating between updates. */
	UPROPERTY(Category = "Vehicle Movement (Networking)", EditDefaultsOnly, meta = (ClampMin = "0.0", UIMin = "0.0"))
		float NetworkMaxSmoothUpdateDistance;

	/** Maximum distance beyond which character is teleported to the new server location without any smoothing. */
	UPROPERTY(Category = "Vehicle Movement (Networking)", EditDefaultsOnly, meta = (ClampMin = "0.0", UIMin = "0.0"))
		float NetworkNoSmoothUpdateDistance;

	/**How long to take to smoothly interpolate from the old pawn position on the client to the corrected one sent by the server.Not used by Linear smoothing.*/
	UPROPERTY(Category = "Vehicle Movement (Networking)", EditDefaultsOnly, AdvancedDisplay, meta = (CLampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
		float NetworkSmoothLocationTime;

	/**
	* How long to take to smoothly interpolate from the old pawn rotation on the client to the corrected one sent by the server. Not used by Linear smoothing.
	*/
	UPROPERTY(Category = "Vehicle Movement (Networking)", EditDefaultsOnly, AdvancedDisplay, meta = (CLampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
		float NetworkSmoothRotationTime;

	float ListenServerNetworkSimulatedSmoothLocationTime;
	float ListenServerNetworkSimulatedSmoothRotationTime;

	/**
	 * How long to take to smoothly interpolate from the old pawn position on the client to the corrected one sent by the server. Not used by Linear smoothing.
	 */
	UPROPERTY(Category = "Character Movement (Networking)", EditDefaultsOnly, AdvancedDisplay, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
		float NetworkSimulatedSmoothLocationTime;

	/**
	 * How long to take to smoothly interpolate from the old pawn rotation on the client to the corrected one sent by the server. Not used by Linear smoothing.
	 */
	UPROPERTY(Category = "Character Movement (Networking)", EditDefaultsOnly, AdvancedDisplay, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
		float NetworkSimulatedSmoothRotationTime;


	/**
	* True when we should ignore server location difference checks for client error on this movement component
	* This can be useful when vehicle is moving at extreme speeds for a duration and you need it to look
	* smooth on clients. Make sure to disable when done, as this would break this vehicle's server-client
	* movement correction.
	*/
	UPROPERTY(Transient, Category = "Vehicle Movement (Networking)", EditAnywhere, BlueprintReadWrite)
		uint8 bIgnoreClientMovementErrorChecksAndCorrection : 1;

	/**
	* Minimum time on the server between acknowledging good client moves. This can save on bandwidth. Set to o to disable throttling.
	*/
	UPROPERTY(Category = "Vehicle Movement (Networking)", EditDefaultsOnly, meta = (ClampMin = "0.0", UIMin = "0.0"))
		float NetworkMinTimeBetweenClientAckGoodMoves;

	/**
	* Minimum time on the server between sending client adjustments when client has exceeded allowable position error.
	* Should be >= NetworkMinTimeBetweenClientAdjustments LargeCorrection (the larger value is used regardless).
	* This can save on bandwidth. Set to o to disable throttling.
	* @see ServerlastClientAdjustment Time
	*/
	UPROPERTY(Category = "Vehicle Movement (Networking)", EditDefaultsOnly, meta = (ClampMin = "0.0", UIMin = "0.0"))
		float NetworkMinTimeBetweenClientAdjustments;

	/**
	* Minimum time on the server between sending client adjustments when client has exceeded allowable position error by a large amount
	(NetworkLargeClientCorrectionDistance).
	* Should be <= NetworkMinTimeBetweenClientAdjustments (the smaller value is used regardless).
	* @see NetworkMinTimeBetweenClientAdjustments
	*/
	UPROPERTY(Category = "Vehicle Movement (Networking)", EditDefaultsOnly, meta = (ClampMin = "0.0", UIMin = "0.0"))
		float NetworkMinTimeBetweenClientAdjustmentsLargeCorrection;

	/**
	* If client error is larger than this, sets bNetworkLargeClientCorrection to reduce delay between client adjustments.
	* @see NetworkMinTimeBetweenClientAdjustments, NetworkMinTimeBetweenClientAdjustments LargeCorrection
	*/
	UPROPERTY(Category = "Vehicle Movement (Networking)", EditDefaultsOnly, meta = (ClampMin = "0.0", UIMin = "0.0"))
		float NetworkLargeClientCorrectionDistance;

	/** If we received a network update from the server as a simulated proxy */
	UPROPERTY(Transient)
		uint32 bNetworkUpdateReceived : 1;

	/** Flag used as optimization to skip simulated proxy position smoothing */
	uint32 bNetworkSmoothingComplete : 1;

	/** Flag indicating the client correction was forced on the server data */
	uint8 bNetworkLargeClientCorrection : 1;

	/**
	* Minimum time between client TimeStamp resets for moves.
	* Needs to be large enough so that we dont confuse the server if the client can stall or timeout * We do this as we use floats for TimeStamps and server derives DeltaTime from two TimeStamps * As Time goes on, accuracy decreases from the floats
	*/
	UPROPERTY()
	float MinTimeBetweenTimeStampResets;

	UPROPERTY()
	FVector LastUpdateLocation;

	UPROPERTY()
	FQuat LastUpdateRotation;

	/** Timestamp when location or rotation last changed during an update. Only valid on the server. */
	UPROPERTY(Transient)
		float ServerLastTransformUpdateTimeStamp;

	/** Timestamp of last client adjustment sent. See NetworkMinTimeBetweenClientAdjustments.*/
	UPROPERTY(Transient)
	float ServerLastClientGoodMoveAckTime;

	/** Timestamp of last client adjustment sent. See NetworkMinTimeBetweenClientAdjustments. */
	UPROPERTY(Transient)
		float ServerLastClientAdjustmentTime;

	/** (Server) Sends an adjustinent to the owning pawn from the server */
	virtual void SendClientAdjustment() override;

	virtual bool ForcePositionUpdate(float DeltaTime) override;

	/** (Client) Called when we received a network update which we will smooth to */
	virtual void SmoothCorrection(const FVector& OldLocation, const FQuat& OldRotation, const FVector& NewLocation, const FQuat& NewRotation) override;

	/** @return FNetworkPredictionData_Client instance used for storing client network data */
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	/** @reture FNetworkPredictionDate_Server instance used for storing server network data */
	virtual class FNetworkPredictionData_Server* GetPredictionData_Server() const override;

	virtual class FNetPhysNetworkPredictionData_Client_Vehicle* GetPredictionData_Client_Vehicle() const;
	virtual class FNetPhysNetworkPredictionData_Server_Vehicle* GetPredictionData_Server_Vehicle() const;

	virtual bool HasPredictionData_Client() const override;
	virtual bool HasPredictionData_Server() const override;

	virtual void ResetPredictionData_Client() override;
	virtual void ResetPredictionData_Server() override;

protected:
	class FNetPhysNetworkPredictionData_Client_Vehicle* ClientPredictionData;
	class FNetPhysNetworkPredictionData_Server_Vehicle* ServerPredictionData;

	/**
	* Internally calls vnClient Position_Interpolate() and SmoothClient Position_Updatevisuals) to handle mesn smoothing for network corrections
	* @see UNetPhysVehicleMovement Component::SmoothClient Position_Interpolate
	* @see UNetPhysVehicleMovement Component::SmoothClient Posifion_UpdateVisuals
	*/
	virtual void SmoothClientPosition(float DeltaSeconds);

	/**
	* Interpelates the value to use to smooth the vehicle mesh location.
	* Sete UNetworkSmoothing Complete to true when it reached the target
	*/
	virtual void SmoothClientPosition_Interpolate(float DeltaSeconds);

	/** Updates the vehicle mesh location to the updated interpeletion value */
	virtual void SmoothClientPosition_UpdateVisuals(float DeltaSeconds);

	/** Send the current lucal pawn owner move to the servery which the server will respond with a correction */
	virtual void ReplicateMoveToServer(float DeltaSeconds);

	/** If ClientData->Update Position is true, then replay any unacked moves. Returns whether any moves rere actualiy replayed */
	virtual bool ClientUpdatePositionAfterServerUpdate();

	/** Calls the correct ServerMove () function */
	virtual void CallServerMove(const class FSavedMove_Vehicle* NewMove, const class FSavedMove_Vehicle* OldMove);

	/**
	* Have the server check if the client is outside an error tolerance, and queue a client adjustment if so.
	* If either Get PredictionDate_Server Vehicle()->bForceClientUpdate or ServerCheckClientError() are true, the client adjustment will be sent.
	* @see ServerCheckClientError()
	*/
	virtual void ServerMoveHandleClientError(float ClientTimeStamp, float DeltaTime, const FVector& Location);

	/**
	* Check for Server-Client disagreement in position or other movement state important enough to trigger a client correction.
	* @see Server MoveHandleClientError
	*/
	virtual bool ServerCheckClientError(float ClientTimeStamp, float DeltaTime, const FVector& ClientWorldLocation, const FVector& Location);

	/** Called on the server to actually move the pawn */
	virtual void PerformMovement(float DeltaSeconds);

	/** Updates the compressed flags and called Performiovement on the server */
	virtual void MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags);
	
	/** Unpack compressed flags from a saved move and set state accordingly. See FSavedMove_Character. */
	virtual void UpdateFromCompressedFlags(uint8 Flags);


	/** Determine minimum delay between sending client updates to the server */
	virtual float GetClientNetSendDeltaTime(const APlayerController* PC, const FNetPhysNetworkPredictionData_Client_Vehicle* ClientData, const FVehicleSavedMovePtr& NewMove) const {
		return 0;
	};
	/** Packs a yaw and pitch */
	uint32 PackYawAndPitchTo32(const float Yaw, const float Pitch) 
	{
		const uint32 YawShort = FRotator::CompressAxisToShort(Yaw);
		const uint32 PitchShort = FRotator::CompressAxisToShort(Pitch);
		const uint32 Rotation32 = (YawShort << 16) | PitchShort;
		return Rotation32;
	};

	/** Clock time on the server bf the last timestamp reset */
	float LastTimeStampResetServerTime;


	/** Internal const check for client timestamp validity without side effects */
	bool IsClientTimeStampValid(float TimeStamp, const FNetPhysNetworkPredictionData_Server_Vehicle& ServerData, bool& bTimeStampResetDetected) const;


	/**
	* Processes client timestamps from Server Moves, detects and protects against time discrepancy between client-reported times and server time
	* Called by UNetPhysVehicleMovement Component::VerifyClientTimeStamp() for valid timestamps.
	*/
	virtual void ProcessClientTimeStampForTimeDiscrepancy(float ClientTimeStamp, FNetPhysNetworkPredictionData_Server_Vehicle& ServerData);
	

	/**
	* Called by UNetPhysVehicleMovementComponent::ProcessClientTimeStampForTimeDiscrepancy (on server) when the time from client moves
	* significantly differs from the server time, indicating potential time manipulation by clients (speed hacks, significant network
	* issues. client performance problems)
	* @param CurrentTimeDiscrepancy Accumulated time difference between client ServerMove and server time - this is bounded
	* by MovementTimeDiscrepancy config variables in AGameNetworkManager, and is the value with which
	* we test against to trigger this function. This is reset when Movement TimeDiscrepancy resolution
	* is enabled
	* @param LifetimeRawTimeDiscrepancy Accumulated time difference between client ServerMove and server time - this is unbounded
	* and does NOT get affected by Movement TimeDiscrepancy resolution, and is useful as a longer-term
	* view of how the given client is performing. High magnitude unbounded error points to
	* intentional tampering by a client vs. Occasional "naturally caused" spikes in error due to
	* burst packet loss/performance hitches
	* @param Lifetime Game time over which LifetimeRawTimeDiscrepancy has accrued (useful for determining severity
	* of LifetimeUnboundedError)
	* @param Current MoveError Time difference between client ServerMove and how much time has passed on the server for the
	* current move that has caused TimeDiscrepancy to accumulate enough to trigger detection.
	*/
	virtual void OnTimeDiscrepancyDetected(float CurrentTimeDiscrepancy, float LifetimeRawTimeDiscrepancy, float Lifetime, float CurrentMoveError);

public:
	/** Force a client adjustment. Resets ServerlastClientAdjustment Time. */
	void ForceClientAdjustment();

	/** Verify that the incoming client TimeStamp is valid and has not expired on the server. Will also handle timestamp resets */
	virtual bool VerifyClientTimeStamp(float TimeStamp, FNetPhysNetworkPredictionData_Server_Vehicle& ServerData);

	// Network RPCs for movement
	/*
	*The actual RPCs are passed to the vehicle pawn, which wrap to the _Implementation and _validate call here, to avoid Component RPC overhead
	* For example:
	* Client: UNetPhysVehicleMovement Component::ServerMove(...) => Calls VehicleOwner->ServerMove (...) triggering RPC on server
	* Server: ATP_VehiclePawn:: ServerMove_Implementation(...) => Calls VehicleMovement->ServerMove_Implementation
	* To override the client call to the server RPC (on VehicleOwner), override Server Move().
	* To override the server implementation, override ServerMove_Implementation().
	*/

	/** Replicated function sent by client to server - contains client movement and view info */
	virtual void ServerMove(float TimeStamp, FVector_NetQuantize100 Location, uint8 Flags, uint8 Roll, uint32 View);
	virtual void ServerMove_Implementation(float TimeStamp, FVector_NetQuantize100 Location, uint8 Flags, uint8 Roll, uint32 View);
	virtual bool ServerMove_Validate(float TimeStamp, FVector_NetQuantize100 Location, uint8 Flags, uint8 Roll, uint32 View);

	
	/** Replicated function sent by client to server - contains client movement. and view info for two moves. Usually called when sending a pending move with a
	current move */
	virtual void ServerMoveDual(float TimeStamp0, uint8 PendingFlags, uint32 View0, float TimeStamp, FVector_NetQuantize100 Location, uint8 NewFlags, uint8
		Roll, uint32 View);

	virtual void ServerMoveDual_Implementation(float TimeStamp0, uint8 PendingFlags, uint32 View0, float TimeStamp, FVector_NetQuantize100 Location, uint8 NewFlags, uint8
		Roll, uint32 View);

	virtual bool ServerMoveDual_Validate(float TimeStamp0, uint8 PendingFlags, uint32 View0, float TimeStamp, FVector_NetQuantize100 Location, uint8 NewFlags,
		uint8 Roll, uint32 View);

	
	/* Resending an (important) old move. Process it if not already processed */
	virtual void ServerMoveOld(float OldTimeStamp, uint8 OldFlags);
	virtual void ServerMoveOld_Implementation(float OldTimeStamp, uint8 OldFlags);
	virtual bool ServerMoveOld_Validate(float OldTimeStamp, uint8 OldFlags);

	/** If no client adjustment is needed after processing received ServerMove(), ack the good move so clients can remove it from Saved Moves */
	virtual void ClientAckGoodMove(float Timestamp);
	virtual void ClientAckGoodMove_Implementation(float Timestamp);


	/** Replicate position correction to client */
	virtual void ClientAdjustPosition(float TimeStamp, FVector NewLoc, FVector NewLinear, FVector NewAngular);
	virtual void ClientAdjustPosition_Implementation(float TimeStamp, FVector NewLoc, FVector NewLinear, FVector NewAngular);
	virtual bool ClientAdjustPosition_Validate(float TimeStamp, FVector NewLoc, FVector NewLinear, FVector NewAngular);
	
	/** Bandwidth saving version of ClientAdjustPosition when velocity is zero */
	virtual void ClientVeryShortAdjustPosition(float TimeStamp, FVector NewLoc);
	virtual void ClientVeryShortAdjustPosition_Implementation(float TimeStamp, FVector NewLoc);
	virtual bool ClientVeryShortAdjustPosition_Validate(float TimeStamp, FVector NewLoc);
};
	/** A move on the server that was sent by the client and might need to be played back */
	//Finished
	class GDKSHOOTER_API FSavedMove_Vehicle
	{
	public:
		FSavedMove_Vehicle() {}; //Good
		virtual ~FSavedMove_Vehicle() {}; //Good

		ATP_VehiclePawn* VehicleOwner;

		/** If we should never combine this move with another move */
		uint32 bForceNoCombine : 1;

		/** If this move is using an old timestamp before it was reset */
		uint32 bOldTimeStampBeforeReset : 1;

		float TimeStamp; // World Time of when this move happead 
		float DeltaTime; // Duration of the move 
		float CustomTimeDilation;

		// Information at the start of the move 
		FVector StartLocation;
		FRotator StartRotation;
		FRotator StartControlRotation;
		FVector StartLinearVelocity;
		FVector StartAngularVelocity;

		// Information at the end of the move 
		FVector SavedLocation;
		FRotator SavedRotation;
		FRotator SavedControlRotation;
		FVector SavedLinearVelocity;
		FVector SavedAngularVelocity;

		/** Clears movement data so that this move can be reused without having to buffer a new instance */
		virtual void Clear();

		/** Sets up this saved move (when it is created to make a predictive correction */
		virtual void SetMoveFor(ATP_VehiclePawn* P, float InDeltaTime, class FNetPhysNetworkPredictionData_Client_Vehicle& ClientData);

		/** Sets the properties describing the position, etc. of the moved pawn at the start of the move */
		virtual void SetInitialPosition(ATP_VehiclePawn* P);

		/** @returns if this move is an important" move that should be sent again if not acked by the server */
		virtual bool IsImportantMove(const FVehicleSavedMovePtr& LastAckedMovePtr) const;

		/** Sets the properties describing the final (the end of the move) position, etc. of the moved pawn */
		virtual void PostUpdate(ATP_VehiclePawn* P);

		/** @returns if this move can be combined with NewMove to reduce bandwidth */
		virtual bool CanCombineWith(const FVehicleSavedMovePtr& NewMovePtr, ATP_VehiclePawn* P, float MaxDelta) const;

		/** Combines this move with an older move and update the state. Used to reduce bandwidth */
		virtual void CombineWith(const FSavedMove_Vehicle* OldMove, ATP_VehiclePawn* InVehicle, APlayerController* PC, const FVector& OldStartLocation);

		/** Called before ClientUpdatePosition to make a predictive correction */
		virtual void PrepMoveFor(ATP_VehiclePawn* P) {}; //Not used

		/** @returns a byte containing encoded flags for movement information */
		virtual uint8 GetCompressedFlags() const { return 0; }; //Good TODO add compressed flags

		// Custom Bit Masks used by Get CompressedFlags() to encode movement info 
		enum CompressedFlags
		{
			FLAG_Reserved = 0x01, // Reserved for future use 
			FLAG_Reserved_1 = 0x02, // Reserved for future use 
			FLAG_Reserved_2 = 0x04, // Reserved for future use 
			FLAG_Reserved_3 = 0x08, // Reserved for future use

			// Remaining bit masks are avaliable for custom use in extended objects 
			FLAG_Custom_0 = 0x10,
			FLAG_Custom_1 = 0x20,
			FLAG_Custom_2 = 0x40,
			FLAG_Custom_3 = 0x80,
		};
	};

	/** Movement adjustment for the local player pawn sent by the server */
	//Finished
	struct GDKSHOOTER_API FClientAdjustment_Vehicle
	{
	public:

		FClientAdjustment_Vehicle()
			: TimeStamp(0.f),
			DeltaTime(0.f),
			NewLoc(ForceInitToZero),
			NewRot(ForceInitToZero),
			NewLinear(ForceInitToZero),
			NewAngular(ForceInitToZero),
			bAckGoodMove(false)
		{}

		float TimeStamp;
		float DeltaTime;
		FVector NewLoc;
		FRotator NewRot;
		FVector NewLinear;
		FVector NewAngular;
		bool bAckGoodMove;
	};

	//Finished
	class GDKSHOOTER_API FNetPhysNetworkPredictionData_Client_Vehicle : public FNetworkPredictionData_Client, protected FNoncopyable
	{
	public:

		FNetPhysNetworkPredictionData_Client_Vehicle(const UNetPhysVehicleMovementComponent& ClientMovement);
		virtual ~FNetPhysNetworkPredictionData_Client_Vehicle();

		/** Client timestamp of last time it sent a servermove() to the server. Used for holding off on sending movement updates to save bandwidth. */
		float ClientUpdateTime;

		/** Current TimeStamp for sending new Moves to the server. */
		float CurrentTimeStamp;


		TArray<FVehicleSavedMovePtr> SavedMoves; // Oldest to Newest buffered moves that are pending updates on the client. Once they are acked by the server they are removed
		TArray<FVehicleSavedMovePtr> FreeMoves; // Moves that are free for buffering, used to avoid having to create new moves 
		FVehicleSavedMovePtr PendingMove; // A move the is pending combining with the next move
		FVehicleSavedMovePtr LastAckedMove; // Last move that the server acknowleged

		int32 MaxFreeMoveCount; // Limit on size of the free moves
		int32 MaxSavedMoveCount; // Size limit of how many moves we can save in the buffer

		uint32 bUpdatePosition : 1; // Opdate postion via ClientUpdatePosition

		/** Original location offset. Used for smoothing */
		FVector OriginalLocationOffset;

		/** World Space Offset to smooth the vehicle location. Target is for this Zero offset */
		FVector LocationOffset;

		/** Used for linear network smoothing */
		FQuat OriginalRotationOffset;

		/** Component space rotation offset fon the mesh */
		FQuat RotationOffset;

		/** Target rotation for smoothing */
		FQuat RotationTarget;

		/** At what time we received an update from the server */
		float LastCorrectionTime;

		/** How long time has elapsed since we received an update from the server */
		float LastCorrectionDelta;

		/*
		*Client world timestamp. Used for network smoothing. We increment this value when smoothing till we reach the 'ServerTimeStamp'
		* @see FNetPhysNetworkPredictionData_Client_Vehicle::ServerTimeStamp
		*/
		double ClientTimeStamp;

		/** Server timestamp of when it sent a move to the client */
		double ServerTimeStamp;

		/** How long to take to smoothly interpolate from the old pawn position on the client to the corrected one sent by the server.  Must be >= 0. Not used for linear smoothing. */
		float SmoothNetUpdateTime;

		/** How long to take to smoothly interpolate from the old pawn rotation on the client to the corrected one sent by the server.  Must be >= 0. Not used for linear smoothing. */
		float SmoothNetUpdateRotationTime;

		/**
		* Max delta time for a given move, in real seconds
		* Based off of AGameNetworkManager: : MaxMoveDeltaTime config setting, but can be modified per actor
		* if needed.
		* This value is mirrored in FNetworkPredictionData_Server, which is what server logic runs off of.
		* Client needs to know this in order to not send move deltas that are going to get clamped anyway (meaning
		* they'll be rejected/corrected).
		*/
		float MaxMoveDeltaTime;

		/** @returns a saved move with the given timestamp. Will return INDEX_NONE if it was not found since it may have been acked or cleared */
		int32 GetSavedMoveIndex(float TimeStamp) const;

		/** Ack a given move, will become LastAckedMove and removed from Saved Move */
		//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		void AckMove(int32 AckedMoveIndex) 
		{
			// It is important that we know the move exists before we go deleting outdated moves.
			// Timestamps are not guaranteed to be increasing order all the time, since they can be reset!
			if (AckedMoveIndex != INDEX_NONE)
			{
				// Keep reference to LastAckedMove
				const FVehicleSavedMovePtr& AckedMove = SavedMoves[AckedMoveIndex];
				UE_LOG(LogNetPlayerMovement, VeryVerbose, TEXT("AckedMove Index: %2d (%2d moves). TimeStamp: %f, CurrentTimeStamp: %f"), AckedMoveIndex, SavedMoves.Num(), AckedMove->TimeStamp, CurrentTimeStamp);
				if (LastAckedMove.IsValid())
				{
					FreeMove(LastAckedMove);
				}
				LastAckedMove = AckedMove;

				// Free expired moves.
				for (int32 MoveIndex = 0; MoveIndex < AckedMoveIndex; MoveIndex++)
				{
					const FVehicleSavedMovePtr& Move = SavedMoves[MoveIndex];
					FreeMove(Move);
				}

				// And finally cull all of those, so only the unacknowledged moves remain in SavedMoves.
				const bool bAllowShrinking = false;
				SavedMoves.RemoveAt(0, AckedMoveIndex + 1, bAllowShrinking);
			}

		};
		
		/** Allocates a new saved move */
		virtual FVehicleSavedMovePtr AllocateNewMove();

		/** Tries te pull a pooled move off the FreeMove list, otherwise we allocate a new moves. Wil return. NULL if the limit of moves has been reached */
		virtual FVehicleSavedMovePtr CreateSavedMove();

		/** Return a move to the free pool. Assumes that the given move will not be referenced by anything but possible the FreeMoves list */
		virtual void FreeMove(const FVehicleSavedMovePtr& Move);

		/** Update Current TimeStamp from the DeltaTime. Basically fixed accuracy with float points to match the server timestamp. */
		float UpdateTimeStampAndDeltaTime(float DeltaTime, ATP_VehiclePawn& VehicleOwner, class UNetPhysVehicleMovementComponent& VehicleMovementComponent);
	};

	//Finished
	class GDKSHOOTER_API FNetPhysNetworkPredictionData_Server_Vehicle : public FNetworkPredictionData_Server, protected FNoncopyable
	{
	public:
		FNetPhysNetworkPredictionData_Server_Vehicle(const UNetPhysVehicleMovementComponent& ServerMovement);
		virtual ~FNetPhysNetworkPredictionData_Server_Vehicle();

		/** Adjustment for the client */
		FClientAdjustment_Vehicle PendingAdjustment;

		/** Timestamp from the clients most recent ServerMove() call. Will reset occasionally to keep accuracy*/
		float CurrentClientTimeStamp;

		/** Timestamp of total elapsed client time. Similar to CurrentClientTimeStamp but this is accumulated with the calculated delta for each move */
		double ServerAccumulatedClientTimeStamp;

		/** Last time server updated client with a move Icorrection */
		float LastUpdateTime;

		/** Server clock time when last server move was received from client (non forced moves) */
		float ServerTimeStampLastServerMove;

		/**
		* Max delta time for a given move, in real seconds
		* Based off of AGameNetworkManager::MaxMoveDeltaTime config setting, but can be modified per actor if needed.
		*/
		float MaxMoveDeltaTime;

		/** Force client update on the next Server MoveHandleClientError() call. */
		uint32 bForceClientUpdate : 1;

		/** Accumulated timestamp difference between autonomous client and server for tracking long-term trends */
		float LifetimeRawTimeDiscrepancy;
		/**
		* Current time discrepancy between client-reported moves and time passed
		* on the server. Time discrepancy resolution's goal is to keep this near zero.
		*/
		float TimeDiscrepancy;

		/** True if currently in the process of resolving time discrepancy */
		bool bResolvingTimeDiscrepancy;

		/**
		* When bResolvingTimeDiscrepancy is true, we are in time discrepancy resolution mode whose output is
		* this value (to be used as the DeltaTime for current ServerMove)
		*/
		float TimeDiscrepancyResolutionMoveDeltaOverride;
		/**
		* When bResolvingTimeDiscrepancy is true, we are in time discrepancy resolution mode where we bound
		* move deltas by Server Deltas. In cases where there are multiple ServerMove RPCs handled within one
		* server frame tick, we need to accumulate the client deltas of the "no tick" Moves so that the next
		* Move processed that the server server has ticked for takes into account those previous deltas.
		* If we did not use this, the higher the framerate of a client vs the server results in more
		* punishment/payback time.
		*/
		float TimeDiscrepancyAccumulatedClientDeltasSinceLastServerTick;

		/** Creation time of this prediction data, used to contextualize LifetimeRawTimeDiscrepancy */
		float WorldCreationTime;

		/**
		* @return Time delta to use for the current ServerMove(). Takes into account time discrepancy resolution if active.
		*/
		float GetServerMoveDeltaTime(float ClientTimeStamp, float ActorTimeDilation) const {
			if (bResolvingTimeDiscrepancy)
			{
				return TimeDiscrepancyResolutionMoveDeltaOverride;
			}
			else
			{
				return GetBaseServerMoveDeltaTime(ClientTimeStamp, ActorTimeDilation);
			}
		};

		/**
		* @return Base time delta to use for a ServerMove, default calculation (no time discrepancy resolution)
		*/
		float GetBaseServerMoveDeltaTime(float ClientTimeStamp, float ActorTimeDilation) const {
			const float DeltaTime = FMath::Min(MaxMoveDeltaTime * ActorTimeDilation, ClientTimeStamp - CurrentClientTimeStamp);
			return DeltaTime;
		};
	};

