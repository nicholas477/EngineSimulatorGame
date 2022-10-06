// Fill out your copyright notice in the Description page of Project Settings.

#include "EngineSimulatorWheeledVehicleMovementComponent.h"
#include "ChaosVehicleMovementComponent.h"
#include "EngineSimulator.h"
#include "VehicleUtility.h"
#include "Sound/SoundWaveProcedural.h"

UEngineSimulatorWheeledVehicleSimulation::UEngineSimulatorWheeledVehicleSimulation(TArray<class UChaosVehicleWheel*>& WheelsIn, USoundWave* EngineSound, class USoundWaveProcedural* OutputEngineSound)
	: UChaosWheeledVehicleSimulation(WheelsIn)
{
	EngineSimulator = CreateEngine(EngineSound, OutputEngineSound);
}

void UEngineSimulatorWheeledVehicleSimulation::ProcessMechanicalSimulation(float DeltaTime)
{
	//if (PVehicle->HasEngine())
	{
		auto& PEngine = PVehicle->GetEngine();
		auto& PTransmission = PVehicle->GetTransmission();
		auto& PDifferential = PVehicle->GetDifferential();

		float WheelRPM = 0;
		for (int I = 0; I < PVehicle->Wheels.Num(); I++)
		{
			if (PVehicle->Wheels[I].EngineEnabled)
			{
				WheelRPM = FMath::Abs(PVehicle->Wheels[I].GetWheelRPM());
			}
		}

		float WheelSpeedRPM = FMath::Abs(PTransmission.GetEngineRPMFromWheelRPM(WheelRPM));
		PEngine.SetEngineRPM(PTransmission.IsOutOfGear(), PTransmission.GetEngineRPMFromWheelRPM(WheelRPM));
		//PEngine.Simulate(DeltaTime);

		TFunction<void(IEngineSimulatorInterface*)> SimulationUpdate;
		while (UpdateQueue.Dequeue(SimulationUpdate))
		{
			SimulationUpdate(EngineSimulator.Get());
		}

		EngineSimulator->Simulate(DeltaTime);

		PTransmission.SetEngineRPM(PEngine.GetEngineRPM()); // needs engine RPM to decide when to change gear (automatic gearbox)
		PTransmission.SetAllowedToChangeGear(!VehicleState.bVehicleInAir && !IsWheelSpinning());
		float GearRatio = PTransmission.GetGearRatio(PTransmission.GetCurrentGear());

		//PTransmission.Simulate(DeltaTime);

		float TransmissionTorque = EngineSimulator->GetFilteredDynoTorque() * EngineSimulator->GetGearRatio() * PTransmission.Setup().FinalDriveRatio;

		//TransmissionTorque = PTransmission.GetTransmissionTorque(PEngine.GetEngineTorque());
		if (WheelSpeedRPM > PEngine.Setup().MaxRPM)
		{
			TransmissionTorque = 0.f;
		}

		Torque = TransmissionTorque;
		RPM = EngineSimulator->GetRPM();
		Speed = EngineSimulator->GetSpeed();

		// apply drive torque to wheels
		for (int WheelIdx = 0; WheelIdx < Wheels.Num(); WheelIdx++)
		{
			auto& PWheel = PVehicle->Wheels[WheelIdx];
			if (PWheel.Setup().EngineEnabled)
			{
				PWheel.SetDriveTorque(Chaos::TorqueMToCm(TransmissionTorque) * PWheel.Setup().TorqueRatio);
			}
			else
			{
				PWheel.SetDriveTorque(0.f);
			}
		}

	}
}

void UEngineSimulatorWheeledVehicleSimulation::ApplyInput(const FControlInputs& ControlInputs, float DeltaTime)
{
	UChaosWheeledVehicleSimulation::ApplyInput(ControlInputs, DeltaTime);

	FControlInputs ModifiedInputs = ControlInputs;

	//PEngine.SetThrottle(ModifiedInputs.ThrottleInput * ModifiedInputs.ThrottleInput);
	AsyncUpdateSimulation([ThrottleInput = ModifiedInputs.ThrottleInput](IEngineSimulatorInterface* EngineInterface)
	{
		//UE_LOG(LogTemp, Warning, TEXT("Setting engine speed: %f"), ThrottleInput * ThrottleInput);
		EngineInterface->SetSpeedControl(ThrottleInput * ThrottleInput);
	});

	//if (ModifiedInputs.GearUpInput)
	//{
	//	AsyncUpdateSimulation([ThrottleInput = ModifiedInputs.ThrottleInput](IEngineSimulatorInterface* EngineInterface)
	//	{
	//		EngineInterface->SetGear(EngineInterface->GetGear() + 1);
	//		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, FString::Printf(TEXT("Shifting gear up to: %d"), EngineInterface->GetGear() + 1));
	//	});
	//	ModifiedInputs.GearUpInput = false;
	//}

	//if (ModifiedInputs.GearDownInput)
	//{
	//	AsyncUpdateSimulation([ThrottleInput = ModifiedInputs.ThrottleInput](IEngineSimulatorInterface* EngineInterface)
	//	{
	//		EngineInterface->SetGear(EngineInterface->GetGear() - 1);

	//		if (EngineInterface->GetGear() != -1)
	//		{
	//			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, FString::Printf(TEXT("Downshifted to: %d"), EngineInterface->GetGear() + 1));
	//		}
	//		else
	//		{
	//			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Shifting to neutral"));
	//		}
	//	});
	//	ModifiedInputs.GearDownInput = false;
	//}
}

void UEngineSimulatorWheeledVehicleSimulation::AsyncUpdateSimulation(TFunction<void(IEngineSimulatorInterface*)> InCallable)
{
	UpdateQueue.Enqueue(InCallable);
}

void UEngineSimulatorWheeledVehicleSimulation::PrintDebugInfo()
{
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Red, FString::Printf(TEXT("Simulation Tranmission torque: %f"), Torque));
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Red, FString::Printf(TEXT("Simulation RPM: %f"), RPM));
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Red, FString::Printf(TEXT("Simulation Speed: %f"), Speed));
}

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
		((UEngineSimulatorWheeledVehicleSimulation*)VehicleSimulationPT.Get())->PrintDebugInfo();
	}
}

void UEngineSimulatorWheeledVehicleMovementComponent::SetEngineSimChangeGearUp(bool bNewGearUp)
{
	if (VehicleSimulationPT && bNewGearUp)
	{
		((UEngineSimulatorWheeledVehicleSimulation*)VehicleSimulationPT.Get())->AsyncUpdateSimulation([](IEngineSimulatorInterface* EngineInterface)
		{
			EngineInterface->SetGear(EngineInterface->GetGear() + 1);
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, FString::Printf(TEXT("Shifting gear up to: %d"), EngineInterface->GetGear() + 1));
		});
	}
}

void UEngineSimulatorWheeledVehicleMovementComponent::SetEngineSimChangeGearDown(bool bNewGearDown)
{
	if (VehicleSimulationPT && bNewGearDown)
	{
		((UEngineSimulatorWheeledVehicleSimulation*)VehicleSimulationPT.Get())->AsyncUpdateSimulation([](IEngineSimulatorInterface* EngineInterface)
		{
			EngineInterface->SetGear(EngineInterface->GetGear() - 1);

			if (EngineInterface->GetGear() != -1)
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, FString::Printf(TEXT("Downshifted to: %d"), EngineInterface->GetGear() + 1));
			}
			else
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Shifting to neutral"));
			}
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
