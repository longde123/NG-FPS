// Copyright (c) Improbable Worlds Ltd, All Rights Reserved


#include "Buildable.h"
#include "Components/BoxComponent.h"
#include "Engine/CollisionProfile.h"

// Sets default values
ABuildable::ABuildable()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));

	PreviewMesh1 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewMesh"));
	PreviewMesh1->SetupAttachment(RootComponent);


	BuildMesh1 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BuildMesh1"));
	BuildMesh1->SetupAttachment(RootComponent);
	BuildMesh1->SetVisibility(false);

	BuildMesh2 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BuildMesh2"));
	BuildMesh2->SetupAttachment(RootComponent);
	BuildMesh2->SetVisibility(false);

	BuildMesh3 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BuildMesh3"));
	BuildMesh3->SetupAttachment(RootComponent);
	BuildMesh3->SetVisibility(false);


	CollisionVolume1 = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionVolume1"));
	CollisionVolume1->SetupAttachment(RootComponent);
	CollisionVolume1->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	CollisionVolume2 = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionVolume2"));
	CollisionVolume2->SetupAttachment(RootComponent);
	CollisionVolume2->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	CollisionVolume3 = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionVolume3"));
	CollisionVolume3->SetupAttachment(RootComponent);
	CollisionVolume3->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
}

// Called when the game starts or when spawned
void ABuildable::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ABuildable::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ABuildable::Place() {
	BuildMesh1->SetVisibility(true);
	CollisionVolume1->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	HealthComponent->GrantHealth((1 / 10) * HealthComponent->GetMaxHealth());
}

void ABuildable::Build(float value) {
	HealthComponent->GrantHealth(value);
	if (HealthComponent->GetCurrentHealth() >= 0.35 * HealthComponent->GetMaxHealth() && HealthComponent->GetCurrentHealth() <= 0.75 * HealthComponent->GetMaxHealth()) {
		BuildMesh2->SetVisibility(true);
		CollisionVolume2->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

		BuildMesh1->SetVisibility(false);
		CollisionVolume1->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	}else if (HealthComponent->GetCurrentHealth() > 0.75 * HealthComponent->GetMaxHealth()) {
		BuildMesh3->SetVisibility(true);
		CollisionVolume3->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

		BuildMesh1->SetVisibility(false);
		CollisionVolume1->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

		BuildMesh2->SetVisibility(false);
		CollisionVolume2->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	}
}

