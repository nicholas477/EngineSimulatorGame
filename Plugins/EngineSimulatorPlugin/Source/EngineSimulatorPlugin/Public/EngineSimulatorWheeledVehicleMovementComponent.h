// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "EngineSimulatorWheeledVehicleMovementComponent.generated.h"

class IEngineSimulatorInterface;

class UEngineSimulatorWheeledVehicleSimulation : public UChaosWheeledVehicleSimulation
{
public:
	UEngineSimulatorWheeledVehicleSimulation(TArray<class UChaosVehicleWheel*>& WheelsIn, class USoundWave* EngineSound, class USoundWaveProcedural* OutputEngineSound);

	/** Update the engine/transmission simulation */
	virtual void ProcessMechanicalSimulation(float DeltaTime) override;

	/** Pass control Input to the vehicle systems */
	virtual void ApplyInput(const FControlInputs& ControlInputs, float DeltaTime) override;

	void AsyncUpdateSimulation(TFunction<void(IEngineSimulatorInterface*)> InCallable);

	void PrintDebugInfo();

protected:
	TUniquePtr<IEngineSimulatorInterface> EngineSimulator;
	TQueue<TFunction<void(IEngineSimulatorInterface*)>, EQueueMode::Mpsc> UpdateQueue;

	// Debug info stuff
	float Torque;
	float RPM;
	float Speed;
	bool bStarterEnabled;
	bool bDynoEnabled;
	bool bIgnitionEnabled;
};

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

	UPROPERTY(EditDefaultsOnly, Category = "Engine Simulator Vehicle Movement")
		USoundWave* EngineSound;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Engine Simulator Vehicle Movement")
		USoundWaveProcedural* OutputEngineSound;

public:
	virtual TUniquePtr<Chaos::FSimpleWheeledVehicle> CreatePhysicsVehicle() override;
};