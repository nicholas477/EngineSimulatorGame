// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "EngineSimulator.generated.h"

class Simulator;
class Engine;
class Vehicle;
class Transmission;

/**
 * 
 */
UCLASS()
class ENGINESIMULATORPLUGIN_API UEngineSimulator : public UObject
{
	GENERATED_BODY()

public:
	UEngineSimulator();
	~UEngineSimulator();

protected:
	void loadScript();
	void loadEngine(Engine* engine, Vehicle* vehicle, Transmission* transmission);
	void process(float frame_dt);

protected:
	Simulator* m_simulator;
	Vehicle* m_vehicle;
	Transmission* m_transmission;
	Engine* m_iceEngine;
};
