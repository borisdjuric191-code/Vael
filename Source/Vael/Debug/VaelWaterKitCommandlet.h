// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "VaelWaterKitCommandlet.generated.h"

/**
 *  Checks the Niagara effects of the water kit headless: whether every water emitter renders with its particle material
 *  (sets the material where it is missing or wrong, nothing else) and whether the systems have the user parameter User.WaterColor.
 *  Run with: UnrealEditor-Cmd.exe Vael.uproject -run=VaelWaterKit [-DryRun] [-Dump]; -Dump also lists how the emitters in the systems are set up. The report goes to Saved/Logs/VaelWaterKit.txt.
 */
UCLASS()
class UVaelWaterKitCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:

	/** Constructor */
	UVaelWaterKitCommandlet();

	/** Checks and fixes the water effects */
	virtual int32 Main(const FString& Params) override;
};
