// Copyright (c) Improbable Worlds Ltd, All Rights Reserved


#include "SolidProjectile.h"

void ASolidProjectile::OnStop(const FHitResult& ImpactResult)
{
	if (!HasAuthority())
	{
		return;
	}

	
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("SolidProjectile!"));

		//Custom Take damage event test
		FPointDamageEvent DmgEvent;
		DmgEvent.DamageTypeClass = DamageTypeClass;
		DmgEvent.HitInfo.ImpactPoint = ImpactResult.Location;

		ImpactResult.Actor->TakeDamage(ExplosionDamage, DmgEvent, InstigatingController, this);
	
}

void ASolidProjectile::DoDamage(const FHitResult& ImpactResult)
{
	if (!HasAuthority())
	{
		return;
	}

	//GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("SolidProjectile!"));

	//Custom Take damage event test
	FPointDamageEvent DmgEvent;
	DmgEvent.DamageTypeClass = DamageTypeClass;
	DmgEvent.HitInfo.ImpactPoint = ImpactResult.Location;

	ImpactResult.Actor->TakeDamage(ExplosionDamage, DmgEvent, InstigatingController, this);
}
