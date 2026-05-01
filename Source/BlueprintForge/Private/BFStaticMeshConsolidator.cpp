// Copyright God's Studio. All Rights Reserved.

#include "BFStaticMeshConsolidator.h"

#include "Editor.h"
#include "Engine/Selection.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DecalActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "Misc/MessageDialog.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "FBFStaticMeshConsolidator"

bool FBFStaticMeshConsolidator::CanExecute()
{
	if (!GEditor)
	{
		return false;
	}

	USelection* Selection = GEditor->GetSelectedActors();
	for (FSelectionIterator It(*Selection); It; ++It)
	{
		if (Cast<AStaticMeshActor>(*It) || Cast<ADecalActor>(*It))
		{
			return true;
		}
	}
	return false;
}

void FBFStaticMeshConsolidator::Execute()
{
	if (!GEditor)
	{
		return;
	}

	// ---- 1. Gather selected AStaticMeshActors and ADecalActors -----------------

	TArray<AStaticMeshActor*> MeshActors;
	TArray<ADecalActor*>      DecalActors;
	USelection* Selection = GEditor->GetSelectedActors();
	for (FSelectionIterator It(*Selection); It; ++It)
	{
		if (AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(*It))
		{
			MeshActors.Add(MeshActor);
		}
		else if (ADecalActor* DecalActor = Cast<ADecalActor>(*It))
		{
			DecalActors.Add(DecalActor);
		}
	}

	if (MeshActors.Num() == 0 && DecalActors.Num() == 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("NoActors", "No Static Mesh Actors or Decal Actors are selected.\nSelect at least one and try again."));
		return;
	}

	// ---- 2. Compute centroid (used as the Blueprint's world origin) ------------

	FVector Centroid = FVector::ZeroVector;
	for (const AStaticMeshActor* Actor : MeshActors)
	{
		Centroid += Actor->GetActorLocation();
	}
	for (const ADecalActor* Actor : DecalActors)
	{
		Centroid += Actor->GetActorLocation();
	}
	Centroid /= static_cast<double>(MeshActors.Num() + DecalActors.Num());

	// ---- 3. Ask the user where to save the Blueprint ---------------------------

	FSaveAssetDialogConfig SaveConfig;
	SaveConfig.DialogTitleOverride = LOCTEXT("SaveDialogTitle", "Save Consolidated Blueprint");
	SaveConfig.DefaultPath        = TEXT("/Game/Blueprints");
	SaveConfig.DefaultAssetName   = TEXT("BP_ConsolidatedMeshes");
	SaveConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;

	FContentBrowserModule& ContentBrowserModule =
		FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	const FString SaveObjectPath =
		ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveConfig);

	if (SaveObjectPath.IsEmpty())
	{
		return; // User cancelled
	}

	const FString OutPackageName = FPackageName::ObjectPathToPackageName(SaveObjectPath);
	const FString OutAssetName   = FPaths::GetBaseFilename(OutPackageName);

	// ---- 4. Create the UPackage and Blueprint asset ----------------------------

	UPackage* Package = CreatePackage(*OutPackageName);
	if (!Package)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("PackageFailed", "Failed to create an asset package. Aborting."));
		return;
	}
	Package->FullyLoad();

	UBlueprint* NewBP = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		Package,
		FName(*OutAssetName),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		NAME_None);

	if (!NewBP)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("BPFailed", "FKismetEditorUtilities::CreateBlueprint returned null. Aborting."));
		return;
	}

	// ---- 5. Add one UStaticMeshComponent per source actor ----------------------

	USimpleConstructionScript* SCS = NewBP->SimpleConstructionScript;
	check(SCS);

	USCS_Node* RootNode = SCS->GetDefaultSceneRootNode();
	if (!RootNode)
	{
		// Shouldn't happen for an AActor-based Blueprint, but guard anyway.
		RootNode = SCS->CreateNode(USceneComponent::StaticClass(), TEXT("DefaultSceneRoot"));
		SCS->AddNode(RootNode);
	}

	// Converts an actor's editor label to a unique, valid SCS variable name.
	TSet<FName> UsedVarNames;
	auto MakeSCSVarName = [&UsedVarNames](const FString& Label) -> FName
	{
		FString Sanitized;
		for (TCHAR Ch : Label)
		{
			Sanitized += (FChar::IsAlnum(Ch) || Ch == TEXT('_')) ? Ch : TEXT('_');
		}
		if (Sanitized.IsEmpty() || FChar::IsDigit(Sanitized[0]))
		{
			Sanitized.InsertAt(0, TEXT('_'));
		}
		FString Unique = Sanitized;
		for (int32 N = 1; UsedVarNames.Contains(FName(*Unique)); ++N)
		{
			Unique = FString::Printf(TEXT("%s_%d"), *Sanitized, N);
		}
		UsedVarNames.Add(FName(*Unique));
		return FName(*Unique);
	};

	for (int32 i = 0; i < MeshActors.Num(); ++i)
	{
		AStaticMeshActor* MeshActor = MeshActors[i];
		UStaticMeshComponent* SourceComp = MeshActor->GetStaticMeshComponent();
		if (!SourceComp)
		{
			continue;
		}

		const FName VarName = MakeSCSVarName(MeshActor->GetActorLabel());
		USCS_Node* NewNode = SCS->CreateNode(UStaticMeshComponent::StaticClass(), VarName);
		UStaticMeshComponent* NewComp = CastChecked<UStaticMeshComponent>(NewNode->ComponentTemplate);

		// Copy the static mesh reference and per-slot materials.
		NewComp->SetStaticMesh(SourceComp->GetStaticMesh());
		for (int32 MatIdx = 0; MatIdx < SourceComp->GetNumMaterials(); ++MatIdx)
		{
			NewComp->SetMaterial(MatIdx, SourceComp->GetMaterial(MatIdx));
		}

		// Position each component relative to the selection centroid so the
		// Blueprint's root lands at the centroid when placed in the world.
		const FTransform ActorXf   = MeshActor->GetActorTransform();
		const FVector    RelLocation = ActorXf.GetLocation() - Centroid;
		NewComp->SetRelativeTransform(FTransform(ActorXf.GetRotation(), RelLocation, ActorXf.GetScale3D()));

		RootNode->AddChildNode(NewNode);
	}

	// ---- 6. Add one UDecalComponent per source decal actor --------------------

	for (int32 i = 0; i < DecalActors.Num(); ++i)
	{
		ADecalActor* DecalActor = DecalActors[i];
		UDecalComponent* SourceDecal = DecalActor->GetDecal();
		if (!SourceDecal)
		{
			continue;
		}

		const FName VarName = FName(*FString::Printf(TEXT("DecalComp%d"), i));
		USCS_Node* NewNode = SCS->CreateNode(UDecalComponent::StaticClass(), VarName);
		UDecalComponent* NewDecal = CastChecked<UDecalComponent>(NewNode->ComponentTemplate);

		NewDecal->SetDecalMaterial(SourceDecal->GetDecalMaterial());
		NewDecal->DecalSize = SourceDecal->DecalSize;
		NewDecal->SortOrder = SourceDecal->SortOrder;

		const FTransform ActorXf    = DecalActor->GetActorTransform();
		const FVector    RelLocation = ActorXf.GetLocation() - Centroid;
		NewDecal->SetRelativeTransform(FTransform(ActorXf.GetRotation(), RelLocation, ActorXf.GetScale3D()));

		RootNode->AddChildNode(NewNode);
	}

	// ---- 7. Compile and register with the asset registry ----------------------

	FKismetEditorUtilities::CompileBlueprint(NewBP);

	FAssetRegistryModule::AssetCreated(NewBP);
	Package->MarkPackageDirty();

	// ---- 7. Report success ----------------------------------------------------

	FNotificationInfo Info(FText::Format(
		LOCTEXT("SuccessFmt", "Blueprint '{0}' created — {1} mesh(es), {2} decal(s). Use File > Save All to persist."),
		FText::FromString(OutAssetName),
		FText::AsNumber(MeshActors.Num()),
		FText::AsNumber(DecalActors.Num())));
	Info.ExpireDuration = 15.0f;
	Info.bFireAndForget = true;
	Info.bUseSuccessFailIcons = true;
	FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Success);
}

#undef LOCTEXT_NAMESPACE
