# Blueprint Copilot

**Created by God's Studio**
Unreal Engine 5.7 — Editor Plugin

---

## Overview

Blueprint Copilot is an Unreal Engine editor plugin that automates the conversion of selected Static Mesh Actors and Decal Actors in the Level Editor into reusable Blueprint assets. It eliminates the manual work of setting up Blueprint component hierarchies by building them directly from your level selection — with an optional optimization pass that uses Hierarchical Instanced Static Mesh components to reduce draw calls.

---

## Features

### Create Blueprint From Selected
Consolidates any number of selected `AStaticMeshActor` and `ADecalActor` instances into a single Blueprint asset.

- Each selected Static Mesh Actor becomes a `UStaticMeshComponent`, preserving its static mesh, materials, transform, and editor label (used as the component variable name in the Blueprint).
- Each selected Decal Actor becomes a `UDecalComponent`, preserving its decal material, decal size, and sort order.
- All components are positioned relative to the centroid of the entire selection, so the Blueprint's root lands at the center of the group when placed back in the world.
- A Content Browser save dialog lets you choose the destination folder and asset name before anything is created.
- The Blueprint is compiled automatically after generation.
- A toast notification confirms success and reports the mesh and decal counts — no blocking dialogs.

### Create Blueprint From Selected Optimized
The same workflow as above, but with an additional draw-call optimization pass for static meshes.

- Static meshes that appear on **more than two** selected actors are grouped into a single `UHierarchicalInstancedStaticMeshComponent` (HISM), with each actor's transform added as an instance.
- Static meshes used by only one or two actors are kept as individual `UStaticMeshComponent` nodes, each named after its source actor's editor label.
- Decal Actors always produce individual `UDecalComponent` nodes — there is no instanced equivalent for decals.
- The success notification reports mesh count, how many mesh types were promoted to HISM, and decal count.
- All other behavior (save dialog, centroid positioning, auto-compile, toast notification) is identical to the standard command.

### Context Menu Integration
Both commands appear in the **right-click context menu** of the Level Editor viewport under a **Blueprint Copilot** section, and are also available under **Tools > Blueprint Copilot** in the main menu bar. The commands are enabled when at least one `AStaticMeshActor` or `ADecalActor` is selected.

---

## Requirements

- Unreal Engine 5.7 or later
- A C++ project (the plugin must be compiled alongside your project)

---

## Installation

### 1. Copy the plugin into your project

Copy the `BlueprintCopilot` folder into the `Plugins` directory at the root of your Unreal project. Create the `Plugins` folder if it does not exist.

```
YourProject/
├── Content/
├── Source/
├── Plugins/
│   └── BlueprintCopilot/   ← place the folder here
└── YourProject.uproject
```

### 2. Add the plugin to your .uproject file

Open `YourProject.uproject` in a text editor and add the plugin entry to the `"Plugins"` array:

```json
{
    "Plugins": [
        {
            "Name": "BlueprintCopilot",
            "Enabled": true
        }
    ]
}
```

### 3. Regenerate project files

Right-click your `.uproject` file and select **Generate Visual Studio project files** (Windows), or run `UnrealBuildTool` from the command line.

### 4. Build the project

Open the solution in Visual Studio (or Rider) and build in **Development Editor** configuration. The plugin module is editor-only and will not be included in shipping builds.

### 5. Enable the plugin in the editor (if needed)

If the plugin does not load automatically, open the editor, go to **Edit > Plugins**, search for **Blueprint Copilot**, and enable it. Restart the editor when prompted.

---

## Usage

1. Select any combination of **Static Mesh Actors** and **Decal Actors** in the Level Editor viewport.
2. Right-click any selected actor to open the context menu.
3. Under the **Blueprint Copilot** section, choose:
   - **Create Blueprint From Selected** — one component per actor, meshes as `UStaticMeshComponent`, decals as `UDecalComponent`.
   - **Create Blueprint From Selected Optimized** — same, but static meshes used more than twice are batched into a single `UHierarchicalInstancedStaticMeshComponent`.
4. In the Content Browser save dialog, choose a destination folder and asset name, then click **Save**.
5. The Blueprint is created and compiled. A toast notification confirms success and shows the mesh and decal counts.
6. Use **File > Save All** to persist the new asset to disk.

---

## Notes

- Generated Blueprints use `AActor` as their base class.
- Individual `UStaticMeshComponent` variable names are derived from the actor's editor label. Spaces and invalid identifier characters are replaced with underscores. Duplicate names are disambiguated with a numeric suffix (e.g. `Rock_1`, `Rock_2`). HISM and decal component names are not affected.
- The plugin is editor-only (`"Type": "Editor"`) and has no runtime cost.
- If you cancel the save dialog, no asset is created.
- Error conditions (no valid selection, package creation failure) are reported via a blocking dialog so they are not missed.
- Decal properties copied per component: decal material, decal size (projection box extents), and sort order.
