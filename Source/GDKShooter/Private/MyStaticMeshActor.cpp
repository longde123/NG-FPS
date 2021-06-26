// Copyright (c) Improbable Worlds Ltd, All Rights Reserved


#include "MyStaticMeshActor.h"

void AMyStaticMeshActor::ProfileLock() {
	UStaticMeshComponent* Comp = GetStaticMeshComponent();
	FBodyInstance* Body = Comp->GetBodyInstance();
	if (Comp && Body) {
		Body->bLockXRotation = true;
		Body->bLockYRotation = true;
		Body->bLockZRotation = true;
		Body->bLockXTranslation = true;
		Body->bLockYTranslation = true;
		Body->bLockZTranslation = false;
	}
}

void AMyStaticMeshActor::ProfileUnlock() {
	UStaticMeshComponent* Comp = GetStaticMeshComponent();
	FBodyInstance* Body = Comp->GetBodyInstance();
	if (Comp && Body) {
		Body->bLockXRotation = false;
		Body->bLockYRotation = false;
		Body->bLockZRotation = false;
		Body->bLockXTranslation = false;
		Body->bLockYTranslation = false;
		Body->bLockZTranslation = false;
	}
}
