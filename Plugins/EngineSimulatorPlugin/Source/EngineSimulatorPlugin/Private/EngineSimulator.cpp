// Fill out your copyright notice in the Description page of Project Settings.

#include "EngineSimulator.h"
#include "EngineSimulatorPlugin.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundWaveProcedural.h"

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

//#include "delta.h"

#include "audio_buffer.h"

#include "Windows/PostWindowsApi.h"
#include "Windows/HideWindowsPlatformAtomics.h"
#include "Windows/HideWindowsPlatformTypes.h"

typedef unsigned int SampleOffset;

/**
 *
 */
class FEngineSimulator : public IEngineSimulatorInterface
{
public:
    FEngineSimulator(USoundWave* EngineSound, USoundWaveProcedural* InSoundWaveOutput);
    virtual ~FEngineSimulator() {};

    // IEngineSimulatorInterface
    virtual void Simulate(float DeltaTime) override { process(DeltaTime); }
    virtual void SetDynoEnabled(bool bEnabled) { m_simulator.m_dyno.m_enabled = bEnabled; }
    virtual void SetStarterEnabled(bool bEnabled) { m_simulator.m_starterMotor.m_enabled = bEnabled; }
    virtual void SetIgnitionEnabled(bool bEnabled) { m_simulator.getEngine()->getIgnitionModule()->m_enabled = bEnabled; }
    virtual void SetSpeedControl(float SpeedControl) { m_iceEngine->setSpeedControl(SpeedControl); }
    virtual void SetGear(int32 Gear) { m_simulator.getTransmission()->changeGear(Gear); };
    virtual int32 GetGear() { return m_simulator.getTransmission()->getGear(); };
    virtual float GetSpeed() { return m_simulator.getEngine()->getSpeed(); };
    virtual float GetRPM() { return m_simulator.getEngine()->getRpm(); };
    virtual float GetRedLine() { return m_simulator.getEngine()->getRedline(); };
    virtual float GetFilteredDynoTorque() { return m_simulator.getFilteredDynoTorque(); };
    virtual float GetGearRatio()
    {
        const int32 current_gear = m_simulator.getTransmission()->getGear();
        if (current_gear >= 0 && current_gear < m_simulator.getTransmission()->getGearCount())
        {
            return m_simulator.getTransmission()->getGearRatios()[current_gear];
        }
        else
        {
            return 0.f;
        }
    }
    // End IEngineSimulatorInterface

protected:
    void loadScript();
    void loadEngine(Engine* engine, Vehicle* vehicle, Transmission* transmission);
    void process(float frame_dt);

protected:
    Simulator m_simulator;
    Vehicle* m_vehicle;
    Transmission* m_transmission;
    Engine* m_iceEngine;

    float m_dynoSpeed;
    USoundWave* EngineSound;
    USoundWaveProcedural* SoundWaveOutput;

    //ysAudioBuffer* m_outputAudioBuffer;
    AudioBuffer m_audioBuffer;
    uint32 PlayCursor;
    std::vector<uint8> Buffer;
    //ysAudioSource* m_audioSource;

    void FillAudio(USoundWaveProcedural* Wave, const int32 SamplesNeeded);
};

FEngineSimulator::FEngineSimulator(USoundWave* InEngineSound, USoundWaveProcedural* InSoundWaveOutput)
{
    m_vehicle = nullptr;
    m_transmission = nullptr;
    m_iceEngine = nullptr;

    m_dynoSpeed = 0;

    EngineSound = InEngineSound;
    SoundWaveOutput = InSoundWaveOutput;

    PlayCursor = 0;

	loadScript();

    m_audioBuffer.initialize(44100, 44100);
    m_audioBuffer.m_writePointer = (int)(44100 * 0.1);

    //ysAudioParameters params;
    //params.m_bitsPerSample = 16;
    //params.m_channelCount = 1;
    //params.m_sampleRate = 44100;
    //m_outputAudioBuffer =
    //    m_engine.GetAudioDevice()->CreateBuffer(&params, 44100);

    //m_audioSource = m_engine.GetAudioDevice()->CreateSource(m_outputAudioBuffer);
    //m_audioSource->SetMode((m_simulator.getEngine() != nullptr)
    //    ? ysAudioSource::Mode::Loop
    //    : ysAudioSource::Mode::Stop);
    //m_audioSource->SetPan(0.0f);
    //m_audioSource->SetVolume(1.0f);

    check(SoundWaveOutput);
    if (SoundWaveOutput)
    {
        SoundWaveOutput->OnSoundWaveProceduralUnderflow.BindRaw(this, &FEngineSimulator::FillAudio);
    }
}

void FEngineSimulator::loadScript()
{
    UE_LOG(LogTemp, Warning, TEXT("void UEngineSimulator::loadScript()"));

    Engine* engine = nullptr;
    Vehicle* vehicle = nullptr;
    Transmission* transmission = nullptr;

#ifdef ATG_ENGINE_SIM_PIRANHA_ENABLED
    es_script::Compiler compiler;
    const FString esPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FEngineSimulatorPluginModule::GetAssetDirectory(), "es/"));
    compiler.initialize();
    compiler.addSearchPath(TCHAR_TO_UTF8(*esPath));
    const FString EnginePath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FEngineSimulatorPluginModule::GetAssetDirectory(), "main.mr"));
    const bool compiled = compiler.compile(TCHAR_TO_UTF8(*EnginePath));
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

void FEngineSimulator::loadEngine(Engine* engine, Vehicle* vehicle, Transmission* transmission)
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
    m_simulator.releaseSimulation();

    if (engine == nullptr || vehicle == nullptr || transmission == nullptr) {
        m_iceEngine = nullptr;
        //m_viewParameters.Layer1 = 0;

        return;
    }

    //createObjects(engine);

    //m_viewParameters.Layer1 = engine->getMaxDepth();
    engine->calculateDisplacement();

    m_simulator.setFluidSimulationSteps(8);
    m_simulator.setSimulationFrequency(engine->getSimulationFrequency());

    Simulator::Parameters simulatorParams;
    simulatorParams.SystemType = Simulator::SystemType::NsvOptimized;
    m_simulator.initialize(simulatorParams);
    m_simulator.loadSimulation(engine, vehicle, transmission);

    Synthesizer::AudioParameters audioParams = m_simulator.getSynthesizer()->getAudioParameters();
    audioParams.InputSampleNoise = static_cast<float>(engine->getInitialJitter());
    audioParams.AirNoise = static_cast<float>(engine->getInitialNoise());
    audioParams.dF_F_mix = static_cast<float>(engine->getInitialHighFrequencyGain());
    m_simulator.getSynthesizer()->setAudioParameters(audioParams);

    for (int i = 0; i < engine->getExhaustSystemCount(); ++i) {
        ImpulseResponse* response = engine->getExhaustSystem(i)->getImpulseResponse();

        //ysWindowsAudioWaveFile waveFile;
        //waveFile.OpenFile(response->getFilename().c_str());
        //waveFile.InitializeInternalBuffer(waveFile.GetSampleCount());
        //waveFile.FillBuffer(0);
        //waveFile.CloseFile();

        if (EngineSound)
        {
            int32 NumSamples = EngineSound->RawData.GetBulkDataSize() / sizeof(int16);
            //check(EngineSound->RawData.GetElementSize() == sizeof(int16_t));
            m_simulator.getSynthesizer()->initializeImpulseResponse(
                reinterpret_cast<const int16_t*>(EngineSound->RawData.LockReadOnly()),
                NumSamples,
                response->getVolume(),
                i
            );

            EngineSound->RawData.Unlock();
        }

        //waveFile.DestroyInternalBuffer();
    }

    if (EngineSound)
    {
        m_simulator.startAudioRenderingThread();
    }
}

void FEngineSimulator::process(float frame_dt)
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

    UE_LOG(LogTemp, Warning, TEXT("Engine throttle: %f"), m_simulator.getEngine()->getSpeedControl());
    UE_LOG(LogTemp, Warning, TEXT("Starter Enabled: %s"), m_simulator.m_starterMotor.m_enabled ? TEXT("TRUE") : TEXT("FALSE"));
    UE_LOG(LogTemp, Warning, TEXT("Dyno Enabled: %s"), m_simulator.m_dyno.m_enabled ? TEXT("TRUE") : TEXT("FALSE"));
    UE_LOG(LogTemp, Warning, TEXT("Ignition Enabled: %s"), m_simulator.getEngine()->getIgnitionModule()->m_enabled ? TEXT("TRUE") : TEXT("FALSE"));

    m_simulator.setSimulationSpeed(speed);

    if (m_simulator.m_dyno.m_enabled) {
        if (!m_simulator.m_dyno.m_hold) {
            if (m_simulator.getFilteredDynoTorque() > units::torque(1.0, units::ft_lb)) {
                m_dynoSpeed += units::rpm(500) * frame_dt;
            }
            else {
                m_dynoSpeed *= (1 / (1 + frame_dt));
            }

            //if ((m_dynoSpeed + units::rpm(1000)) > m_iceEngine->getRedline()) {
            //    m_simulator.m_dyno.m_enabled = false;
            //    m_dynoSpeed = units::rpm(0);
            //}
        }
    }

    m_simulator.m_dyno.m_rotationSpeed = m_dynoSpeed + units::rpm(1000);

    m_simulator.startFrame(frame_dt);

    auto proc_t0 = std::chrono::steady_clock::now();
    const int iterationCount = m_simulator.getFrameIterationCount();
    while (m_simulator.simulateStep()) {
        //m_oscCluster->sample();
    }

    auto proc_t1 = std::chrono::steady_clock::now();

    m_simulator.endFrame();

    auto duration = proc_t1 - proc_t0;
    if (iterationCount > 0) {
        //m_performanceCluster->addTimePerTimestepSample(
        //    (duration.count() / 1E9) / iterationCount);
    }

    //const SampleOffset safeWritePosition = m_audioSource->GetCurrentWritePosition();
    //const SampleOffset writePosition = m_audioBuffer.m_writePointer;

    ////SampleOffset targetWritePosition =
    ////    m_audioBuffer.getBufferIndex(safeWritePosition, (int)(44100 * 0.1));
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
    //    //if (m_oscillatorSampleOffset % 4 == 0) {
    //    //    m_oscCluster->getAudioWaveformOscilloscope()->addDataPoint(
    //    //        m_oscillatorSampleOffset,
    //    //        sample / (float)(INT16_MAX));
    //    //}

    //    m_audioBuffer.writeSample(sample, m_audioBuffer.m_writePointer, (int)i);

    //    //m_oscillatorSampleOffset = (m_oscillatorSampleOffset + 1) % (44100 / 10);
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
}

const static int32 SampleRate = 44100;

void FEngineSimulator::FillAudio(USoundWaveProcedural* Wave, const int32 SamplesNeeded)
{
    // Unreal engine uses a fixed sample size.
    static const uint32 SAMPLE_SIZE = sizeof(uint16);

    //if (!StartPlaying.test_and_set())
    //{
    //    PlayCursor = 0;
    //    bPlayingSound = true;
    //}

    // We're using only one channel.
    
    const uint32 SampleCount = SamplesNeeded;// FMath::Min<uint32>(OptimalSampleCount, SamplesNeeded);
    int16_t* samples = new int16_t[SamplesNeeded];
    const int readSamples = m_simulator.readAudioOutput(SamplesNeeded, samples);

    // If we're not playing, fill the buffer with zeros for silence.
    //if (!bPlayingSound)
    //{
    //    const uint32 ByteCount = SampleCount * SAMPLE_SIZE;
    //    Buffer.resize(ByteCount);
    //    FMemory::Memset(&Buffer[0], 0, ByteCount);
    //    Wave->QueueAudio(&Buffer[0], ByteCount);
    //    return;
    //}

    //uint32 Fraction = 1; // Fraction of a second to play.
    //float Frequency = 1046.5;
    //uint32 TotalSampleCount = SampleRate / Fraction;

    //Buffer.resize(SampleCount * SAMPLE_SIZE);
    //int16* data = (int16*)&Buffer[0];


    //PlayCursor += SampleCount;

    //if (PlayCursor >= TotalSampleCount)
    //{
    //    bPlayingSound = false;
    //}

    Wave->QueueAudio((const uint8*)samples, SampleCount * SAMPLE_SIZE);
}


TUniquePtr<IEngineSimulatorInterface> CreateEngine(USoundWave* EngineSound, USoundWaveProcedural* SoundWaveOutput)
{
    return MakeUnique<FEngineSimulator>(EngineSound, SoundWaveOutput);
}