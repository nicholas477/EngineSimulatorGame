// Fill out your copyright notice in the Description page of Project Settings.

#include "EngineSimulatorWheeledVehicleSimulation.h"
#include "EngineSimulatorWheeledVehicleMovementComponent.h"
#include "ChaosVehicleMovementComponent.h"
#include "EngineSimulator.h"
#include "VehicleUtility.h"
#include "Sound/SoundWaveProcedural.h"
#include "ChaosVehicleManager.h"

DECLARE_STATS_GROUP(TEXT("EngineSimulatorPlugin"), STATGROUP_EngineSimulatorPlugin, STATGROUP_Advanced);
DECLARE_CYCLE_STAT(TEXT("EngineThread:UpdateSimulation"), STAT_EngineSimulatorPlugin_UpdateSimulation, STATGROUP_EngineSimulatorPlugin);

FEngineSimulatorThread::FEngineSimulatorThread(IEngineSimulatorInterface* InEngine)
	: bStopRequested(false)
	, Semaphore(FGenericPlatformProcess::GetSynchEventFromPool(false))
	, EngineSimulator(InEngine)
	, Thread(FRunnableThread::Create(this, TEXT("Engine Simulator Thread"))) // TODO: change this later
{
	
}

FEngineSimulatorThread::~FEngineSimulatorThread()
{
	//Stop();

	if (Thread)
	{
		Thread->Kill(true);
		delete Thread;
		Thread = nullptr;
	}

	FGenericPlatformProcess::ReturnSynchEventToPool(Semaphore);
	Semaphore = nullptr;
}

bool FEngineSimulatorThread::Init()
{
	return true;
}

uint32 FEngineSimulatorThread::Run()
{
	while (!bStopRequested)
	{
		Semaphore->Wait();
		if (bStopRequested)
		{
			return 0;
		}
		else
		{
			FEngineSimulatorInput ThisInput;
			{
				FScopeLock Lock(&InputMutex);
				ThisInput = Input;
			}

			{
				SCOPE_CYCLE_COUNTER(STAT_EngineSimulatorPlugin_UpdateSimulation);
				float DynoSpeed = ThisInput.EngineRPM * (EngineSimulator->GetGearRatio() == 0.f ? 1000000.f : EngineSimulator->GetGearRatio());
				EngineSimulator->SetDynoSpeed(DynoSpeed);

				TFunction<void(IEngineSimulatorInterface*)> SimulationUpdate;
				while (UpdateQueue.Dequeue(SimulationUpdate))
				{
					SimulationUpdate(EngineSimulator);
				}

				EngineSimulator->Simulate(ThisInput.DeltaTime);

				float TransmissionTorque = EngineSimulator->GetFilteredDynoTorque() * EngineSimulator->GetGearRatio();

				FloatHistory.AddSample(TransmissionTorque);
				DebugPrint = [T = TransmissionTorque, RPM = EngineSimulator->GetRPM(), Speed = EngineSimulator->GetSpeed(), DynoSpeed = DynoSpeed, FloatHistory = FloatHistory](UWorld* World)
				{
					GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Red, FString::Printf(TEXT("Simulation Tranmission torque: %f"), T));
					GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Red, FString::Printf(TEXT("Simulation RPM: %f"), RPM));
					GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Red, FString::Printf(TEXT("Simulation Speed: %f"), Speed));
					GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Red, FString::Printf(TEXT("Dyno Speed (RPM): %f"), DynoSpeed));

					//FDebugFloatHistory FloatHistory;
					//FloatHistory.AddSample()
//#if ENABLE_DRAW_DEBUG
//					//const FDebugFloatHistory& FloatHistory, const FTransform& DrawTransform, FVector2D DrawSize, FLinearColor DrawColor, float LifeTime
//					if (World->GetFirstPlayerController<APlayerController>())
//					{
//						FVector DrawLocation = World->GetFirstPlayerController<APlayerController>()->PlayerCameraManager->GetCameraLocation();
//						DrawLocation += World->GetFirstPlayerController<APlayerController>()->PlayerCameraManager->GetCameraRotation().Vector() * 600.f;
//						::DrawDebugFloatHistory(*World, FloatHistory, DrawLocation, FVector2D(500.f, 500.f), FColor::Blue, false, 0.f);
//					}
//#endif
				};

				{
					FScopeLock Lock(&OutputMutex);
					Output.Torque = TransmissionTorque;
					Output.RPM = EngineSimulator->GetRPM();
					Output.Redline = EngineSimulator->GetRedLine();
					Output.Horsepower = EngineSimulator->GetDynoPower();
					Output.Name = EngineSimulator->GetName();
					Output.FrameCounter = ThisInput.FrameCounter + 1;
				}
			}
		}
	}

	return 0;
}

void FEngineSimulatorThread::Stop()
{
	bStopRequested = true;
	Semaphore->Trigger();
	Thread->WaitForCompletion();
}

void FEngineSimulatorThread::Exit()
{
	//Stop();
}

void FEngineSimulatorThread::Trigger()
{
	Semaphore->Trigger();
}

UEngineSimulatorWheeledVehicleSimulation::UEngineSimulatorWheeledVehicleSimulation(TArray<class UChaosVehicleWheel*>& WheelsIn, 
	USoundWave* EngineSound, class USoundWaveProcedural* OutputEngineSound)
	: UChaosWheeledVehicleSimulation(WheelsIn)
{
	EngineSimulator = CreateEngine(EngineSound, OutputEngineSound);
	EngineSimulatorThread = MakeUnique<FEngineSimulatorThread>(EngineSimulator.Get());
}

void UEngineSimulatorWheeledVehicleSimulation::ProcessMechanicalSimulation(float DeltaTime)
{
	if (EngineSimulatorThread)
	{
		// Retrieve output from the last frame
		FEngineSimulatorOutput SimulationOutput;
		{
			FScopeLock Lock(&EngineSimulatorThread->OutputMutex);
			SimulationOutput = EngineSimulatorThread->Output;
		}

		{
			FScopeLock Lock(&LastOutputMutex);
			LastOutput = SimulationOutput;
		}

		if (SimulationOutput.FrameCounter != GFrameCounter)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, FString::Printf(TEXT("Simulation behind by %lld frames!!!"), GFrameCounter - SimulationOutput.FrameCounter));
		}

		auto& PTransmission = PVehicle->GetTransmission();

		float WheelRPM = 0;
		for (int I = 0; I < PVehicle->Wheels.Num(); I++)
		{
			if (PVehicle->Wheels[I].EngineEnabled)
			{
				WheelRPM = FMath::Abs(PVehicle->Wheels[I].GetWheelRPM());
			}
		}

		float DynoSpeed = WheelRPM * PTransmission.Setup().FinalDriveRatio;

		// Do input here...
		{
			EngineSimulatorThread->InputMutex.Lock();
			EngineSimulatorThread->Input.DeltaTime = DeltaTime;
			EngineSimulatorThread->Input.EngineRPM = DynoSpeed;
			EngineSimulatorThread->Input.FrameCounter = GFrameCounter;
			EngineSimulatorThread->InputMutex.Unlock();
		}
		EngineSimulatorThread->Trigger();

		// apply drive torque to wheels
		for (int WheelIdx = 0; WheelIdx < Wheels.Num(); WheelIdx++)
		{
			auto& PWheel = PVehicle->Wheels[WheelIdx];
			//PWheel.bInContact = true; // fuck you epic
			if (PWheel.Setup().EngineEnabled)
			{
				PWheel.SetDriveTorque(Chaos::TorqueMToCm(SimulationOutput.Torque * PTransmission.Setup().FinalDriveRatio) * PWheel.Setup().TorqueRatio);
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
}

void UEngineSimulatorWheeledVehicleSimulation::AsyncUpdateSimulation(TFunction<void(IEngineSimulatorInterface*)> InCallable)
{
	if (EngineSimulatorThread)
	{
		EngineSimulatorThread->UpdateQueue.Enqueue(InCallable);
	}
}

void UEngineSimulatorWheeledVehicleSimulation::PrintDebugInfo(UWorld* InWorld)
{
	if (EngineSimulatorThread && EngineSimulatorThread->DebugPrint)
	{
		EngineSimulatorThread->DebugPrint(InWorld);
	}
}

FEngineSimulatorOutput UEngineSimulatorWheeledVehicleSimulation::GetLastOutput()
{
	FScopeLock Lock(&LastOutputMutex);
	return LastOutput;
}