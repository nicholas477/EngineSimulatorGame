// Fill out your copyright notice in the Description page of Project Settings.

#include "EngineSimulator.h"
#include "EngineSimulatorPlugin.h"

#ifdef PI
#undef PI
#endif

#ifdef TWO_PI
#undef TWO_PI
#endif

#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/AllowWindowsPlatformAtomics.h"
#include "Windows/PreWindowsApi.h"

#include <mmiscapi.h>

#pragma warning(disable : 4587)

#include "simulator.h"
#include "compiler.h"
#include "engine.h"
#include "transmission.h"

// These are only needed for the graphical representation
//#include "simulation_object.h"
//#include "connecting_rod_object.h"
//#include "piston_object.h"
//#include "crankshaft_object.h"
//#include "combustion_chamber_object.h"
//#include "cylinder_bank_object.h"
//#include "cylinder_head_object.h"

#include "Windows/PostWindowsApi.h"
#include "Windows/HideWindowsPlatformAtomics.h"
#include "Windows/HideWindowsPlatformTypes.h"

UEngineSimulator::UEngineSimulator()
{
    m_simulator = new Simulator();
	loadScript();
}

UEngineSimulator::~UEngineSimulator()
{
	delete m_simulator;
}

void UEngineSimulator::loadScript()
{
    UE_LOG(LogTemp, Warning, TEXT("void UEngineSimulator::loadScript()"));

    Engine* engine = nullptr;
    Vehicle* vehicle = nullptr;
    Transmission* transmission = nullptr;

#ifdef ATG_ENGINE_SIM_PIRANHA_ENABLED
    es_script::Compiler compiler;
    compiler.initialize();
    const bool compiled = compiler.compile("/main.mr");
    if (compiled) {
        const es_script::Compiler::Output output = compiler.execute();

        engine = output.engine;
        vehicle = output.vehicle;
        transmission = output.transmission;
    }
    else {
        engine = nullptr;
        vehicle = nullptr;
        transmission = nullptr;
    }

    compiler.destroy();
#endif /* ATG_ENGINE_SIM_PIRANHA_ENABLED */

    if (vehicle == nullptr) {
        Vehicle::Parameters vehParams;
        vehParams.mass = units::mass(1597, units::kg);
        vehParams.diffRatio = 3.42;
        vehParams.tireRadius = units::distance(10, units::inch);
        vehParams.dragCoefficient = 0.25;
        vehParams.crossSectionArea = units::distance(6.0, units::foot) * units::distance(6.0, units::foot);
        vehParams.rollingResistance = 2000.0;
        vehicle = new Vehicle;
        vehicle->initialize(vehParams);
    }

    if (transmission == nullptr) {
        const double gearRatios[] = { 2.97, 2.07, 1.43, 1.00, 0.84, 0.56 };
        Transmission::Parameters tParams;
        tParams.GearCount = 6;
        tParams.GearRatios = gearRatios;
        tParams.MaxClutchTorque = units::torque(1000.0, units::ft_lb);
        transmission = new Transmission;
        transmission->initialize(tParams);
    }

    loadEngine(engine, vehicle, transmission);
    //refreshUserInterface();
}

void UEngineSimulator::loadEngine(Engine* engine, Vehicle* vehicle, Transmission* transmission)
{
    UE_LOG(LogTemp, Warning, TEXT("void UEngineSimulator::loadEngine()"));

    if (m_vehicle != nullptr) {
        delete m_vehicle;
        m_vehicle = nullptr;
    }

    if (m_transmission != nullptr) {
        delete m_transmission;
        m_transmission = nullptr;
    }

    if (m_iceEngine != nullptr) {
        m_iceEngine->destroy();
        delete m_iceEngine;
    }

    m_iceEngine = engine;
    m_vehicle = vehicle;
    m_transmission = transmission;

    //destroyObjects();
    m_simulator->releaseSimulation();

    if (engine == nullptr || vehicle == nullptr || transmission == nullptr) {
        m_iceEngine = nullptr;
        //m_viewParameters.Layer1 = 0;

        return;
    }

    //createObjects(engine);

    //m_viewParameters.Layer1 = engine->getMaxDepth();
    engine->calculateDisplacement();

    m_simulator->setFluidSimulationSteps(8);
    m_simulator->setSimulationFrequency(engine->getSimulationFrequency());

    Simulator::Parameters simulatorParams;
    simulatorParams.SystemType = Simulator::SystemType::NsvOptimized;
    m_simulator->initialize(simulatorParams);
    m_simulator->loadSimulation(engine, vehicle, transmission);

    //Synthesizer::AudioParameters audioParams = m_simulator->getSynthesizer()->getAudioParameters();
    //audioParams.InputSampleNoise = static_cast<float>(engine->getInitialJitter());
    //audioParams.AirNoise = static_cast<float>(engine->getInitialNoise());
    //audioParams.dF_F_mix = static_cast<float>(engine->getInitialHighFrequencyGain());
    //m_simulator->getSynthesizer()->setAudioParameters(audioParams);

    //for (int i = 0; i < engine->getExhaustSystemCount(); ++i) {
    //    ImpulseResponse* response = engine->getExhaustSystem(i)->getImpulseResponse();

    //    ysWindowsAudioWaveFile waveFile;
    //    waveFile.OpenFile(response->getFilename().c_str());
    //    waveFile.InitializeInternalBuffer(waveFile.GetSampleCount());
    //    waveFile.FillBuffer(0);
    //    waveFile.CloseFile();

    //    m_simulator->getSynthesizer()->initializeImpulseResponse(
    //        reinterpret_cast<const int16_t*>(waveFile.GetBuffer()),
    //        waveFile.GetSampleCount(),
    //        response->getVolume(),
    //        i
    //    );

    //    waveFile.DestroyInternalBuffer();
    //}

    //m_simulator.startAudioRenderingThread();
}

void UEngineSimulator::process(float frame_dt)
{
    frame_dt = static_cast<float>(clamp(frame_dt, 1 / 200.0f, 1 / 30.0f));

    double speed = 1.0 / 1.0;
    //if (m_engine.IsKeyDown(ysKey::Code::N1)) {
    //    speed = 1 / 10.0;
    //}
    //else if (m_engine.IsKeyDown(ysKey::Code::N2)) {
    //    speed = 1 / 100.0;
    //}
    //else if (m_engine.IsKeyDown(ysKey::Code::N3)) {
    //    speed = 1 / 200.0;
    //}
    //else if (m_engine.IsKeyDown(ysKey::Code::N4)) {
    //    speed = 1 / 500.0;
    //}
    //else if (m_engine.IsKeyDown(ysKey::Code::N5)) {
    //    speed = 1 / 1000.0;
    //}

    m_simulator->setSimulationSpeed(speed);

    const double avgFramerate = FMath::Clamp(1.0 / FApp::GetDeltaTime(), 30.0f, 1000.0f);
    m_simulator->startFrame(1 / avgFramerate);

    auto proc_t0 = std::chrono::steady_clock::now();
    const int iterationCount = m_simulator->getFrameIterationCount();
    while (m_simulator->simulateStep()) {
        //m_oscCluster->sample();
    }

    auto proc_t1 = std::chrono::steady_clock::now();

    m_simulator->endFrame();

    auto duration = proc_t1 - proc_t0;
    if (iterationCount > 0) {
        //m_performanceCluster->addTimePerTimestepSample(
        //    (duration.count() / 1E9) / iterationCount);
    }

    //const SampleOffset safeWritePosition = m_audioSource->GetCurrentWritePosition();
    //const SampleOffset writePosition = m_audioBuffer.m_writePointer;

    //SampleOffset targetWritePosition =
    //    m_audioBuffer.getBufferIndex(safeWritePosition, (int)(44100 * 0.1));
    //SampleOffset maxWrite = m_audioBuffer.offsetDelta(writePosition, targetWritePosition);

    //SampleOffset currentLead = m_audioBuffer.offsetDelta(safeWritePosition, writePosition);
    //SampleOffset newLead = m_audioBuffer.offsetDelta(safeWritePosition, targetWritePosition);

    //if (currentLead > 44100 * 0.5) {
    //    m_audioBuffer.m_writePointer = m_audioBuffer.getBufferIndex(safeWritePosition, (int)(44100 * 0.05));
    //    currentLead = m_audioBuffer.offsetDelta(safeWritePosition, m_audioBuffer.m_writePointer);
    //    maxWrite = m_audioBuffer.offsetDelta(m_audioBuffer.m_writePointer, targetWritePosition);
    //}

    //if (currentLead > newLead) {
    //    maxWrite = 0;
    //}

    //int16_t* samples = new int16_t[maxWrite];
    //const int readSamples = m_simulator.readAudioOutput(maxWrite, samples);

    //for (SampleOffset i = 0; i < (SampleOffset)readSamples && i < maxWrite; ++i) {
    //    const int16_t sample = samples[i];
    //    if (m_oscillatorSampleOffset % 4 == 0) {
    //        m_oscCluster->getAudioWaveformOscilloscope()->addDataPoint(
    //            m_oscillatorSampleOffset,
    //            sample / (float)(INT16_MAX));
    //    }

    //    m_audioBuffer.writeSample(sample, m_audioBuffer.m_writePointer, (int)i);

    //    m_oscillatorSampleOffset = (m_oscillatorSampleOffset + 1) % (44100 / 10);
    //}

    //delete[] samples;

    //if (readSamples > 0) {
    //    SampleOffset size0, size1;
    //    void* data0, * data1;
    //    m_audioSource->LockBufferSegment(
    //        m_audioBuffer.m_writePointer, readSamples, &data0, &size0, &data1, &size1);

    //    m_audioBuffer.copyBuffer(
    //        reinterpret_cast<int16_t*>(data0), m_audioBuffer.m_writePointer, size0);
    //    m_audioBuffer.copyBuffer(
    //        reinterpret_cast<int16_t*>(data1),
    //        m_audioBuffer.getBufferIndex(m_audioBuffer.m_writePointer, size0),
    //        size1);

    //    m_audioSource->UnlockBufferSegments(data0, size0, data1, size1);
    //    m_audioBuffer.commitBlock(readSamples);
    //}

    //m_performanceCluster->addInputBufferUsageSample(
    //    (double)m_simulator.getSynthesizerInputLatency() / m_simulator.getSynthesizerInputLatencyTarget());
    //m_performanceCluster->addAudioLatencySample(
    //    m_audioBuffer.offsetDelta(m_audioSource->GetCurrentWritePosition(), m_audioBuffer.m_writePointer) / (44100 * 0.1));
}
