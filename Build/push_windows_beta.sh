#!/bin/bash
mkdir butler
rm -rfv butler/EngineSimulatorGame
cp -r Windows/ butler/EngineSimulatorGame
rm -rfv ./butler/EngineSimulatorGame/EngineSimulatorGame/Saved
rm -rfv ./butler/EngineSimulatorGame/FileOpenOrder
rm -rfv ./butler/EngineSimulatorGame/Manifest*.txt
butler push --ignore '*.pdb' butler/ epicgameguy/engine-simulator-in-unreal-engine:windows-beta
read -p "Press any key to close window..."
