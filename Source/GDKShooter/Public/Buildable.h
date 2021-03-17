// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Characters/Components/HealthComponent.h"
#include "Components/BoxComponent.h"
#include "Buildable.generated.h"

UCLASS()
class GDKSHOOTER_API ABuildable : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ABuildable();

	UPROPERTY(Category = Character, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
		UHealthComponent *HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Building)
	UStaticMeshComponent *PreviewMesh1;


	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Building)
		UStaticMeshComponent *BuildMesh1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Building)
		UStaticMeshComponent *BuildMesh2;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Building)
		UStaticMeshComponent *BuildMesh3;

	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Building)
	class UBoxComponent *CollisionVolume1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Building)
		class UBoxComponent *CollisionVolume2;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Building)
		class UBoxComponent *CollisionVolume3;

	UFUNCTION(BlueprintCallable, Category = Building)
		void Place();

	UFUNCTION(BlueprintCallable, Category = Building)
		void Build(float value);


	// Called every frame
	virtual void Tick(float DeltaTime) override;

	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

};
