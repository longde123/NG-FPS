// Copyright (c) Improbable Worlds Ltd, All Rights Reserved


#include "BuildManagerComponent.h"
#include "Buildable.h"

// Sets default values for this component's properties
UBuildManagerComponent::UBuildManagerComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	isBuilding = false;
	canBuild = false;

	// ...
}


// Called when the game starts
void UBuildManagerComponent::BeginPlay()
{
	Super::BeginPlay();
	playerCamera = GetOwner()->FindComponentByClass<UCameraComponent>();
	if (playerCamera == nullptr) {
		//UE_LOG(LogTemp, Error, Text("Unable to find player camera"));
	}
	// ...
	
}


// Called every frame
void UBuildManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (isBuilding) {
		FHitResult HitResult;

		float LineTraceDistance = 1500.f;
		float HeadOffset = 150.f;

		FRotator CameraRotation = playerCamera->GetComponentRotation();
		FVector Start = playerCamera->GetComponentLocation() + (CameraRotation.Vector() * HeadOffset);

		FVector End = Start + (CameraRotation.Vector() * LineTraceDistance);

		FCollisionQueryParams TraceParams(FName(TEXT("InteractTrace")), true, NULL);
		TraceParams.bTraceComplex = true;
		TraceParams.bReturnPhysicalMaterial = true;

		bool bIsHit = GetWorld()->LineTraceSingleByChannel(
			HitResult,      // FHitResult object that will be populated with hit info
			Start,      // starting position
			End,        // end position
			ECC_GameTraceChannel3,  // collision channel - 3rd custom one
			TraceParams      // additional trace settings
		);

		if (bIsHit)
		{
			// start to end, green, will lines always stay on, depth priority, thickness of line
			DrawDebugLine(GetWorld(), Start, HitResult.ImpactPoint, FColor::Green, false, 5.f, ECC_WorldStatic, 1.f);

			FRotator ProperRotation = HitResult.GetComponent()->GetComponentRotation();
			ProperRotation.SetComponentForAxis(EAxis::Z, CameraRotation.GetComponentForAxis(EAxis::Z));

			FVector ProperLocation = HitResult.ImpactPoint;

			//DrawDebugBox(GetWorld(), HitResult.ImpactPoint, FVector(100, 100, 100), ProperRotation.Quaternion(), FColor::Red, false, 0, 0, 10);

			if (currentBuildable == nullptr) {
				currentBuildable = GetWorld()->SpawnActor<ABuildable>(ABuildable::StaticClass(), ProperLocation, ProperRotation);
				DrawDebugBox(GetWorld(), ProperLocation, FVector(100, 100, 100), ProperRotation.Quaternion(), FColor::Red, false, 0, 0, 10);
			}
			else {
				currentBuildable->SetActorLocationAndRotation(ProperLocation, ProperRotation);
			}
			canBuild = true;
		}
		else
		{
			// start to end, purple, will lines always stay on, depth priority, thickness of line
			DrawDebugLine(GetWorld(), Start, End, FColor::Purple, false, 5.f, ECC_WorldStatic, 1.f);
			canBuild = false;
		}
	}
	else if(currentBuildable != nullptr) {
		currentBuildable->Destroy();
		currentBuildable = nullptr;
	}

	// ...
}

void UBuildManagerComponent::ToggleBuildMode() {
	isBuilding = !isBuilding;
	GEngine->AddOnScreenDebugMessage(-1,15,FColor::Green,TEXT("ToogleBuildMode"));
}

void UBuildManagerComponent::RequestBuild() {
	if (canBuild&&isBuilding) {
		//GEngine->AddOnScreenDebugMessage(-1, 15, FColor::Green, TEXT("RequestBuild"));
		currentBuildable->Place();
		currentBuildable = nullptr;
	}
}

