// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Character.h"

/**
 * 
 */
class GDKSHOOTER_API Player_AI_Tracker
{
public:
	Player_AI_Tracker();
	TMap<APlayerState*, TArray<ACharacter>> Tracker;
};
