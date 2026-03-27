# Blueprint Copilot

**Created by God's Studio**
Unreal Engine 5.7 — Editor Plugin

---

## Overview

Blueprint Copilot is an Unreal Engine editor plugin that automates the conversion of selected Static Mesh Actors in the Level Editor into reusable Blueprint assets. It eliminates the manual work of setting up Blueprint component hierarchies by building them directly from your level selection — with an optional optimization pass that uses Hierarchical Instanced Static Mesh components to reduce draw calls.

---

## Features

### Create Blueprint From Selected
Consolidates any number of selected `AStaticMeshActor` instances into a single Blueprint asset.

- Each selected actor becomes a `UStaticMeshComponent` on the Blueprint's construction script, preserving its static mesh, materials, and transform.
- All components are positioned relative to the centroid of the selection, so the Blueprint's root lands at the center of the group when placed back in the world.
- A Content Browser save dialog lets you choose the destination folder and asset name before anything is created.
- The Blueprint is compiled automatically after generation.
- A toast notification confirms success — no blocking dialogs.

### Create Blueprint From Selected Optimized
The same workflow as above, but with an additional draw-call optimization pass.

- Static meshes that appear on **more than two** selected actors are grouped into a single `UHierarchicalInstancedStaticMeshComponent` (HISM), with each actor's transform added as an instance.
- Static meshes used by only one or two actors are kept as individual `UStaticMeshComponent` nodes.
- The success notification reports how many mesh types were promoted to HISM components.
- All other behavior (save dialog, centroid positioning, auto-compile, toast notification) is identical to the standard command.

### Context Menu Integration
Both commands appear in the **right-click context menu** of the Level Editor viewport under a **Blueprint Copilot** section, and are also available under **Tools > Blueprint Copilot** in the main menu bar. The commands are only enabled when at least one `AStaticMeshActor` is selected.

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

1. Select one or more **Static Mesh Actors** in the Level Editor viewport.
2. Right-click any selected actor to open the context menu.
3. Under the **Blueprint Copilot** section, choose:
   - **Create Blueprint From Selected** — standard one-component-per-actor Blueprint.
   - **Create Blueprint From Selected Optimized** — same, but repeated meshes are batched into HISM components.
4. In the Content Browser save dialog, choose a destination folder and asset name, then click **Save**.
5. The Blueprint is created and compiled. A notification in the editor confirms success.
6. Use **File > Save All** to persist the new asset to disk.

---

## Notes

- Generated Blueprints use `AActor` as their base class.
- The plugin is editor-only (`"Type": "Editor"`) and has no runtime cost.
- If you cancel the save dialog, no asset is created.
- Error conditions (no valid selection, package creation failure) are reported via a blocking dialog so they are not missed.
