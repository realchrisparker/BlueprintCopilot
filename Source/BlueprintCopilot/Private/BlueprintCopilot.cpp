// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlueprintCopilot.h"
#include "BlueprintCopilotStyle.h"
#include "BlueprintCopilotCommands.h"
#include "BCStaticMeshConsolidator.h"
#include "BCHISMConsolidator.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"

static const FName BlueprintCopilotTabName("BlueprintCopilot");

#define LOCTEXT_NAMESPACE "FBlueprintCopilotModule"

void FBlueprintCopilotModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FBlueprintCopilotStyle::Initialize();
	FBlueprintCopilotStyle::ReloadTextures();

	FBlueprintCopilotCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FBlueprintCopilotCommands::Get().PluginAction,
		FExecuteAction::CreateStatic(&FBCStaticMeshConsolidator::Execute),
		FCanExecuteAction::CreateStatic(&FBCStaticMeshConsolidator::CanExecute));

	PluginCommands->MapAction(
		FBlueprintCopilotCommands::Get().OptimizedPluginAction,
		FExecuteAction::CreateStatic(&FBCHISMConsolidator::Execute),
		FCanExecuteAction::CreateStatic(&FBCHISMConsolidator::CanExecute));

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FBlueprintCopilotModule::RegisterMenus));
}

void FBlueprintCopilotModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FBlueprintCopilotStyle::Shutdown();

	FBlueprintCopilotCommands::Unregister();
}

void FBlueprintCopilotModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("BlueprintCopilot", LOCTEXT("BlueprintCopilotSection", "Blueprint Copilot"));
			Section.AddMenuEntryWithCommandList(FBlueprintCopilotCommands::Get().PluginAction, PluginCommands);
			Section.AddMenuEntryWithCommandList(FBlueprintCopilotCommands::Get().OptimizedPluginAction, PluginCommands);
		}
	}

	{
		UToolMenu* ContextMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.ActorContextMenu");
		{
			FToolMenuSection& Section = ContextMenu->FindOrAddSection("BlueprintCopilot", LOCTEXT("BlueprintCopilotSection", "Blueprint Copilot"));
			Section.AddMenuEntryWithCommandList(FBlueprintCopilotCommands::Get().PluginAction, PluginCommands);
			Section.AddMenuEntryWithCommandList(FBlueprintCopilotCommands::Get().OptimizedPluginAction, PluginCommands);
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FBlueprintCopilotModule, BlueprintCopilot)