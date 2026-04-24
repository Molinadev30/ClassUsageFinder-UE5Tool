# Class Usage Finder

An Unreal Engine 5.7 editor plugin that tells you every Data Asset, Game Feature, Experience, Ability Set, or Blueprint that references a given C++ class.

Unreal's built-in Reference Viewer works on asset-to-asset references, so it can't answer the common question "which experiences / ability sets / data assets actually point at my `UMyAbility` class?" This plugin fills that gap by walking reflected properties (`TSubclassOf<T>`, `TSoftClassPtr<T>`, instanced sub-objects, nested structs, arrays, maps, sets) and reporting the exact property path where the match was found.

## Install

1. Copy the `ClassUsageFinder` folder into your project's `Plugins/` directory.
2. Regenerate project files (right-click `.uproject` -> "Generate Visual Studio project files").
3. Build your project. The plugin compiles alongside it.
4. Launch the editor. Open via **Tools -> Class Usage Finder** or via the **Window -> Tools** workspace menu.

The plugin only loads in Editor builds and never ships with a packaged game.

## Usage

1. Click the `<pick a class>` button and choose a C++ class (any `UObject`-derived class is valid).
2. Toggle options as needed:
   - **Include subclasses** - also match references to classes that derive from the target.
   - **Scan Data Assets** / **Scan Blueprints** - narrow what the scan enumerates.
   - **Resolve soft refs** - load soft-class references during scan so subclass checks work on them. Disable for faster scans where only exact matches matter.
3. Click **Scan Project**. Progress dialog appears; you can cancel.
4. Results list shows Asset / Asset Class / Property Path / Matched Class / Ref kind (hard or soft).
5. Double-click a row (or press **Open**) to open the asset in its editor. Press **Browse** to reveal it in the Content Browser.
6. **Export CSV** dumps the full result table to a file.

## What it detects

- `TSubclassOf<T>` properties (hard class refs).
- `TSoftClassPtr<T>` properties (soft class refs).
- Soft object refs whose property type is a `UClass` subclass.
- Class refs nested inside arrays, sets, maps, structs, and editor-instanced sub-objects.
- References in `UDataAsset`-derived assets (Game Feature Data, Experiences, Ability Sets, etc.) and in Blueprint class defaults.

## Limitations

- Full project scan loads every candidate asset through the Asset Registry - expect it to take a while on large projects. Progress is reported and cancellable.
- Soft class refs that can't be resolved (missing assets) are skipped silently.
- References stored inside level actors, packaged build data, or non-reflected fields are not detected.
- Not yet: an async / incremental index. First implementation prioritises correctness over speed.

## Roadmap

- Async indexed scan with save-time invalidation.
- Right-click "Find Usages" on C++ classes from Content Browser and Class Viewer.
- Reverse mode: given an asset, list every C++ class it references.

## Licence

MIT. Share freely.
