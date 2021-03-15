// Copyright (c) Improbable Worlds Ltd, All Rights Reserved


#include "BuildManagerComponent.h"

// Sets default values for this component's properties
UBuildManagerComponent::UBuildManagerComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	isBuilding = false;

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
		FVector location = getNextBuildLocation();
		FRotator rotation = getNextBuildRotation();

		DrawDebugBox(GetWorld(), location, FVector(100, 100, 100), rotation.Quaternion(), FColor::Red, false, 0, 0, 10);
	}

	// ...
}

void UBuildManagerComponent::ToggleBuildMode() {
	isBuilding = !isBuilding;
	GEngine->AddOnScreenDebugMessage(-1,15,FColor::Green,TEXT("ToogleBuildMode"));
}

void UBuildManagerComponent::RequestBuild() {
	GEngine->AddOnScreenDebugMessage(-1, 15, FColor::Green, TEXT("RequestBuild"));
}

FVector UBuildManagerComponent::getNextBuildLocation() const{
	FHitResult HitResult;

	float LineTraceDistance = 1000.f;

	FVector Start = playerCamera->GetComponentLocation()+50.f;
	FRotator Rotation = playerCamera->GetComponentRotation();

	FVector End = Start + (Rotation.Vector() * LineTraceDistance);
	
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
		//GEngine->AddOnScreenDebugMessage(-1, 15, FColor::Green, TEXT("Hit"));
		// start to end, green, will lines always stay on, depth priority, thickness of line
		DrawDebugLine(GetWorld(), Start, HitResult.ImpactPoint, FColor::Green, false, 5.f, ECC_WorldStatic, 1.f);
		DrawDebugBox(GetWorld(), HitResult.ImpactPoint, FVector(100, 100, 100), Rotation.Quaternion(), FColor::Red, false, 0, 0, 10);
		GEngine->AddOnScreenDebugMessage(-1, 15, FColor::Green, FString::SanitizeFloat(HitResult.Distance));
		return HitResult.ImpactPoint;
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 15, FColor::Green, TEXT("Not hit"));
		// start to end, purple, will lines always stay on, depth priority, thickness of line
		DrawDebugLine(GetWorld(), Start, End, FColor::Purple, false, 5.f, ECC_WorldStatic, 1.f);
		return FVector(0, 0, 0);
	}
}

FRotator UBuildManagerComponent::getNextBuildRotation() const{
	return playerCamera->GetComponentRotation();
}
