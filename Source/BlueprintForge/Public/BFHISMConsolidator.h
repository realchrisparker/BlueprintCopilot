// Copyright God's Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * FBFHISMConsolidator
 *
 * Like FBFStaticMeshConsolidator but optimized: selected AStaticMeshActors whose
 * static mesh appears more than twice are grouped into a single
 * UHierarchicalInstancedStaticMeshComponent.  Meshes used only once or twice keep
 * individual UStaticMeshComponents.  All components land in one Blueprint asset
 * positioned relative to the centroid of the selection.
 */
class FBFHISMConsolidator
{
public:
	/** Execute the optimized consolidation. Bound to "Create Blueprint From Selected Optimized". */
	static void Execute();

	/** Returns true when at least one AStaticMeshActor is selected. */
	static bool CanExecute();
};
