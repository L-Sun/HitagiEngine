# Hitagi Engine Overall Abstraction Design

Hitagi separates reusable runtime facilities, reusable editing facilities, game runtime policy, and the application that combines them. The four roles are **Engine**, **Editor Framework**, **Game**, and **Game Editor**. The Editor Framework and game share engine abstractions and remain independent of each other; the game editor connects their capabilities for a particular game.

This separation lets a game use its own renderer, material contracts, and asset conversion rules while reusing the engine and editor framework. A runtime distribution needs engine, game, and the required cooked assets and external resource files. Authoring dependencies stay on the editor side.

## Dependency structure

The diagram arranges code responsibilities in layers. Game Editor integrates the two independent branches, Editor Framework and Game; both use Engine as their shared foundation. This is a build-dependency view, not asset data flow or runtime object ownership.

![Layered architecture: Game Editor integrates independent Editor Framework and Game branches above their shared Engine foundation.](../assets/architecture_dependencies.svg)

Equivalently:

```text
Engine <- Editor Framework <- Game Editor
Engine <- Game             <- Game Editor
```

The game editor can also use engine APIs directly. Engine has no dependency on editor or game, and game has no dependency on editor. Game-specific integration belongs at the top, where both sets of APIs are available.

| Role | Owns | Representative code |
| --- | --- | --- |
| engine | Runtime services, ECS, physics, graphics and rendering primitives, GUI, runtime resources and their loading contracts | [`hitagi/engine`](../../hitagi/engine/engine.cppm) and the modules it assembles |
| Editor Framework | Reusable editing UI, viewports, inspectors, commands, source import, material graph and validation tools, and Cook APIs | [`hitagi/editor`](../../hitagi/editor/editor.cppm) |
| game | Gameplay, game-specific resource semantics, renderer composition, and material/pass policy | [`pbr_demo_game`](../../examples/pbr_demo/pbr_demo.cppm) |
| game editor | Application assembly, game-specific panels, source-to-game asset conversion, and game asset build workflows | [PBR editor entry point](../../examples/pbr_demo/editor.cpp), [Snake editor entry point](../../examples/snake/editor.cpp) |

## Editor Framework and editor applications

The Editor Framework provides reusable editing workflows and extension points for game editor applications. Its scope includes both interactive editing and tools that can run without a UI. Asset importers and cookers belong alongside generic editing capabilities because they operate on authoring data.

The `hitagi_editor` build target is a static library depending on `engine` and editor-only third-party bridges. The `editor` executable in `examples/editor` is a generic host: it creates an `Engine`, attaches a `hitagi::Editor`, and runs the engine loop. The generic editor host explicitly selects the deferred preset. A game editor is another host that additionally composes game-specific behavior.

Editor Framework names the architectural layer. The C++ module remains `editor`, and its build target remains `hitagi_editor`. These names describe different things:

| Name | Meaning |
| --- | --- |
| Editor Framework | The architectural layer providing reusable editing capabilities and extension points |
| `export module editor` | The C++ module exposing generic editor APIs |
| `hitagi_editor` | The static-library build target containing those APIs |
| `hitagi::Editor` | A `core::RuntimeModule` instance that runs the interactive editor inside an engine instance |
| `editor` | The generic editor executable target |
| `pbr-demo-game-editor` | An application combining the generic editor library and PBR game library |

A `RuntimeModule` is a unit in the engine's runtime module tree. Attaching an editor or game object through `Engine::AddSubModule` gives the engine an instance to own and tick; it does not introduce a compile-time dependency on that object's concrete type into engine.

Namespace spelling is also separate from architectural ownership. Some editor types currently live in `hitagi::`, while PBR game types live in `game::pbr_demo`. Their build dependencies and responsibilities determine their layer.

## How the game editor connects both branches

The [PBR target definitions](../../examples/pbr_demo/xmake.lua) implement the dependency structure directly:

```text
hitagi_preset_pbr    -> engine
hitagi_preset_pbr_authoring -> hitagi_preset_pbr + hitagi_editor
pbr-demo-game-lib    -> engine + hitagi_preset_pbr
hitagi_editor       -> engine + hitagi_interop_editor
pbr-demo-game       -> pbr-demo-game-lib
pbr-demo-game-editor -> pbr-demo-game-lib + hitagi_editor + hitagi_preset_pbr_authoring
```

The game library selects the optional PBR preset and defines game runtime behavior. The preset runtime owns `PbrMaterialDesc`, typed defaults, the `PBRForward` pass contract, and `PbrRenderer`; its authoring companion owns PBR shader generation. Neither Engine nor Editor Framework depends on these presets. Editor Framework exposes generic source import, material graphs, validation, and cook facilities. The PBR game editor supplies the adapter between them: it interprets imported material information using PBR rules and produces the parameters and passes expected by the game's renderer.

The existing editor startup performs this composition:

```cpp
hitagi::AppConfig config = hitagi::LoadEditorAppConfig();
hitagi::EditorLaunchOptions options = hitagi::ParseEditorLaunchOptions(argc, argv);
options.material_processor = MakePbrDemoMaterialProcessor(config.asset_root_path);

hitagi::Engine engine(std::move(config));
engine.SetRenderer(std::make_unique<game::pbr_demo::PbrDemoRenderer>(engine.ResourceLoadContext()));
engine.AddSubModule(std::make_unique<hitagi::Editor>(engine, std::move(options)));
```

This excerpt uses existing APIs; `MakePbrDemoMaterialProcessor` is defined by the [PBR game editor](../../examples/pbr_demo/editor.cpp). The generic USD importer invokes the supplied `MaterialProcessor` callback without depending on PBR code. The same game renderer is installed by the game executable, allowing authoring previews and runtime rendering to use the same game contract.

Game owns the meaning of its materials and passes. The source-format adapter that applies those rules belongs in game editor when it needs editor types such as `EditorMaterial` or `MaterialGraph`. Shared runtime descriptions and rendering rules stay available through game without introducing an editor dependency.

## Shared runtime objects and authoring metadata

Both branches use `asset::Scene`, `Mesh`, `Material`, `Texture`, `Camera`, and `Light`. Source import produces these resource types directly, so the editor can render and manipulate them before cooking. Runtime cooked loading reconstructs the same types. Sharing the model means sharing APIs and semantics; loading a cooked file creates new objects rather than restoring process pointers or editor object identity.

Editor-specific provenance is attached outside the runtime material:

```cpp
struct EditorMaterial {
    std::shared_ptr<asset::Material> material;
    MaterialSourceInfo source_info;
};
```

`MaterialSourceInfo` records source paths, source shader identifiers, source parameters, and unsupported inputs or nodes. `EditorCookContext` associates that information with material objects. The runtime `Material` carries parameter values and `MaterialPass` descriptions. Its resource representation does not require the source document or editable graph.

A `MaterialPass` specifies a pass contract, pipeline, ordered typed bindings with mandatory defaults, and packed material data. Cook rejects missing or mistyped final parameters. Runtime warns and uses contract defaults without changing field layout. The game renderer selects a compatible pass; the asset layer manages its representation and resource loading. This keeps a game-specific shading model out of the generic asset type. See [Materials](../assets/material.md) for the binding contract and shared storage ABI details.

The current editor Play mode clones the entity hierarchy and transforms into another `Scene`, while retaining shared Mesh, Camera, and Light references. It isolates scene structure and transforms, but does not deep-copy the resource graph. Changes to shared resource contents can therefore affect both scenes.

## Cooking and runtime loading

The authoring and runtime sides meet at two contracts: the shared resource model in memory, and the HTGC format used to deliver cooked resources.

```text
Source assets
    -> Generic editor importer + game editor material processing
    -> asset::Scene / Material / Mesh ...
    -> Editor Cook API
    -> HTGC bytes + referenced external files
    -> AssetManager cooked loader
    -> asset::Scene / Material / Mesh ...
    -> Game renderer
```

The Editor Framework provides `CookEditorScene` and `CookEditorMaterial`, which return a `core::Buffer`. Writer and reader share the POD layout declarations in `asset:cooked_format`. The caller chooses output paths, persists the result, and arranges required external files. Texture pixels remain in referenced image files; shader descriptors can carry paths or source text. Source metadata and editable material graphs are not serialized into HTGC.

The PBR game editor already provides a `--cook` command. It assembles memory, file I/O, job, and asset services without opening the interactive editor, imports the source scene with its material processor, calls the generic Cook API, and writes the artifact. This is a game editor application workflow using reusable editor capabilities.

The generic editor's `SaveCurrentScene` UI remains disabled pending scene export. That limitation is separate from the implemented Cook APIs and PBR command-line workflow. Saving an editable source document and producing runtime cooked data are separate persistence contracts; HTGC does not preserve all authoring information needed for a source round trip.

See [Cooked binary format](../assets/cooked_binary_format.md) for the file layout and reader/writer contract.

## Shader ABI and optional rendering policy

Engine owns bindless descriptors, handles, residency, and the raw-storage shader layout API. The PBR preset describes its CPU frame, instance, and draw fields; Engine generates their shader declarations from member types and offsets. Material shader fields are generated from the same typed defaults that govern runtime packing. A game chooses what a draw references, but does not maintain a second handwritten HLSL ABI or manage descriptor indices.

Engine starts with `ClearRenderer`, which only initializes the requested target. The `editor` and `snake-editor` hosts explicitly install the optional `preset.deferred` renderer; the PBR hosts explicitly select `preset.pbr`. The old deferred implementation, G-buffer/lighting passes, concrete material routing, and Toon configuration descriptions now live under `presets/deferred`. Editor selection uses the `IDeferredRenderer` extension capability and the generic `RenderGeometry` / `ViewConstant` contract, not a concrete renderer type.

Rendering models remain optional policy packages. The PBR compiler and lighting code live outside Editor Framework. Generic USD import does not depend on PBR; game editor supplies the conversion adapter. The former material cache and hot-reload placeholder API are removed rather than presented as completed facilities.

## Service composition and resource lifetime

`Engine` assembles and owns runtime infrastructure, including file I/O, jobs, graphics services, and `AssetManager`. The host application supplies configuration, chooses game and editor modules, and installs a renderer. Configuration persistence belongs to the host. A headless tool or test can assemble just the services it needs, as the PBR cooker does.

Consumers receive explicit dependencies. For example, `AssetManager` receives `FileIOManager` and `JobSystem`; resource GPU loading receives a `ResourceLoadContext` containing device, queues, bindings, and shader compiler. The game editor uses the engine instance it owns to connect these services to editor and game code. See [Dependency injection](dependency_injection.md) for injection forms and destruction order.

The asset manager provides lookup, texture deduplication, and loading policy through weak-reference registries. Actual resource ownership follows `shared_ptr` relationships, such as Scene components holding Mesh objects and SubMesh objects holding Materials. GPU residency is controlled by `Resource::Load` and `Unload`, independently of whether the consumer is an editor preview or a running game. See [Resource lifecycle](../assets/resource_lifecycle.md) for asynchronous texture decode and unload coordination.

## Placing new functionality

Use the information a feature must understand to decide its home:

| A feature needs to understand | Place it in |
| --- | --- |
| Generic runtime resources, graphics operations, ECS, or scheduling | engine |
| Generic authoring operations, source import, editor interaction, or cooking mechanics | Editor Framework |
| A game's runtime behavior, shading semantics, or render-pass policy | game |
| Both editor APIs and game-specific rules, including source conversion and custom inspectors | game editor |

Keep editor-only dependencies out of runtime targets. Add a narrow extension point to a reusable layer when its caller must supply game policy, as with `MaterialProcessor`; let game editor provide the implementation. Reuse the shared resource model where its semantics fit, and retain authoring-only state at the editor boundary.
