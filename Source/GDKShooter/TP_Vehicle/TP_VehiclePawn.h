// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehicle.h"
#include "NetPhysVehicleMovementComponent.h"
#include "TP_VehiclePawn.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UTextRenderComponent;
class UInputComponent;
class UNetPhysVehicleMovementComponent;


UCLASS(config=Game)
class ATP_VehiclePawn : public AWheeledVehicle
{
	GENERATED_BODY()

	/** Spring arm that will offset the camera */
	UPROPERTY(Category = Camera, VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* SpringArm;

	/** Camera component that will be our viewpoint */
	UPROPERTY(Category = Camera, VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* Camera;

	/** SCene component for the In-Car view origin */
	UPROPERTY(Category = Camera, VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class USceneComponent* InternalCameraBase;

	/** Camera component for the In-Car view */
	UPROPERTY(Category = Camera, VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* InternalCamera;

	/** Text component for the In-Car speed */
	UPROPERTY(Category = Display, VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UTextRenderComponent* InCarSpeed;

	/** Text component for the In-Car gear */
	UPROPERTY(Category = Display, VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UTextRenderComponent* InCarGear;

	
public:
	ATP_VehiclePawn();

	UPROPERTY()
		class UNetPhysVehicleMovementComponent* NetVehicleMovement;

	

	FCollisionShape GetPawnCollisionShape() 
	{
		return GetMesh()->GetCollisionShape();
	};

	UFUNCTION(Server, Unreliable, WithValidation)
		void ServerMove(float TimeStamp, FVector_NetQuantize100 Location, uint8 Flags, uint8 Roll, uint32 View);
		void ServerMove_Implementation(float TimeStamp, FVector_NetQuantize100 Location, uint8 Flags, uint8 Roll, uint32 View);
		bool ServerMove_Validate(float TimeStamp, FVector_NetQuantize100 Location, uint8 Flags, uint8 Roll, uint32 View);

	UFUNCTION(Server, Unreliable, WithValidation)
		void ServerMoveDual(float TimeStamp0, uint8 PendingFlags, uint32 View0, float TimeStamp, FVector_NetQuantize100 Location, uint8 NewFlags, uint8 Roll, uint32 View);
		void ServerMoveDual_Implementation(float TimeStamp0, uint8 PendingFlags, uint32 View0, float TimeStamp, FVector_NetQuantize100 Location, uint8 NewFlags, uint8 Roll, uint32 View);
		bool ServerMoveDual_Validate(float TimeStamp0, uint8 PendingFlags, uint32 View0, float TimeStamp, FVector_NetQuantize100 Location, uint8 NewFlags, uint8 Roll, uint32 View);

	UFUNCTION(Server, Unreliable, WithValidation)
		void ServerMoveOld(float OldTimeStamp, uint8 OldFlags);
		void ServerMoveOld_Implementation(float OldTimeStamp, uint8 OldFlags);
		bool ServerMoveOld_Validate(float OldTimeStamp, uint8 OldFlags);

	UFUNCTION(Client, Unreliable, WithValidation)
		void ClientVeryShortAdjustPosition(float TimeStamp, FVector NewLoc);
		void ClientVeryShortAdjustPosition_Implementation(float TimeStamp, FVector NewLoc);
		bool ClientVeryShortAdjustPosition_Validate(float TimeStamp, FVector NewLoc);

	UFUNCTION(Client, Unreliable, WithValidation)
		void ClientAdjustPosition(float TimeStamp, FVector NewLoc, FVector NewLinear, FVector NewAngular);
		void ClientAdjustPosition_Implementation(float TimeStamp, FVector NewLoc, FVector NewLinear, FVector NewAngular);
		bool ClientAdjustPosition_Validate(float TimeStamp, FVector NewLoc, FVector NewLinear, FVector NewAngular);

	UFUNCTION(unreliable, client)
		void ClientAckGoodMove(float TimeStamp);
		void ClientAckGoodMove_Implementation(float TimeStamp);

	/** The current speed as a string eg 10 km/h */
	UPROPERTY(Category = Display, VisibleDefaultsOnly, BlueprintReadOnly)
	FText SpeedDisplayString;

	/** The current gear as a string (R,N, 1,2 etc) */
	UPROPERTY(Category = Display, VisibleDefaultsOnly, BlueprintReadOnly)
	FText GearDisplayString;

	UPROPERTY(Category = Display, VisibleDefaultsOnly, BlueprintReadOnly)
	/** The color of the incar gear text in forward gears */
	FColor	GearDisplayColor;

	/** The color of the incar gear text when in reverse */
	UPROPERTY(Category = Display, VisibleDefaultsOnly, BlueprintReadOnly)
	FColor	GearDisplayReverseColor;

	/** Are we using incar camera */
	UPROPERTY(Category = Camera, VisibleDefaultsOnly, BlueprintReadOnly)
	bool bInCarCameraActive;

	/** Are we in reverse gear */
	UPROPERTY(Category = Camera, VisibleDefaultsOnly, BlueprintReadOnly)
	bool bInReverseGear;

	/** Initial offset of incar camera */
	FVector InternalCameraOrigin;
	// Begin Pawn interface
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	// End Pawn interface



	// Begin Actor interface
	virtual void Tick(float Delta) override;
protected:
	virtual void BeginPlay() override;
	virtual void OnRep_ReplicatedMovement() override;
	virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;
	virtual void PostNetReceiveLocationAndRotation() override;

public:
	// End Actor interface

	/** Handle pressing forwards */
	void MoveForward(float Val);
	/** Setup the strings used on the hud */
	void SetupInCarHUD();

	/** Update the physics material used by the vehicle mesh */
	void UpdatePhysicsMaterial();
	/** Handle pressing right */
	void MoveRight(float Val);
	/** Handle handbrake pressed */
	void OnHandbrakePressed();
	/** Handle handbrake released */
	void OnHandbrakeReleased();
	/** Switch between cameras */
	void OnToggleCamera();
	/** Handle reset VR device */
	void OnResetVR();

	static const FName LookUpBinding;
	static const FName LookRightBinding;

private:
	/** 
	 * Activate In-Car camera. Enable camera and sets visibility of incar hud display
	 *
	 * @param	bState true will enable in car view and set visibility of various if its doesnt match new state
	 * @param	bForce true will force to always change state
	 */
	void EnableIncarView( const bool bState, const bool bForce = false );

	/** Update the gear and speed strings */
	void UpdateHUDStrings();

	/* Are we on a 'slippery' surface */
	bool bIsLowFriction;


public:
	/** Returns SpringArm subobject **/
	FORCEINLINE USpringArmComponent* GetSpringArm() const { return SpringArm; }
	/** Returns Camera subobject **/
	FORCEINLINE UCameraComponent* GetCamera() const { return Camera; }
	/** Returns InternalCamera subobject **/
	FORCEINLINE UCameraComponent* GetInternalCamera() const { return InternalCamera; }
	/** Returns InCarSpeed subobject **/
	FORCEINLINE UTextRenderComponent* GetInCarSpeed() const { return InCarSpeed; }
	/** Returns InCarGear subobject **/
	FORCEINLINE UTextRenderComponent* GetInCarGear() const { return InCarGear; }
};
