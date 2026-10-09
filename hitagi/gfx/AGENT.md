# GFX Agent Guidance

Read the repository [AGENT.md](../../AGENT.md) first. This file records gfx-specific contracts and source entry points.

## Abstraction Boundaries

- `hitagi::gfx` unifies DX12 and Vulkan through `gfx.base`; `gfx.mock` supports CPU-side tests. `gfx::create_device()` selects the backend. The `gfx` aggregate exports the base API and render graph.
- `hitagi::rg` describes resource accesses and pass dependencies. Its public concepts are resources, passes, and access edges. A gfx View is an execution object for an access, not a separate graph node.
- `GPUBuffer` owns byte storage; `GPUBufferView` describes a range and element layout. Multiple views may overlap. Creating a view must not rearrange stored data.
- Keep binding, mapping, and descriptor ownership with gfx objects; keep dependency construction, scheduling, and retention through GPU completion with the graph. Reuse gfx descriptions without duplicating their field definitions in rg.

## Source Entry Points

Each named module keeps one aggregate `.cppm` and uses `.cpp` interface partitions, following asset's organization. Partition filenames follow the layout before `68d48b5c`. Export declarations with `export namespace`; place out-of-class implementations and internal helpers in ordinary named namespaces. Do not add implementation directories, `impl.cpp`, or per-partition `.cppm` files. Complete classes live in their responsibility-specific files; `dx12_types.cpp` / `vk_types.cpp` contain forward declarations only. `RenderGraph` lives in `render_graph.cpp`, together with methods that assemble nodes/builders through the complete graph definition. Inspect module declarations rather than inferring a file's role from its extension.

Static resource factories are defined in the ordinary module implementation unit [base/resource_creation.cpp](base/resource_creation.cpp). It imports the backend interfaces without making the base interface depend on them. Do not move those definitions into a base interface partition, which would introduce a module dependency cycle.

External dependency modules are provided by the independent `hitagi_interop` target in [interop/XMake](../interop/xmake.lua), named `interop.<library_name>`. [interop.fmt](../interop/fmt.cppm) and [interop.spdlog](../interop/spdlog.cppm) export their respective library entities; import fmt explicitly when using it, since spdlog does not re-export it. Windows-only [interop.dx12](../interop/dx12.cppm) exports DX12/DXGI/WRL entities and typed macro adapters in `hitagi::interop`; the separate [interop.d3d12ma](../interop/d3d12ma.cppm) exports the allocator library. These modules must not import engine modules. [interop.vulkan](../interop/vulkan.cppm) and [interop.magic_enum](../interop/magic_enum.cppm) re-export the official upstream modules without copying their implementations. Keep `VULKAN_HPP_NO_CONSTRUCTORS` and platform definitions public so producers and importers agree. Do not reintroduce Vulkan-Hpp textual includes alongside that module. [interop.vma](../interop/vma.cppm) exports VMA declarations; its sole implementation is [interop/vma.cpp](../interop/vma.cpp), not a public module source. Tracy types come from `interop.tracy`, `interop.tracy.vulkan`, and Windows-only `interop.tracy.dx12`; its [thin macro header](../interop/tracy_macros.hpp) preserves call-site scope, source locations and disabled argument elision without including Tracy headers. Consumers must not include VMA or Tracy headers directly. Raw Vulkan C entities are also exported by `interop.vulkan`; `interop.sdl`, `interop.win32`, `interop.dxc`, and `interop.spirv_reflect` cover the remaining native APIs. Only the thin `win32_macros.hpp` / `dxc_macros.hpp` retain call-site COM type deduction. Vendor SDK headers must not be included in gfx consumers.

| Area | Declarations / implementation |
| --- | --- |
| Shared API aggregate and Device | [base/device.cppm](base/device.cppm), [base/device.cpp](base/device.cpp) |
| Shared types, resources/views, synchronization, bindings | [base/types.cpp](base/types.cpp), [base/gpu_resource.cpp](base/gpu_resource.cpp), [base/sync.cpp](base/sync.cpp), [base/bindless.cpp](base/bindless.cpp) |
| Shared commands, shader compiler, utilities | [base/command_context.cpp](base/command_context.cpp), [base/command_queue.cpp](base/command_queue.cpp), [base/shader_compiler.cpp](base/shader_compiler.cpp), [base/utils.cpp](base/utils.cpp) |
| Backend factory and direct readback helper | [gfx.cpp](gfx.cpp) |
| DX12 aggregate, shared declarations and conversions | [dx12/dx12_device.cppm](dx12/dx12_device.cppm), [dx12_types.cpp](dx12/dx12_types.cpp), [dx12_utils.cpp](dx12/dx12_utils.cpp) |
| DX12 resources, commands and bindings | [dx12_resource.cpp](dx12/dx12_resource.cpp), [dx12_command_list.cpp](dx12/dx12_command_list.cpp), [dx12_bindless.cpp](dx12/dx12_bindless.cpp) |
| Vulkan aggregate, requirements, conversions | [vulkan/vk_device.cppm](vulkan/vk_device.cppm), [vk_configs.cpp](vulkan/vk_configs.cpp), [vk_utils.cpp](vulkan/vk_utils.cpp) |
| Vulkan shared declarations, resources, commands and bindings | [vk_types.cpp](vulkan/vk_types.cpp), [vk_resource.cpp](vulkan/vk_resource.cpp), [vk_command_buffer.cpp](vulkan/vk_command_buffer.cpp), [vk_bindless.cpp](vulkan/vk_bindless.cpp) |
| Mock backend | [mock/mock_device.cppm](mock/mock_device.cppm), [mock/mock_resource.cpp](mock/mock_resource.cpp), [mock/mock_device.cpp](mock/mock_device.cpp) |
| Graph aggregate, handles and edges | [render_graph/render_graph.cppm](render_graph/render_graph.cppm), [type.cpp](render_graph/type.cpp), [resource_edge.cpp](render_graph/resource_edge.cpp) |
| Graph nodes and builders | [resource_node.cpp](render_graph/resource_node.cpp), [pass_node.cpp](render_graph/pass_node.cpp), [pass_builder.cpp](render_graph/pass_builder.cpp) |
| Compilation, execution, reset, resource pool | [render_graph.cpp](render_graph/render_graph.cpp), [resource_node.cpp](render_graph/resource_node.cpp) |
| Tests | [render_graph_test.cpp](test/render_graph_test.cpp), [device_test.cpp](test/device_test.cpp), [bindless_test.cpp](test/bindless_test.cpp) |

## Resource, View, and Binding Contracts

- Resources retain their descriptions through `ResourceWithDesc`, but do not retain an abstract `Device` or expose `GetDevice()`. Use the resource type's static `Create` method; the assembly boundary selects the backend and validates native ownership before injecting concrete dependencies.
- Backend resources depend on the native device, allocator, binding facilities, logger, or compiler that they actually use, not the complete backend Device. Texture initialization that needs command submission is assembled in `resource_creation.cpp`; allocation and mapping behavior remain backend responsibilities.
- Device owns native bootstrap state, allocators, and capabilities. The Engine owns `CommandQueues`, bindings, and the shader compiler through its graphics services module, created after Device and destroyed before it. Consumers receive explicit references; low-level resources must not receive this owner as a service locator. Tests must provide the same lifetime ordering.
- Queues own execution synchronization and profiling state; Vulkan queues also own their command pools. Command contexts and submitted resources must not outlive the facilities they reference. Bindings must outlive all views and samplers using their handles.
- `GPUBufferDesc::size` is a byte count. `element_size`, `element_stride`, and binding offset alignment have different meanings. A zero ViewDesc stride means tightly packed elements; Storage usage does not imply record padding.
- Query storage binding constraints with `GPUBuffer::GetStorageViewRequirements(device)` when planning independently bound ranges. Backend differences stay behind that API; the caller determines data placement.
- `MappedSpan<T>` is created only by `view.GetMappedSpan<T>()`. It follows the View's explicit stride, validates mapping/type constraints, and unmaps through RAII. It does not construct a temporary View from a Buffer. Keep its underlying storage alive throughout the mapping.
- Strided mappings are not necessarily contiguous. Use indexed/element-wise access when needed; `data()` rejects non-contiguous multi-element spans.
- Buffer/texture views and samplers own their bindless handles. Resolve a graph handle, then use the object's `GetBindlessHandle()`; do not add a parallel PassNode binding API.
- GPU use can outlive CPU command recording. Resources, views, samplers, and descriptors must remain alive until their GPU work completes. Graph retention supplies this for graph accesses; direct gfx callers must provide it. A non-owning reference is valid only while its owner's lifetime is guaranteed.
- Texture attachment descriptors belong to Views: `DX12TextureView` owns RTV/DSV descriptors and `VulkanTextureView` owns its image view.
- Record arrays normally use one View/descriptor with shader indexing. `BindlessMetaInfo` carries a handle, record index, and stride; preserve the CPU/HLSL ABI when changing it. Independent binding alignment does not determine the stride inside an already bound array.
- Shaders use HLSL compiled by DXC to DXIL/SPIR-V. The shared storage path uses raw byte-address buffers. Integer ID textures use integer loads rather than filtered sampling.

## Render Graph Contracts

### Declaration and Resolution

- Declare/import resources and declare their pass accesses before `Compile()`. Physical resources are instantiated lazily; access Views are prepared for execution. Executor callbacks must not mutate the compiled graph or bypass its declared resource dependencies.
- Resource handles identify graph resources. `GPUBufferEdgeHandle` and `TextureEdgeHandle` identify a particular pass-owned access. `pass.Resolve(resource)` returns the resource; `pass.Resolve(edge)` returns that access's View.
- Render/Compute buffer and texture access declarations return edge handles. Configuration setters, sampler declarations, Copy operations, and Present declarations return `void`. Render/Compute/Copy `Finish()` returns a pass handle; Present `Finish()` returns `void`. Keep these builders non-fluent.
- Edges retain gfx ViewDesc values; the physical resource pointer must be empty at declaration because the graph resource handle is authoritative. An edge holds at most one View. Copy accesses need no View, and samplers need no invented SamplerView.
- Multiple accesses to one resource remain dependencies on that resource. Compatible access requirements are merged for barriers; conflicting read/write or texture layouts within a pass are rejected. Synchronization currently has whole-resource granularity.
- `MoveFrom()` creates another graph node for the same physical resource and expresses subsequent use, such as drawing an overlay onto a prior output. It does not provide cross-frame history storage.

### Compilation, Execution, and Lifetime

- Compile keeps enabled Present passes and passes marked `AllowPassCulling(false)`, then traces their dependencies and sorts the graph. Extraction helpers create non-cullable passes, so offscreen graphs can execute without Present. A graph with no retained output can be fully culled.
- Call `Execute()` only after successful `Compile()`. Current CPU command recording is serial. Submission uses per-queue fences; Execute waits for this execution's latest submissions before retirement/reset. Do not describe this as parallel recording or only waiting for the previous frame.
- A RenderGraph object may be reused across frames. Reset clears passes, transient nodes, and dependency edges, but retains imported resource nodes until `ClearImportedResources()`.
- Resource handle slots have no generation counter. Do not reuse transient handles after reset or imported handles after clearing imports. Access handles are specific to their owning pass.
- The transient pool reuses compatible complete resources across frames; it is not within-frame physical memory aliasing. Imported and moved resources are excluded from pooling.
- Graph construction/mutation is currently single-threaded. Pass identity generation also uses a shared non-atomic counter, so even separate graphs must not create passes concurrently. Supporting that requires an explicit concurrency design; a counter alone would not make graph mutation thread-safe.

## Backend and Synchronization Constraints

- `Transition()` both updates tracked resource state and returns the barrier to emit. Do not discard it or call it speculatively.
- Command contexts require `Begin()` / `End()` around recording. Submit each context to a matching Graphics, Compute, or Copy queue.
- Acquire the swapchain image before using it. The Vulkan queue/swapchain implementation manages its binary semaphores; avoid duplicating that management in callers.
- Queue selection is implemented in the Vulkan backend. Do not hard-code queue family indices in calling code.
- Vulkan currently requires descriptor-heap and related features with no descriptor-set fallback. Consult `required_device_extensions` and feature setup in [vk_configs.cpp](vulkan/vk_configs.cpp) and [vk_device.cpp](vulkan/vk_device.cpp) before changing compatibility requirements.
- DX12 uses a device-local Agility SDK factory/configuration. Do not reintroduce process-wide device/debug configuration as a workaround.

## Changes and Verification

- For shared API changes, trace DX12, Vulkan, Mock, graph callers, and shader consumers as applicable. Keep backend-specific behavior behind the shared contract.
- For resource/handle changes, verify ownership, stale or wrong-pass handles, range/stride interpretation, overlapping accesses, and GPU retention where affected.
- For backend or shader behavior, Mock-only tests cannot establish GPU correctness. Use relevant backend tests and shader compilation, and report unavailable coverage.
- gfx tests are part of the root `unit_tests` target. Use `--gtest_list_tests` and filter actual suite names; follow the root build/validation guidance.
- Update this file when contracts or source entry points change. Keep mapping tables and implementation inventories in source rather than duplicating them here.
