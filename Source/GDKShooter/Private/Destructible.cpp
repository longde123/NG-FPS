// Copyright (c) Improbable Worlds Ltd, All Rights Reserved


#include "Destructible.h"
#include "Components/BoxComponent.h"
#include "Engine/CollisionProfile.h"
#include <Runtime\Engine\Public\Net\UnrealNetwork.h>

// Sets default values
ADestructible::ADestructible()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	TestTag1 = CreateDefaultSubobject<UTestTag>(TEXT("TestTag1"));

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	HealthComponent->SetIsReplicated(true);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	RootComponent->SetIsReplicated(true);

	BuildMesh1 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BuildMesh1"));
	BuildMesh1->SetupAttachment(RootComponent);
	BuildMesh1->SetVisibility(false);
	BuildMesh1->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	BuildMesh1->SetIsReplicated(true);

	BuildMesh2 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BuildMesh2"));
	BuildMesh2->SetupAttachment(RootComponent);
	BuildMesh2->SetVisibility(false);
	BuildMesh2->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	BuildMesh2->SetIsReplicated(true);

	BuildMesh3 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BuildMesh3"));
	BuildMesh3->SetupAttachment(RootComponent);
	BuildMesh3->SetVisibility(true);
	BuildMesh3->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	BuildMesh3->SetIsReplicated(true);

	collision.Init(false, 3);
	collision[2] = true;

}

// Called when the game starts or when spawned
void ADestructible::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ADestructible::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ADestructible::OnRep_Collision() {
	if (collision[0]) {
		BuildMesh1->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}
	else {
		BuildMesh1->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	}

	if (collision[1]) {
		BuildMesh2->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}
	else {
		BuildMesh2->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	}

	if (collision[2]) {
		BuildMesh3->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}
	else {
		BuildMesh3->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	}
}

void ADestructible::HelathUpdate() {
	if (HealthComponent->GetCurrentHealth() >= 0.35 * HealthComponent->GetMaxHealth() && HealthComponent->GetCurrentHealth() <= 0.75 * HealthComponent->GetMaxHealth()) {
		BuildMesh2->SetVisibility(true);
		collision[1] = true;
		BuildMesh2->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

		BuildMesh1->SetVisibility(false);
		collision[0] = false;
		BuildMesh1->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

		BuildMesh3->SetVisibility(false);
		collision[2] = false;
		BuildMesh3->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	}
	else if (HealthComponent->GetCurrentHealth() > 0.75 * HealthComponent->GetMaxHealth()) {
		BuildMesh3->SetVisibility(true);
		collision[2] = true;
		BuildMesh3->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

		BuildMesh1->SetVisibility(false);
		collision[0] = false;
		BuildMesh1->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

		BuildMesh2->SetVisibility(false);
		collision[1] = false;
		BuildMesh2->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	}
	else if (HealthComponent->GetCurrentHealth() == 0) {
		this->Destroy();
	}
}

float ADestructible::TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	TakeDamageCrossServer(Damage, DamageEvent, EventInstigator, DamageCauser);
	return Damage;
}

void ADestructible::TakeDamageCrossServer_Implementation(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	float ActualDamage = Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);
	HealthComponent->TakeDamage(ActualDamage, DamageEvent, EventInstigator, DamageCauser);
	HelathUpdate();
}

void ADestructible::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADestructible, collision);
}

