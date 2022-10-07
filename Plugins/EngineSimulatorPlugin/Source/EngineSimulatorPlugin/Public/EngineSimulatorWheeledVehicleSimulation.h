// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "HAL/Runnable.h"
#include "EngineSimulatorWheeledVehicleSimulation.generated.h"

class IEngineSimulatorInterface;

struct FEngineSimulatorInput
{
	float DeltaTime = 1.0f / 60.f;
	float EngineRPM = 0;
	uint64 FrameCounter = 0;
};

USTRUCT(BlueprintType)
struct FEngineSimulatorOutput
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Engine Simulator Output")
		float Torque = 0; // Newtons per meter

	UPROPERTY(BlueprintReadOnly, Category = "Engine Simulator Output")
		float RPM = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Engine Simulator Output")
		float Redline = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Engine Simulator Output")
		float Horsepower = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Engine Simulator Output")
		FString Name;

	uint64 FrameCounter = 0;
};

class FEngineSimulatorThread : public FRunnable
{
public:

	// Constructor, create the thread by calling this
	FEngineSimulatorThread(IEngineSimulatorInterface* InEngine);

	// Destructor
	virtual ~FEngineSimulatorThread() override;


	// Overriden from FRunnable
	// Do not call these functions youself, that will happen automatically
	bool Init() override; // Do your setup here, allocate memory, ect.
	uint32 Run() override; // Main data processing happens here
	void Stop() override; // Clean up any memory you allocated here
	void Exit() override;

	void Trigger();

protected:
	TAtomic<bool> bStopRequested;

	FCriticalSection InputMutex;
	FEngineSimulatorInput Input;

	FCriticalSection OutputMutex;
	FEngineSimulatorOutput Output;

	FEvent* Semaphore;
	IEngineSimulatorInterface* EngineSimulator;

	TQueue<TFunction<void(IEngineSimulatorInterface*)>, EQueueMode::Mpsc> UpdateQueue;
	TFunction<void(UWorld* World)> DebugPrint;

	FRunnableThread* Thread;

	friend class UEngineSimulatorWheeledVehicleSimulation;

	// Other stuff
	FDebugFloatHistory FloatHistory;
};

class UEngineSimulatorWheeledVehicleSimulation : public UChaosWheeledVehicleSimulation
{
public:
	UEngineSimulatorWheeledVehicleSimulation(TArray<class UChaosVehicleWheel*>& WheelsIn, class USoundWave* EngineSound, class USoundWaveProcedural* OutputEngineSound);

	/** Update the engine/transmission simulation */
	virtual void ProcessMechanicalSimulation(float DeltaTime) override;

	/** Pass control Input to the vehicle systems */
	virtual void ApplyInput(const FControlInputs& ControlInputs, float DeltaTime) override;

	void AsyncUpdateSimulation(TFunction<void(IEngineSimulatorInterface*)> InCallable);

	void PrintDebugInfo(UWorld* World);

	FEngineSimulatorOutput GetLastOutput();

protected:
	TUniquePtr<IEngineSimulatorInterface> EngineSimulator;
	TUniquePtr<FEngineSimulatorThread> EngineSimulatorThread;

	FEngineSimulatorOutput LastOutput;
	mutable FCriticalSection LastOutputMutex;

	bool bStarterEnabled;
	bool bDynoEnabled;
	bool bIgnitionEnabled;
};