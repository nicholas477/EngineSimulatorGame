// Copyright Epic Games, Inc. All Rights Reserved.

#include "EngineSimulatorPlugin.h"
#include "Core.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
//#include "EngineSimulatorPluginLibrary/ExampleLibrary.h"

#define LOCTEXT_NAMESPACE "FEngineSimulatorPluginModule"

void FEngineSimulatorPluginModule::StartupModule()
{
}

void FEngineSimulatorPluginModule::ShutdownModule()
{

}

FString FEngineSimulatorPluginModule::GetAssetDirectory()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("EngineSimulatorPlugin"));

	check(Plugin.IsValid());

	return FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources"), TEXT("assets"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FEngineSimulatorPluginModule, EngineSimulatorPlugin)
