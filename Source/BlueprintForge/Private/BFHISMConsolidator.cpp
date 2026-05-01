// Copyright God's Studio. All Rights Reserved.

#include "BFHISMConsolidator.h"

#include "Editor.h"
#include "Engine/Selection.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DecalActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DecalComponent.h"
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

#define LOCTEXT_NAMESPACE "FBFHISMConsolidator"

bool FBFHISMConsolidator::CanExecute()
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

void FBFHISMConsolidator::Execute()
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
	for (const ADecalActor* Actor : DecalActors)
	{
		Centroid += Actor->GetActorLocation();
	}
	Centroid /= static_cast<double>(MeshActors.Num() + DecalActors.Num());

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

	int32 HISMCount = 0;

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

				const FName VarName = MakeSCSVarName(Actor->GetActorLabel());
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
			}
		}
	}

	// ---- 7. Add one UDecalComponent per source decal actor --------------------
	// Decals have no instanced equivalent so each always gets its own component.

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

	// ---- 8. Compile and register with the asset registry ----------------------

	FKismetEditorUtilities::CompileBlueprint(NewBP);

	FAssetRegistryModule::AssetCreated(NewBP);
	Package->MarkPackageDirty();

	// ---- 8. Report success ----------------------------------------------------

	FNotificationInfo Info(FText::Format(
		LOCTEXT("SuccessFmt", "Blueprint '{0}' created — {1} mesh(es) ({2} as HISM), {3} decal(s). Use File > Save All to persist."),
		FText::FromString(OutAssetName),
		FText::AsNumber(MeshActors.Num()),
		FText::AsNumber(HISMCount),
		FText::AsNumber(DecalActors.Num())));
	Info.ExpireDuration = 15.0f;
	Info.bFireAndForget = true;
	Info.bUseSuccessFailIcons = true;
	FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(SNotificationItem::CS_Success);
}

#undef LOCTEXT_NAMESPACE
