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
	previewMode = true;

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

		float LineTraceDistance = 1800.f;
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
			ECC_Visibility,  // collision channel
			TraceParams      // additional trace settings
		);

		currentTrace = HitResult.ImpactPoint;

		//DrawPreview
		if (bIsHit&&previewMode){
			// start to end, green, will lines always stay on, depth priority, thickness of line
			//DrawDebugLine(GetWorld(), Start, HitResult.ImpactPoint, FColor::Green, false, 5.f, ECC_WorldStatic, 1.f);

			FRotator ProperRotation = HitResult.GetComponent()->GetComponentRotation();
			ProperRotation.SetComponentForAxis(EAxis::Z, CameraRotation.GetComponentForAxis(EAxis::Z));

			FVector ProperLocation = HitResult.ImpactPoint;
			

			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

			if (currentBuildable == nullptr) {
				currentBuildable = GetWorld()->SpawnActor<ABuildable>(BuildableFortification, ProperLocation, ProperRotation, SpawnParams);
				//DrawDebugBox(GetWorld(), ProperLocation, FVector(100, 100, 100), ProperRotation.Quaternion(), FColor::Red, false, 0, 0, 10);
			}
			else {
				currentBuildable->SetActorLocationAndRotation(ProperLocation, ProperRotation);
			}
			canBuild = true;
		}else if (bIsHit&&!previewMode){
			FVector distanceCalculator = plantingPoint - currentTrace;
			DrawDebugLine(GetWorld(), plantingPoint, currentTrace, FColor::Green, false, 5.f, ECC_WorldStatic, 1.f);
			float distance = distanceCalculator.Size();
			
			

			FString TheFloatStr = FString::SanitizeFloat(distance);
			GEngine->AddOnScreenDebugMessage(-1, 15, FColor::Green, *TheFloatStr);



			//Get buildable lenght
			FVector debugSize = currentBuildable->GetComponentsBoundingBox(true,true).GetExtent();
			DrawDebugBox(GetWorld(), currentBuildable->GetTargetLocation(), debugSize, currentBuildable->GetActorRotation().Quaternion(), FColor::Red, false, 0, 0, 10);
			
			//Divide distance between planting point and currentTrace by buildable lenght
			int amountOfCover = (distance / debugSize.Size());
			
			
			TheFloatStr = FString::FromInt(amountOfCover);
			GEngine->AddOnScreenDebugMessage(-1, 15, FColor::Cyan, TheFloatStr);
			
			//for each full distance spawn a buidable and add it to managedBuildables
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

			for (int i = 0; i < amountOfCover; i++) {
				FVector additiveVector = plantingPoint + (distanceCalculator * debugSize.Size() * (-1) * i);
				managedBuildables.Add(GetWorld()->SpawnActor<ABuildable>(BuildableFortification, additiveVector, distanceCalculator.Rotation(), SpawnParams));
			}

			if (managedBuildables.Num() > amountOfCover) {
				for (int i = amountOfCover; i < managedBuildables.Num(); i++) {
					managedBuildables[i]->Destroy();
					if (!managedBuildables[i]->IsPendingKill()) {
						managedBuildables[i] = nullptr;
					}
				}
			}


		}
		else if (!bIsHit) {
			canBuild = false;
		}
	}
	else if(currentBuildable != nullptr) {
		currentBuildable->Destroy();
		currentBuildable = nullptr;
	}

}

void UBuildManagerComponent::ToggleBuildMode() {
	isBuilding = !isBuilding;
	GEngine->AddOnScreenDebugMessage(-1,15,FColor::Green,TEXT("ToogleBuildMode"));
}

void UBuildManagerComponent::RequestBuild() {
	if (canBuild) {
		GEngine->AddOnScreenDebugMessage(-1, 15, FColor::Green, TEXT("RequestBuild"));
		previewMode = false;
		plantingPoint = currentTrace;
	}
}

void UBuildManagerComponent::ReleaseBuild(){
	currentBuildable->Destroy();
	currentBuildable = nullptr;


	if (canBuild && isBuilding) {
		for (int i = 0; i < managedBuildables.Num(); i++) {
			managedBuildables[i]->Destroy();
		}
	}
	managedBuildables.Empty();

	/*

	if (canBuild && isBuilding) {
		GEngine->AddOnScreenDebugMessage(-1, 15, FColor::Green, TEXT("ReleaseBuild"));

		for (int i = 0; i < managedBuildables.Num(); i++) {
			managedBuildables[i]->Place();
		}
		managedBuildables.Empty();
	}
	*/
	previewMode = true;
}
