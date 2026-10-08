# GFX Agent Guidance

Read the repository [AGENT.md](../../AGENT.md) first. This file records gfx-specific contracts and source entry points.

## Abstraction Boundaries

- `hitagi::gfx` unifies DX12 and Vulkan through `gfx.base`; `gfx.mock` supports CPU-side tests. `gfx::create_device()` selects the backend. The `gfx` aggregate exports the base API and render graph.
- `hitagi::rg` describes resource accesses and pass dependencies. Its public concepts are resources, passes, and access edges. A gfx View is an execution object for an access, not a separate graph node.
- `GPUBuffer` owns byte storage; `GPUBufferView` describes a range and element layout. Multiple views may overlap. Creating a view must not rearrange stored data.
- Keep binding, mapping, and descriptor ownership with gfx objects; keep dependency construction, scheduling, and retention through GPU completion with the graph. Reuse gfx descriptions without duplicating their field definitions in rg.

## Source Entry Points

Declarations are concentrated in the files below; do not assume separate type/resource/command module partitions exist.

| Area | Declarations / implementation |
| --- | --- |
| Shared API, resource/view descriptions, synchronization, shader compiler | [base/device.cppm](base/device.cppm), [base/gpu_resource.cpp](base/gpu_resource.cpp), [base/shader_compiler.cpp](base/shader_compiler.cpp) |
| Backend factory and direct readback helper | [gfx.cpp](gfx.cpp) |
| DX12 declarations and enum conversions | [dx12/dx12_device.cppm](dx12/dx12_device.cppm) |
| DX12 resources, commands, bindings | [dx12_resource.cpp](dx12/dx12_resource.cpp), [dx12_command_list.cpp](dx12/dx12_command_list.cpp), [dx12_bindless.cpp](dx12/dx12_bindless.cpp) |
| Vulkan declarations, requirements, enum conversions | [vulkan/vk_device.cppm](vulkan/vk_device.cppm) |
| Vulkan resources, commands, bindings | [vk_resource.cpp](vulkan/vk_resource.cpp), [vk_command_buffer.cpp](vulkan/vk_command_buffer.cpp), [vk_bindless.cpp](vulkan/vk_bindless.cpp) |
| Mock backend | [mock/mock_device.cppm](mock/mock_device.cppm), [mock/mock_device.cpp](mock/mock_device.cpp) |
| Graph API, handles, nodes, edges, builders | [render_graph/render_graph.cppm](render_graph/render_graph.cppm) |
| Access declarations and pass execution | [pass_builder.cpp](render_graph/pass_builder.cpp), [pass_node.cpp](render_graph/pass_node.cpp) |
| Compilation, execution, reset, resource pool | [render_graph.cpp](render_graph/render_graph.cpp), [resource_node.cpp](render_graph/resource_node.cpp) |
| Tests | [render_graph_test.cpp](test/render_graph_test.cpp), [device_test.cpp](test/device_test.cpp), [bindless_test.cpp](test/bindless_test.cpp) |

## Resource, View, and Binding Contracts

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
- Vulkan currently requires descriptor-heap and related features with no descriptor-set fallback. Consult `required_device_extensions` and feature setup in [vk_device.cppm](vulkan/vk_device.cppm) and [vk_device.cpp](vulkan/vk_device.cpp) before changing compatibility requirements.
- DX12 uses a device-local Agility SDK factory/configuration. Do not reintroduce process-wide device/debug configuration as a workaround.

## Changes and Verification

- For shared API changes, trace DX12, Vulkan, Mock, graph callers, and shader consumers as applicable. Keep backend-specific behavior behind the shared contract.
- For resource/handle changes, verify ownership, stale or wrong-pass handles, range/stride interpretation, overlapping accesses, and GPU retention where affected.
- For backend or shader behavior, Mock-only tests cannot establish GPU correctness. Use relevant backend tests and shader compilation, and report unavailable coverage.
- gfx tests are part of the root `unit_tests` target. Use `--gtest_list_tests` and filter actual suite names; follow the root build/validation guidance.
- Update this file when contracts or source entry points change. Keep mapping tables and implementation inventories in source rather than duplicating them here.
