// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "EngineSimulatorWheeledVehicleMovementComponent.generated.h"

class USoundWave;
class USoundWaveProcedural;

UCLASS()
class ENGINESIMULATORPLUGIN_API UEngineSimulatorWheeledVehicleMovementComponent : public UChaosWheeledVehicleMovementComponent
{
	GENERATED_UCLASS_BODY()

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Set the user input for gear up */
	UFUNCTION(BlueprintCallable, Category = "Game|Components|EngineSimulatorVehicleMovement")
	void SetEngineSimChangeGearUp(bool bNewGearUp);

	/** Set the user input for gear down */
	UFUNCTION(BlueprintCallable, Category = "Game|Components|EngineSimulatorVehicleMovement")
	void SetEngineSimChangeGearDown(bool bNewGearDown);

	UFUNCTION(BlueprintCallable, Category = "Game|Components|EngineSimulatorVehicleMovement")
	void SetEngineSimEnableDyno(bool bEnableDyno);

	UPROPERTY(BlueprintReadOnly, Category = "Engine Simulator Vehicle Component")
		bool bClutchIn = false;

	UPROPERTY(BlueprintReadOnly, Category = "Engine Simulator Vehicle Component")
		int32 CurrentGear = -1;

	UPROPERTY(EditDefaultsOnly, Category = "Engine Simulator Vehicle Movement")
		USoundWave* EngineSound;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Engine Simulator Vehicle Movement")
		USoundWaveProcedural* OutputEngineSound;

public:
	virtual TUniquePtr<Chaos::FSimpleWheeledVehicle> CreatePhysicsVehicle() override;
};