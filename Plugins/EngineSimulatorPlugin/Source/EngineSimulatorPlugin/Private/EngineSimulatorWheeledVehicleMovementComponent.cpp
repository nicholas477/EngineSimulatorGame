// Fill out your copyright notice in the Description page of Project Settings.

#include "EngineSimulatorWheeledVehicleMovementComponent.h"
#include "EngineSimulatorWheeledVehicleSimulation.h"
#include "ChaosVehicleMovementComponent.h"
#include "VehicleUtility.h"
#include "Sound/SoundWaveProcedural.h"
#include "ChaosVehicleManager.h"
#include "EngineSimulator.h"

UEngineSimulatorWheeledVehicleMovementComponent::UEngineSimulatorWheeledVehicleMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;

	OutputEngineSound = CreateDefaultSubobject<USoundWaveProcedural>(FName("Engine Sound Output"));
	OutputEngineSound->SetSampleRate(44000);
	OutputEngineSound->NumChannels = 1;
	OutputEngineSound->Duration = INDEFINITELY_LOOPING_DURATION;
	OutputEngineSound->SoundGroup = SOUNDGROUP_Default;
	OutputEngineSound->bLooping = false;
}

void UEngineSimulatorWheeledVehicleMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (VehicleSimulationPT.Get())
	{
		UEngineSimulatorWheeledVehicleSimulation* VS = ((UEngineSimulatorWheeledVehicleSimulation*)VehicleSimulationPT.Get());
		VS->PrintDebugInfo(GetWorld());
		LastEngineSimulatorOutput = VS->GetLastOutput();
	}
}

void UEngineSimulatorWheeledVehicleMovementComponent::SetEngineSimChangeGearUp(bool bNewGearUp)
{
	if (VehicleSimulationPT && bNewGearUp)
	{
		if (CurrentGear != FMath::Clamp(CurrentGear + 1, -1, LastEngineSimulatorOutput.NumGears - 1))
		{
			CurrentGear = FMath::Clamp(CurrentGear + 1, -1, LastEngineSimulatorOutput.NumGears - 1);
			((UEngineSimulatorWheeledVehicleSimulation*)VehicleSimulationPT.Get())->AsyncUpdateSimulation([=](IEngineSimulatorInterface* EngineInterface)
			{
				EngineInterface->SetGear(CurrentGear);
				GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, FString::Printf(TEXT("Shifting gear up to: %d"), CurrentGear + 1));
			});
		}
	}
}

void UEngineSimulatorWheeledVehicleMovementComponent::SetEngineSimChangeGearDown(bool bNewGearDown)
{
	if (VehicleSimulationPT && bNewGearDown)
	{
		CurrentGear = FMath::Clamp(CurrentGear - 1, -1, LastEngineSimulatorOutput.NumGears - 1);
		((UEngineSimulatorWheeledVehicleSimulation*)VehicleSimulationPT.Get())->AsyncUpdateSimulation([=](IEngineSimulatorInterface* EngineInterface)
		{
			EngineInterface->SetGear(CurrentGear);

			if (CurrentGear != -1)
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, FString::Printf(TEXT("Downshifted to: %d"), CurrentGear + 1));
			}
			else
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Shifting to neutral"));
			}
		});
	}
}

void UEngineSimulatorWheeledVehicleMovementComponent::SetEngineSimEnableDyno(bool bEnableDyno)
{
	bClutchIn = !bEnableDyno;
	if (VehicleSimulationPT)
	{
		((UEngineSimulatorWheeledVehicleSimulation*)VehicleSimulationPT.Get())->AsyncUpdateSimulation([bEnableDyno](IEngineSimulatorInterface* EngineInterface)
		{
			EngineInterface->SetDynoEnabled(bEnableDyno);

			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, FString::Printf(TEXT("Enabled dyno: %s"), bEnableDyno ? TEXT("True") : TEXT("False")));
		});
	}
}

TUniquePtr<Chaos::FSimpleWheeledVehicle> UEngineSimulatorWheeledVehicleMovementComponent::CreatePhysicsVehicle() 
{
	// Make the Vehicle Simulation class that will be updated from the physics thread async callback
	VehicleSimulationPT = MakeUnique<UEngineSimulatorWheeledVehicleSimulation>(Wheels, EngineSound, OutputEngineSound);

	((UEngineSimulatorWheeledVehicleSimulation*)VehicleSimulationPT.Get())->AsyncUpdateSimulation([](IEngineSimulatorInterface* EngineInterface)
	{
		UE_LOG(LogTemp, Warning, TEXT("Enabling dyno, ignition, starter"));
		EngineInterface->SetDynoEnabled(true);
		EngineInterface->SetIgnitionEnabled(true);
		EngineInterface->SetStarterEnabled(true);
	});

	return UChaosVehicleMovementComponent::CreatePhysicsVehicle();
}
