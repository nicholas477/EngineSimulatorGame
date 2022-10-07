// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Templates/UniquePtr.h"

class Simulator;
class Engine;
class Vehicle;
class Transmission;

class ENGINESIMULATORPLUGIN_API IEngineSimulatorInterface
{
public:
	virtual void Simulate(float DeltaTime) = 0;
	virtual void SetDynoEnabled(bool bEnabled) = 0;
	virtual void SetStarterEnabled(bool bEnabled) = 0;
	virtual void SetIgnitionEnabled(bool bEnabled) = 0;
	virtual void SetSpeedControl(float Speed) = 0;
	virtual void SetDynoSpeed(float RPM) = 0;
	virtual void SetGear(int32 Gear) = 0;
	//virtual void SetClutch(float Clutch) = 0;
	virtual int32 GetGear() = 0;
	virtual float GetSpeed() = 0;
	virtual float GetRPM() = 0;
	virtual float GetRedLine() = 0;
	virtual float GetFilteredDynoTorque() = 0;
	virtual float GetDynoPower() = 0; // In horsepower
	virtual float GetGearRatio() = 0;
	virtual int32 GetGearCount() = 0;
	virtual bool IsDynoEnabled() = 0;
	virtual FString GetName() = 0;
	virtual ~IEngineSimulatorInterface() {};
};

TUniquePtr<IEngineSimulatorInterface> CreateEngine(class USoundWave* EngineSound, class USoundWaveProcedural* SoundWaveOutput);
