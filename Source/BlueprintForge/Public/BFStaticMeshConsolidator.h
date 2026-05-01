// Copyright God's Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * FBFStaticMeshConsolidator
 *
 * Takes all selected AStaticMeshActor instances in the Level Editor and
 * consolidates them into a single Blueprint asset whose base class is AActor.
 * Each source actor becomes a UStaticMeshComponent on the Blueprint's SCS,
 * positioned relative to the centroid of the selection.
 */
class FBFStaticMeshConsolidator
{
public:
	/** Execute the consolidation. Bound to the "Create Blueprint From Selected" command. */
	static void Execute();

	/** Returns true when at least one AStaticMeshActor is selected. */
	static bool CanExecute();
};
