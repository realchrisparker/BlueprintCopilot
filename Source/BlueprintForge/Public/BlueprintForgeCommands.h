// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "BlueprintForgeStyle.h"

class FBlueprintForgeCommands : public TCommands<FBlueprintForgeCommands>
{
public:

	FBlueprintForgeCommands()
		: TCommands<FBlueprintForgeCommands>(TEXT("BlueprintForge"), NSLOCTEXT("Contexts", "BlueprintForge", "BlueprintForge Plugin"), NAME_None, FBlueprintForgeStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > PluginAction;
	TSharedPtr< FUICommandInfo > OptimizedPluginAction;
};
