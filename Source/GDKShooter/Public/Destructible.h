// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Characters/Components/HealthComponent.h"
#include "TestTag.h"
#include "Destructible.generated.h"

UCLASS()
class GDKSHOOTER_API ADestructible : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ADestructible();

	UPROPERTY(Category = Character, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
		UHealthComponent* HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Building)
		UStaticMeshComponent* BuildMesh1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Building)
		UStaticMeshComponent* BuildMesh2;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Building)
		UStaticMeshComponent* BuildMesh3;

	UPROPERTY(ReplicatedUsing = OnRep_Collision)
		TArray<bool> collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Building)
		class UTestTag* TestTag1;

	UFUNCTION()
		void OnRep_Collision();

	UFUNCTION(BlueprintCallable, Category = Health)
		void HelathUpdate();


protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	float TakeDamage(float Damage, const struct FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;


	UFUNCTION(CrossServer, Reliable)
		void TakeDamageCrossServer(float Damage, const struct FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser);


};
