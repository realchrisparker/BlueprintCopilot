// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlueprintForge.h"
#include "BlueprintForgeStyle.h"
#include "BlueprintForgeCommands.h"
#include "BFStaticMeshConsolidator.h"
#include "BFHISMConsolidator.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"

static const FName BlueprintForgeTabName("BlueprintForge");

#define LOCTEXT_NAMESPACE "FBlueprintForgeModule"

void FBlueprintForgeModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	FBlueprintForgeStyle::Initialize();
	FBlueprintForgeStyle::ReloadTextures();

	FBlueprintForgeCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FBlueprintForgeCommands::Get().PluginAction,
		FExecuteAction::CreateStatic(&FBFStaticMeshConsolidator::Execute),
		FCanExecuteAction::CreateStatic(&FBFStaticMeshConsolidator::CanExecute));

	PluginCommands->MapAction(
		FBlueprintForgeCommands::Get().OptimizedPluginAction,
		FExecuteAction::CreateStatic(&FBFHISMConsolidator::Execute),
		FCanExecuteAction::CreateStatic(&FBFHISMConsolidator::CanExecute));

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FBlueprintForgeModule::RegisterMenus));
}

void FBlueprintForgeModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FBlueprintForgeStyle::Shutdown();

	FBlueprintForgeCommands::Unregister();
}

void FBlueprintForgeModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("BlueprintForge", LOCTEXT("BlueprintForgeSection", "Blueprint Forge"));
			Section.AddMenuEntryWithCommandList(FBlueprintForgeCommands::Get().PluginAction, PluginCommands);
			Section.AddMenuEntryWithCommandList(FBlueprintForgeCommands::Get().OptimizedPluginAction, PluginCommands);
		}
	}

	{
		UToolMenu* ContextMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.ActorContextMenu");
		{
			FToolMenuSection& Section = ContextMenu->FindOrAddSection("BlueprintForge", LOCTEXT("BlueprintForgeSection", "Blueprint Forge"));
			Section.AddMenuEntryWithCommandList(FBlueprintForgeCommands::Get().PluginAction, PluginCommands);
			Section.AddMenuEntryWithCommandList(FBlueprintForgeCommands::Get().OptimizedPluginAction, PluginCommands);
		}
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBlueprintForgeModule, BlueprintForge)
