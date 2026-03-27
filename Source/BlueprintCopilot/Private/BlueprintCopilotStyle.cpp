// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlueprintCopilotStyle.h"
#include "BlueprintCopilot.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateStyleRegistry.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FBlueprintCopilotStyle::StyleInstance = nullptr;

void FBlueprintCopilotStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FBlueprintCopilotStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FBlueprintCopilotStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("BlueprintCopilotStyle"));
	return StyleSetName;
}


const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);

TSharedRef< FSlateStyleSet > FBlueprintCopilotStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("BlueprintCopilotStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("BlueprintCopilot")->GetBaseDir() / TEXT("Resources"));

	Style->Set("BlueprintCopilot.PluginAction",          new IMAGE_BRUSH_SVG(TEXT("copilot"), Icon20x20));
	Style->Set("BlueprintCopilot.OptimizedPluginAction", new IMAGE_BRUSH_SVG(TEXT("copilot"), Icon20x20));
	return Style;
}

void FBlueprintCopilotStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FBlueprintCopilotStyle::Get()
{
	return *StyleInstance;
}
