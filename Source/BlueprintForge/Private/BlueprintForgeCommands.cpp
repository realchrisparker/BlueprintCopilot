// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlueprintForgeCommands.h"

#define LOCTEXT_NAMESPACE "FBlueprintForgeModule"

void FBlueprintForgeCommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "Create Blueprint From Selected", "Consolidates selected Static Mesh Actors into a new Blueprint asset", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(OptimizedPluginAction, "Create Blueprint From Selected Optimized", "Consolidates selected Static Mesh Actors into a Blueprint, using HISM components for meshes used more than twice", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
