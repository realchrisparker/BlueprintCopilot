// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "BlueprintCopilotStyle.h"

class FBlueprintCopilotCommands : public TCommands<FBlueprintCopilotCommands>
{
public:

	FBlueprintCopilotCommands()
		: TCommands<FBlueprintCopilotCommands>(TEXT("BlueprintCopilot"), NSLOCTEXT("Contexts", "BlueprintCopilot", "BlueprintCopilot Plugin"), NAME_None, FBlueprintCopilotStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > PluginAction;
	TSharedPtr< FUICommandInfo > OptimizedPluginAction;
};
