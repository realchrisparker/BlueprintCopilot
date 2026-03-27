// Copyright God's Studio. All Rights Reserved.

#include "BCHISMConsolidator.h"

#include "Editor.h"
#include "Engine/Selection.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
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

#define LOCTEXT_NAMESPACE "FBCHISMConsolidator"

bool FBCHISMConsolidator::CanExecute()
{
	if (!GEditor)
	{
		return false;
	}

	USelection* Selection = GEditor->GetSelectedActors();
	for (FSelectionIterator It(*Selection); It; ++It)
	{
		if (Cast<AStaticMeshActor>(*It))
		{
			return true;
		}
	}
	return false;
}

void FBCHISMConsolidator::Execute()
{
	if (!GEditor)
	{
		return;
	}

	// ---- 1. Gather selected AStaticMeshActors ----------------------------------

	TArray<AStaticMeshActor*> MeshActors;
	USelection* Selection = GEditor->GetSelectedActors();
	for (FSelectionIterator It(*Selection); It; ++It)
	{
		if (AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(*It))
		{
			MeshActors.Add(MeshActor);
		}
	}

	if (MeshActors.Num() == 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("NoMeshActors", "No Static Mesh Actors are selected.\nSelect one or more AStaticMeshActor instances and try again."));
		return;
	}

	// ---- 2. Group actors by their UStaticMesh asset ----------------------------

	TMap<UStaticMesh*, TArray<AStaticMeshActor*>> MeshGroups;
	for (AStaticMeshActor* Actor : MeshActors)
	{
		UStaticMeshComponent* Comp = Actor->GetStaticMeshComponent();
		if (Comp && Comp->GetStaticMesh())
		{
			MeshGroups.FindOrAdd(Comp->GetStaticMesh()).Add(Actor);
		}
	}

	// ---- 3. Compute centroid (used as the Blueprint's world origin) ------------

	FVector Centroid = FVector::ZeroVector;
	for (const AStaticMeshActor* Actor : MeshActors)
	{
		Centroid += Actor->GetActorLocation();
	}
	Centroid /= static_cast<double>(MeshActors.Num());

	// ---- 4. Ask the user where to save the Blueprint ---------------------------

	FSaveAssetDialogConfig SaveConfig;
	SaveConfig.DialogTitleOverride = LOCTEXT("SaveDialogTitle", "Save Optimized Blueprint");
	SaveConfig.DefaultPath        = TEXT("/Game/Blueprints");
	SaveConfig.DefaultAssetName   = TEXT("BP_OptimizedMeshes");
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

	// ---- 5. Create the UPackage and Blueprint asset ----------------------------

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

	// ---- 6. Build SCS nodes ----------------------------------------------------
	// Meshes used by more than 2 actors → one UHierarchicalInstancedStaticMeshComponent.
	// Meshes used by 1 or 2 actors      → individual UStaticMeshComponents.

	USimpleConstructionScript* SCS = NewBP->SimpleConstructionScript;
	check(SCS);

	USCS_Node* RootNode = SCS->GetDefaultSceneRootNode();
	if (!RootNode)
	{
		RootNode = SCS->CreateNode(USceneComponent::StaticClass(), TEXT("DefaultSceneRoot"));
		SCS->AddNode(RootNode);
	}

	int32 NodeIndex = 0;
	int32 HISMCount = 0;

	for (auto& Pair : MeshGroups)
	{
		UStaticMesh* Mesh = Pair.Key;
		TArray<AStaticMeshActor*>& Actors = Pair.Value;

		if (Actors.Num() > 2)
		{
			// --- HISM path ---
			const FName VarName = FName(*FString::Printf(TEXT("HISM_%s"), *Mesh->GetName()));
			USCS_Node* NewNode = SCS->CreateNode(UHierarchicalInstancedStaticMeshComponent::StaticClass(), VarName);
			UHierarchicalInstancedStaticMeshComponent* HISM =
				CastChecked<UHierarchicalInstancedStaticMeshComponent>(NewNode->ComponentTemplate);

			HISM->SetStaticMesh(Mesh);

			// Copy per-slot materials from the first actor in the group.
			UStaticMeshComponent* SourceComp = Actors[0]->GetStaticMeshComponent();
			for (int32 MatIdx = 0; MatIdx < SourceComp->GetNumMaterials(); ++MatIdx)
			{
				HISM->SetMaterial(MatIdx, SourceComp->GetMaterial(MatIdx));
			}

			// Add one instance per actor, transform relative to the centroid.
			for (AStaticMeshActor* Actor : Actors)
			{
				const FTransform& WorldXf = Actor->GetActorTransform();
				const FVector RelLocation = WorldXf.GetLocation() - Centroid;
				HISM->AddInstance(FTransform(WorldXf.GetRotation(), RelLocation, WorldXf.GetScale3D()));
			}

			RootNode->AddChildNode(NewNode);
			++HISMCount;
		}
		else
		{
			// --- Individual UStaticMeshComponent path ---
			for (AStaticMeshActor* Actor : Actors)
			{
				UStaticMeshComponent* SourceComp = Actor->GetStaticMeshComponent();
				if (!SourceComp)
				{
					continue;
				}

				const FName VarName = FName(*FString::Printf(TEXT("StaticMeshComp%d"), NodeIndex));
				USCS_Node* NewNode = SCS->CreateNode(UStaticMeshComponent::StaticClass(), VarName);
				UStaticMeshComponent* NewComp = CastChecked<UStaticMeshComponent>(NewNode->ComponentTemplate);

				NewComp->SetStaticMesh(SourceComp->GetStaticMesh());
				for (int32 MatIdx = 0; MatIdx < SourceComp->GetNumMaterials(); ++MatIdx)
				{
					NewComp->SetMaterial(MatIdx, SourceComp->GetMaterial(MatIdx));
				}

				const FTransform ActorXf = Actor->GetActorTransform();
				const FVector RelLocation = ActorXf.GetLocation() - Centroid;
				NewComp->SetRelativeTransform(FTransform(ActorXf.GetRotation(), RelLocation, ActorXf.GetScale3D()));

				RootNode->AddChildNode(NewNode);
				++NodeIndex;
			}
		}
	}

	// ---- 7. Compile and register with the asset registry ----------------------

	FKismetEditorUtilities::CompileBlueprint(NewBP);

	FAssetRegistryModule::AssetCreated(NewBP);
	Package->MarkPackageDirty();

	// ---- 8. Report success ----------------------------------------------------

	FNotificationInfo Info(FText::Format(
		LOCTEXT("SuccessFmt", "Blueprint '{0}' created from {1} actor(s) — {2} mesh type(s) as HISM. Use File > Save All to persist."),
		FText::FromString(OutAssetName),
		FText::AsNumber(MeshActors.Num()),
		FText::AsNumber(HISMCount)));
	Info.ExpireDuration = 15.0f;
	Info.bFireAndForget = true;
	Info.bUseSuccessFailIcons = true;
	FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Success);
}

#undef LOCTEXT_NAMESPACE
