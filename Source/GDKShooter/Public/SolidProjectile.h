// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Weapons/Projectile.h"
#include "SolidProjectile.generated.h"

/**
 * 
 */
UCLASS()
class GDKSHOOTER_API ASolidProjectile : public AProjectile
{
	GENERATED_BODY()

	virtual void OnStop(const FHitResult& ImpactResult) override;

	UFUNCTION(BlueprintCallable, Category = "Damage")
	void DoDamage(const FHitResult& ImpactResult);
};
