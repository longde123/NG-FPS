// Copyright (c) Improbable Worlds Ltd, All Rights Reserved


#include "TP_V3.h"
#include "CustomWheeledVehicle.h"
#include "TP_VehicleHud.h"

ATP_V3::ATP_V3()
{
	DefaultPawnClass = ACustomWheeledVehicle::StaticClass();
	HUDClass = ATP_VehicleHud::StaticClass();
}
