// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "MyStaticMeshActor.generated.h"

/**
 * 
 */
UCLASS()
class GDKSHOOTER_API AMyStaticMeshActor : public AStaticMeshActor
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable)
	void ProfileLock();

	UFUNCTION(BlueprintCallable)
	void ProfileUnlock();
};
