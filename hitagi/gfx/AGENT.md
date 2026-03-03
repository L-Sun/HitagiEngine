# GFX Module Architecture

> This document is maintained by LLMs working on the gfx module. See [Maintenance](#maintenance) section for update rules.

## Module Overview

`hitagi::gfx` is a graphics device abstraction layer providing a unified API over DX12 and Vulkan backends.
`hitagi::rg` is a frame-scoped render graph built on top of gfx.

**Modules**: `gfx.base` (abstract API) → `gfx.dx12` / `gfx.vulkan` / `gfx.mock` (backends) → `gfx.render_graph`

## File Map

| Directory | Module | Purpose |
|---|---|---|
| `base/` | `gfx.base` | Abstract GPU API — all types, virtual interfaces, shader compiler |
| `dx12/` | `gfx.dx12` | DX12 backend (Windows-only) using D3D12 Agility SDK + D3D12MA |
| `vulkan/` | `gfx.vulkan` | Vulkan backend (Windows + Linux) using Vulkan-Hpp RAII + VMA |
| `mock/` | `gfx.mock` | Null backend for unit tests — all methods are no-ops |
| `render_graph/` | `gfx.render_graph` | Frame-scoped render graph with typed handles and pass builders |
| `test/` | — | GTest-based tests: device_test, render_graph_test, shader_compiler_test, desc_hash_test |

### Key Files per Directory

**`base/`** — 8 module partitions composing `gfx.base`:

| File | Partition | Key Exports |
|---|---|---|
| `types.cppm` | `:types` | `Format`, `GPUBufferUsageFlags`, `TextureUsageFlags`, `ShaderType`, `BarrierAccess`, `PipelineStage`, `TextureLayout`, pipeline states |
| `gpu_resource.cppm` | `:gpu_resource` | `Resource`, `ResourceWithDesc<Desc>` (CRTP), `GPUBuffer`, `Texture`, `Sampler`, `SwapChain`, `Shader`, `RenderPipeline`, `ComputePipeline`, all `*Desc` structs |
| `sync.cppm` | `:sync` | `Fence`, `GlobalBarrier`, `GPUBufferBarrier`, `TextureBarrier`, `FenceSignalInfo`, `FenceWaitInfo` |
| `bindless.cppm` | `:bindless` | `BindlessHandle`, `BindlessMetaInfo`, `BindlessUtils` |
| `command_context.cppm` | `:command_context` | `CommandType`, `CommandContext`, `GraphicsCommandContext`, `ComputeCommandContext`, `CopyCommandContext` |
| `command_queue.cppm` | `:command_queue` | `CommandQueue` |
| `shader_compiler.cppm` | `:shader_compiler` | `ShaderCompiler` (DXC-based; DXIL and SPIR-V output) |
| `utils.cppm` | `:utils` | `format_as()` for fmt, `get_format_bit_size`, `split_semantic` |
| `device.cppm` | primary | `Device` (abstract, extends `RuntimeModule`) — factory for all GPU objects |

**`dx12/`** — 8 module partitions composing `gfx.dx12`:

| File | Partition | Contains |
|---|---|---|
| `dx12_device.cppm` | primary | `DX12Device` — IDXGIFactory2, IDXGIAdapter4, ID3D12Device, D3D12MA::Allocator |
| `dx12_resource.cppm` | `:resource` | `DX12GPUBuffer`, `DX12Texture`, `DX12Sampler`, `DX12Shader`, `DX12RenderPipeline`, `DX12ComputePipeline`, `DX12SwapChain` |
| `dx12_command_list.cppm` | `:command_list` | `DX12GraphicsCommandList`, `DX12ComputeCommandList`, `DX12CopyCommandList` |
| `dx12_command_queue.cppm` | `:command_queue` | `DX12CommandQueue` — wraps `ID3D12CommandQueue` + internal `DX12Fence` for WaitIdle |
| `dx12_descriptor_heap.cppm` | `:descriptor_heap` | `Descriptor`, `DescriptorHeap`, `DescriptorAllocator` — CPU-side RTV/DSV only |
| `dx12_sync.cppm` | `:sync` | `DX12Fence` — wraps `ID3D12Fence` + Win32 event handle |
| `dx12_bindless.cppm` | `:bindless` | `DX12BindlessUtils` — CBV_SRV_UAV heap + Sampler heap + shared root signature |
| `dx12_utils.cppm` | `:utils` | Format/barrier/state conversion functions (`to_dxgi_format`, `to_d3d_barrier_access`, etc.) |

**`vulkan/`** — 9 module partitions composing `gfx.vulkan`:

| File | Partition | Contains |
|---|---|---|
| `vk_device.cppm` | primary | `VulkanDevice` — vk::raii::Instance/PhysicalDevice/Device, VmaAllocator |
| `vk_resource.cppm` | `:resource` | `VulkanBuffer`, `VulkanImage`, `VulkanSampler`, `VulkanSwapChain`, `VulkanShader`, `VulkanRenderPipeline`, `VulkanComputePipeline` |
| `vk_command_buffer.cppm` | `:command_buffer` | `VulkanGraphicsCommandBuffer`, `VulkanComputeCommandBuffer`, `VulkanTransferCommandBuffer` |
| `vk_command_queue.cppm` | `:command_queue` | `VulkanCommandQueue` — wraps vk::raii::Queue with queue family index |
| `vk_sync.cppm` | `:sync` | `VulkanTimelineSemaphore` — wraps `vk::raii::Semaphore` (timeline type) |
| `vk_bindless.cppm` | `:bindless` | `VulkanBindlessUtils` — descriptor pool + set layouts + pipeline layout |
| `vk_configs.cppm` | `:configs` | Required instance/device extensions, descriptor pool sizes |
| `vk_utils.cppm` | `:utils` | Format/barrier/state conversion functions, physical device scoring, queue helpers |
| `vma_patch.cpp` | — | VMA translation unit (separate static lib `vma_patch`) |

**`render_graph/`** — 5 module partitions composing `gfx.render_graph`:

| File | Partition | Contains |
|---|---|---|
| `type.cppm` | `:type` | `RenderGraphNode`, `RenderGraphHandle<T>` template, all 9 typed handle aliases |
| `resource_edge.cppm` | `:resource_edge` | `GPUBufferEdge`, `TextureEdge`, `SamplerEdge` — carry access/stage/layout metadata |
| `resource_node.cppm` | `:resource_node` | `ResourceNode` hierarchy: `GPUBufferNode`, `TextureNode`, `SamplerNode`, `RenderPipelineNode`, `ComputePipelineNode` |
| `pass_node.cppm` | `:pass_node` | `PassNode` hierarchy: `RenderPassNode`, `ComputePassNode`, `CopyPassNode`, `PresentPassNode` |
| `pass_builder.cppm` | `:pass_builder` | Fluent builder API: `RenderPassBuilder`, `ComputePassBuilder`, `CopyPassBuilder`, `PresentPassBuilder` |
| `render_graph.cppm` | primary | `RenderGraph` — owns all nodes, BlackBoard, per-queue fences, retired nodes |

## Class Hierarchy

### Device
```
core::RuntimeModule
└── gfx::Device                    [abstract] base/device.cppm
    ├── DX12Device     [final]     dx12/dx12_device.cppm
    ├── VulkanDevice   [final]     vulkan/vk_device.cppm
    └── MockDevice                 mock/mock_device.cppm
```

### GPU Resources
```
Resource → ResourceWithDesc<Desc>  [CRTP]    base/gpu_resource.cppm
├── GPUBuffer (virtual Map/UnMap; tracks BarrierAccess + PipelineStage)
│   ├── DX12GPUBuffer     (D3D12MA::Allocation + ID3D12Resource)
│   ├── VulkanBuffer      (vk::raii::Buffer + VmaAllocation)
│   └── MockGPUBuffer
├── Texture (tracks TextureLayout)
│   ├── DX12Texture       (ID3D12Resource + RTV/DSV Descriptor)
│   ├── VulkanImage       (vk::raii::Image + vk::raii::ImageView + VmaAllocation)
│   └── MockTexture
├── Sampler  [= ResourceWithDesc<SamplerDesc>]
├── SwapChain
│   ├── DX12SwapChain     (IDXGISwapChain4 + vector<DX12Texture> back buffers)
│   └── VulkanSwapChain   (vk::raii::SwapchainKHR + SemaphorePair per-frame)
├── Shader
│   ├── DX12Shader        (DXIL binary via core::Buffer)
│   └── VulkanShader      (SPIR-V binary + vk::raii::ShaderModule)
├── RenderPipeline  [= ResourceWithDesc<RenderPipelineDesc>]
└── ComputePipeline [= ResourceWithDesc<ComputePipelineDesc>]
```

### Command Contexts
```
CommandContext                          base/command_context.cppm
├── GraphicsCommandContext
│   ├── DX12GraphicsCommandList        (ID3D12GraphicsCommandList + ID3D12CommandAllocator)
│   └── VulkanGraphicsCommandBuffer    (vk::raii::CommandBuffer + swapchain semaphores)
├── ComputeCommandContext
│   ├── DX12ComputeCommandList
│   └── VulkanComputeCommandBuffer
└── CopyCommandContext
    ├── DX12CopyCommandList
    └── VulkanTransferCommandBuffer    (also holds swapchain semaphores)
```

### Synchronization
```
Fence                                  base/sync.cppm
├── DX12Fence [final]                  (ID3D12Fence + Win32 event handle)
├── VulkanTimelineSemaphore [final]    (vk::raii::Semaphore, VK_SEMAPHORE_TYPE_TIMELINE)
└── MockFence                          (std::atomic_uint64_t)
```

### Render Graph Nodes
```
RenderGraphNode                        render_graph/type.cppm
├── ResourceNode                       render_graph/resource_node.cppm
│   ├── GPUBufferNode (supports Move)
│   ├── TextureNode   (supports Move)
│   ├── SamplerNode
│   ├── RenderPipelineNode
│   └── ComputePipelineNode
└── PassNode                           render_graph/pass_node.cppm
    ├── RenderPassNode    (Executor, render target + depth stencil)
    ├── ComputePassNode   (Executor)
    ├── CopyPassNode      (Executor)
    └── PresentPassNode   (Executor, SwapChain, source texture)
```

## Backend Abstraction Pattern

All creatable objects are pure virtual in `gfx.base`. Backends implement them. The factory lives in `gfx.cpp`:

```cpp
auto gfx::create_device(Device::Type type, ...) -> std::unique_ptr<Device> {
    switch (type) {
        case Device::Type::DX12:   return std::make_unique<DX12Device>(...);
        case Device::Type::Vulkan: return std::make_unique<VulkanDevice>(...);
        case Device::Type::Mock:   return std::make_unique<MockDevice>(...);
    }
}
```

## API Abstraction Mapping Tables

### Device & Infrastructure

| `gfx::base` | DX12 | Vulkan | Docs |
|---|---|---|---|
| `Device` | `ID3D12Device` + `IDXGIFactory2` + `IDXGIAdapter4` | `vk::raii::Instance` + `vk::raii::PhysicalDevice` + `vk::raii::Device` | [D3D12 Device](https://microsoft.github.io/DirectX-Specs/d3d/D3D12EnhancedBarriers.html) / [VkDevice](https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#devsandqueues-devices) |
| Memory Allocator | `D3D12MA::Allocator` | `VmaAllocator` | [D3D12MA](https://gpuopen.com/d3d12-memory-allocator/) / [VMA](https://gpuopen.com/vulkan-memory-allocator/) |
| `CommandQueue` | `ID3D12CommandQueue` | `vk::raii::Queue` | [D3D12 Queues](https://microsoft.github.io/DirectX-Specs/d3d/CPUEfficiency.html) / [VkQueue](https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#devsandqueues-queues) |
| `ShaderCompiler` | DXC → DXIL | DXC → SPIR-V | [DXC](https://github.com/microsoft/DirectXShaderCompiler) |

### Resources

| `gfx::base` | DX12 | Vulkan | Docs |
|---|---|---|---|
| `GPUBuffer` | `D3D12MA::Allocation` + `ID3D12Resource` | `vk::raii::Buffer` + `VmaAllocation` | [D3D12 Buffers](https://microsoft.github.io/DirectX-Specs/d3d/ResourceBinding.html) / [VkBuffer](https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#resources-buffers) |
| `Texture` | `D3D12MA::Allocation` + `ID3D12Resource` + `Descriptor` (RTV/DSV) | `vk::raii::Image` + `vk::raii::ImageView` + `VmaAllocation` | [D3D12 Textures](https://microsoft.github.io/DirectX-Specs/d3d/ResourceBinding.html) / [VkImage](https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#resources-images) |
| `Sampler` | `SamplerDesc` only (descriptor stored in bindless heap) | `vk::raii::Sampler` | [VkSampler](https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#samplers) |
| `SwapChain` | `IDXGISwapChain4` + `vector<DX12Texture>` | `vk::raii::SwapchainKHR` + `SemaphorePair` (image_available + presentable) | [DXGI SwapChain](https://microsoft.github.io/DirectX-Specs/d3d/DXGI.html) / [VK_KHR_swapchain](https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#VK_KHR_swapchain) |
| `Shader` | `core::Buffer` (DXIL binary) → `D3D12_SHADER_BYTECODE` | `core::Buffer` (SPIR-V) + `vk::raii::ShaderModule` | — |
| `RenderPipeline` | `ID3D12PipelineState` | `vk::raii::Pipeline` | [D3D12 PSO](https://microsoft.github.io/DirectX-Specs/d3d/PipelineStateAPI.html) / [VkPipeline](https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#pipelines-graphics) |
| `ComputePipeline` | `ID3D12PipelineState` | `vk::raii::Pipeline` | — |

### Command Contexts

| `gfx::base` | DX12 | Vulkan | Notes |
|---|---|---|---|
| `GraphicsCommandContext` | `DX12GraphicsCommandList` (`ID3D12GraphicsCommandList` + `ID3D12CommandAllocator`) | `VulkanGraphicsCommandBuffer` (`vk::raii::CommandBuffer`) | Vulkan version has extra `BlitTexture()` for present |
| `ComputeCommandContext` | `DX12ComputeCommandList` | `VulkanComputeCommandBuffer` | — |
| `CopyCommandContext` | `DX12CopyCommandList` | `VulkanTransferCommandBuffer` | Vulkan transfer also holds swapchain semaphores |
| `BeginRendering` | `OMSetRenderTargets` (up to 8 RTVs + DSV) | `vkCmdBeginRendering` (VK_KHR_dynamic_rendering, multiple color attachments) | MRT supported — up to 8 color render targets + optional depth-stencil |
| `PushBindlessMetaInfo` | `SetGraphicsRoot32BitConstants` | `vkCmdPushConstants` | Push constant = single `BindlessMetaInfo` |
| `ResourceBarrier` | Enhanced Barriers (`ID3D12GraphicsCommandList7::Barrier`) | `vkCmdPipelineBarrier2` (Synchronization2) | Both use modern barrier APIs |

### Synchronization

| `gfx::base` | DX12 | Vulkan | Docs |
|---|---|---|---|
| `Fence` | `DX12Fence` (`ID3D12Fence`) | `VulkanTimelineSemaphore` (`VK_SEMAPHORE_TYPE_TIMELINE`) | [D3D12 Fence](https://microsoft.github.io/DirectX-Specs/d3d/CPUEfficiency.html) / [VK Timeline Semaphores](https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#synchronization-semaphores) |
| `Fence::Signal(value)` | `ID3D12Fence::Signal(value)` — CPU-side signal | `vkSignalSemaphore(value)` — CPU-side signal | — |
| `Fence::Wait(value)` | `SetEventOnCompletion` + `WaitForSingleObject` | `vkWaitSemaphores(timeout_ns)` | — |
| `Fence::GetCurrentValue` | `ID3D12Fence::GetCompletedValue()` | `VkSemaphore::getCounterValue()` | — |
| Queue GPU-signal | `ID3D12CommandQueue::Signal(fence, value)` | Via `vk::SemaphoreSubmitInfo` in `vkQueueSubmit2` | — |
| Queue GPU-wait | `ID3D12CommandQueue::Wait(fence, value)` | Via `vk::SemaphoreSubmitInfo` in `vkQueueSubmit2` | — |
| Queue Submit | `ExecuteCommandLists` + separate Wait/Signal calls | `vkQueueSubmit2` (single call with wait+signal semaphore arrays) | DX12 uses `vkQueueSubmit2` equivalent via separate calls |
| Swapchain Sync | Implicit (DXGI handles present sync) | Explicit binary semaphores: `image_available` (wait) + `presentable` (signal) | Vulkan appends swapchain semaphores to submit info |
| WaitIdle | Internal `DX12Fence` with monotonic `m_SubmitCount` | `vk::raii::Queue::waitIdle()` | DX12 signals its internal fence on every submit |

### Barrier Access Mapping

| `gfx::BarrierAccess` | DX12 (`D3D12_BARRIER_ACCESS`) | Vulkan (`vk::AccessFlagBits2`) |
|---|---|---|
| `None` | `NO_ACCESS` | `eNone` |
| `CopySrc` | `COPY_SOURCE` | `eTransferRead` |
| `CopyDst` | `COPY_DEST` | `eTransferWrite` |
| `Vertex` | `VERTEX_BUFFER` | `eVertexAttributeRead` |
| `Index` | `INDEX_BUFFER` | `eIndexRead` |
| `Constant` | `CONSTANT_BUFFER` | `eUniformRead` |
| `ShaderRead` | `SHADER_RESOURCE` | `eShaderRead` |
| `ShaderWrite` | `UNORDERED_ACCESS` | `eShaderWrite` |
| `DepthStencilRead` | `DEPTH_STENCIL_READ` | `eDepthStencilAttachmentRead` |
| `DepthStencilWrite` | `DEPTH_STENCIL_WRITE` | `eDepthStencilAttachmentWrite` |
| `RenderTarget` | `RENDER_TARGET` | `eColorAttachmentWrite` |
| `Present` | `COMMON` | `eMemoryRead` |

> DX12 docs: [Enhanced Barriers](https://microsoft.github.io/DirectX-Specs/d3d/D3D12EnhancedBarriers.html)
> Vulkan docs: [Synchronization2](https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#VK_KHR_synchronization2)

### Pipeline Stage Mapping

| `gfx::PipelineStage` | DX12 (`D3D12_BARRIER_SYNC`) | Vulkan (`vk::PipelineStageFlagBits2`) |
|---|---|---|
| `None` | `NONE` | `eNone` |
| `VertexInput` | `DRAW` | `eVertexInput` |
| `VertexShader` | `VERTEX_SHADING` | `eVertexShader` |
| `PixelShader` | `PIXEL_SHADING` | `eFragmentShader` |
| `DepthStencil` | `DEPTH_STENCIL` | `eLateFragmentTests` |
| `Render` | `RENDER_TARGET` | `eColorAttachmentOutput` |
| `Resolve` | `RESOLVE` | `eResolve` |
| `AllGraphics` | `DRAW` | `eAllGraphics` |
| `ComputeShader` | `COMPUTE_SHADING` | `eComputeShader` |
| `Copy` | `COPY` | `eCopy` |
| `All` | `ALL` | `eAllCommands` |

### Texture Layout Mapping

| `gfx::TextureLayout` | DX12 (`D3D12_BARRIER_LAYOUT`) | Vulkan (`vk::ImageLayout`) |
|---|---|---|
| `Unkown` | `UNDEFINED` | `eUndefined` |
| `Common` | `COMMON` | `eGeneral` |
| `CopySrc` | `COPY_SOURCE` | `eTransferSrcOptimal` |
| `CopyDst` | `COPY_DEST` | `eTransferDstOptimal` |
| `ShaderRead` | `SHADER_RESOURCE` | `eShaderReadOnlyOptimal` |
| `ShaderWrite` | `UNORDERED_ACCESS` | `eGeneral` |
| `DepthStencilRead` | `DEPTH_STENCIL_READ` | `eDepthStencilReadOnlyOptimal` |
| `DepthStencilWrite` | `DEPTH_STENCIL_WRITE` | `eDepthStencilAttachmentOptimal` |
| `RenderTarget` | `RENDER_TARGET` | `eColorAttachmentOptimal` |
| `ResolveSrc` | `RESOLVE_SOURCE` | `eColorAttachmentOptimal` |
| `ResolveDst` | `RESOLVE_DEST` | `eColorAttachmentOptimal` |
| `Present` | `PRESENT` | `ePresentSrcKHR` |

### Bindless Mapping

| `gfx::base` | DX12 | Vulkan | Docs |
|---|---|---|---|
| `BindlessUtils` | `DX12BindlessUtils` | `VulkanBindlessUtils` | — |
| Descriptor Storage | Two `ID3D12DescriptorHeap`s: CBV_SRV_UAV + Sampler (GPU-visible, shader-bound) | `vk::raii::DescriptorPool` with `UPDATE_AFTER_BIND` + multiple `DescriptorSet`s | [D3D12 Bindless](https://microsoft.github.io/DirectX-Specs/d3d/ResourceBinding.html) / [VK_EXT_descriptor_indexing](https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#VK_EXT_descriptor_indexing) |
| Root/Pipeline Layout | Shared `ID3D12RootSignature` with root constants + descriptor tables | Shared `vk::raii::PipelineLayout` with push constant range + set layouts | — |
| Meta Info Delivery | `SetGraphicsRoot32BitConstants` / `SetComputeRoot32BitConstants` | `vkCmdPushConstants` | Both push a `BindlessMetaInfo` struct (single `BindlessHandle`) |
| Handle Pool | `deque<BindlessHandle>` per type (CBV_SRV_UAV / Sampler) | `vector<BindlessHandle>` per type (4 pools: Buffer, Texture, Sampler, Invalid) | Handles are recycled on discard |
| Descriptor Limits | Configured via heap creation size | `max_storage_descriptors=10000`, `max_sampled_image=10000`, `max_storage_image=10000`, `max_sampler=128` (in `vk_configs.cppm`) | — |

## Synchronization Details

### Fence Model
The abstraction uses a **monotonically-increasing uint64 value** model, which maps naturally to:
- **DX12**: `ID3D12Fence` — natively supports monotonic uint64 signaling/waiting
- **Vulkan**: `VK_SEMAPHORE_TYPE_TIMELINE` — equivalent semantics

Both backends support CPU-side Signal/Wait and GPU-side Signal/Wait (via queue submit).

### Per-Queue Fence Tracking
- `DX12CommandQueue` owns an internal `DX12Fence` + `m_SubmitCount` (incremented on every submit). `WaitIdle()` waits on the last `m_SubmitCount`.
- `VulkanCommandQueue` uses `vk::raii::Queue::waitIdle()` directly — no internal fence needed.

### SwapChain Synchronization
- **DX12**: DXGI handles present synchronization implicitly. No explicit semaphores needed.
- **Vulkan**: Uses explicit binary semaphores via `VulkanSwapChain::SemaphorePair`:
  - `image_available`: signaled by `vkAcquireNextImageKHR`, waited on before rendering (at `eColorAttachmentOutput` stage)
  - `presentable`: signaled after rendering, waited on by `vkQueuePresentKHR`
  - These semaphores are automatically injected into submit info by `VulkanCommandQueue::Submit()` when it detects `VulkanGraphicsCommandBuffer` or `VulkanTransferCommandBuffer` carrying swapchain semaphores.

### Barrier Model
Both backends use **modern enhanced barrier APIs** (not legacy resource state tracking):
- **DX12**: Enhanced Barriers (`D3D12_GLOBAL_BARRIER`, `D3D12_BUFFER_BARRIER`, `D3D12_TEXTURE_BARRIER`) via `ID3D12GraphicsCommandList7::Barrier()`
- **Vulkan**: Synchronization2 (`vk::MemoryBarrier2`, `vk::BufferMemoryBarrier2`, `vk::ImageMemoryBarrier2`) via `vkCmdPipelineBarrier2`

Resources track their current state internally:
- `GPUBuffer`: tracks `m_CurrentAccess` + `m_CurrentStage` — `Transition()` returns a `GPUBufferBarrier` with src=current, dst=new, and updates internal state
- `Texture`: tracks `m_CurrentAccess` + `m_CurrentStage` + `m_CurrentLayout` — `Transition()` returns a `TextureBarrier` with layout transition

## Resource Management

### Ownership Model
All GPU resources are created via `Device::Create*()` and returned as `std::shared_ptr<T>`. This enables:
- Shared ownership between the render graph and user code
- Automatic cleanup when the last reference is dropped
- Safe import into render graphs (the graph holds a `shared_ptr`)

### Memory Allocation
- **DX12**: All buffers and textures go through `D3D12MA::Allocator` (committed or placed resources). The allocator is configured with custom allocation callbacks for tracking.
- **Vulkan**: All buffers and images go through `VmaAllocator`. Custom allocation callbacks for both Vulkan API objects and VMA.
- **Buffer alignment**: `GPUBuffer` stores `m_ElementAlignment`; `AlignedElementSize()` returns `align(element_size, m_ElementAlignment)`. Total size = `AlignedElementSize() * element_count`.

### Host-Visible Buffers (Map/UnMap)
- Buffers with `GPUBufferUsageFlags::MapRead` or `MapWrite` can be mapped.
- `GPUBufferView<T>` provides a typed RAII wrapper: maps on construction, unmaps on destruction, validates usages at runtime.
- Both backends use a `map_mutex` + `mapped_count` to support nested maps.

### Initial Data Upload
`Device::CreateGPUBuffer()` and `Device::CreateTexture()` accept an optional `std::span<const std::byte> initial_data`:
- **DX12**: Uses GPU Upload Heap for initial data transfer
- **Vulkan**: Uses `VK_EXT_host_image_copy` for textures; staging buffer for buffers

### Descriptor Management (DX12-specific)
- **RTV/DSV**: Managed by `DescriptorAllocator` → `DescriptorHeap` (CPU-side, paged). Each `DX12Texture` that is a render target or depth stencil owns a `Descriptor`.
- **CBV/SRV/UAV + Sampler**: Managed by `DX12BindlessUtils` in large GPU-visible heaps. Bound at command list begin time.

### SwapChain Back Buffer Management
- **DX12**: `DX12SwapChain` holds `vector<DX12Texture>` back buffers, created from `GetBuffer()`. `AcquireTextureForRendering()` returns the current back buffer by index.
- **Vulkan**: `VulkanSwapChain` holds `vector<unique_ptr<Texture>>` images. `AcquireTextureForRendering()` calls `vkAcquireNextImageKHR` and sets the `SemaphorePair`.

## Render Graph Lifecycle

### Per-Frame Usage Pattern
```
1. Import/Create resources     → get typed handles (GPUBufferHandle, TextureHandle, ...)
2. Declare passes              → use RenderPassBuilder/ComputePassBuilder/CopyPassBuilder/PresentPassBuilder
   - .Read(handle) / .Write(handle) / .AddRenderTarget(handle) / .SetExecutor(lambda)
   - .Finish() → registers the pass node + edges
3. Compile()                   → DFS from PresentPassNode to find essential nodes
                               → topological sort into ExecuteLayers keyed by CommandType
                               → Initialize() acquires GPU resources (pool first, then Device::Create*)
4. Execute()                   → for each layer, execute all passes
                               → Submit to queues with fence wait/signal
                               → Retire nodes guarded by fence values
5. RetireNodes()               → recycle transient resources to pool, evict stale entries
6. Reset()                     → clear all nodes, blackboard, execute layers for next frame
```

### Compilation Details
- `Compile()` starts a DFS from the `PresentPassNode` to determine which nodes are "essential"
- Output nodes of essential pass nodes are also kept to avoid execution failure
- Topological sort (Kahn's algorithm) groups nodes into `ExecuteLayer`s
- Each `ExecuteLayer` is an `EnumArray<vector<PassNode*>, CommandType>` — passes in the same layer and same command type are batched together
- Cycle detection: if visited count != essential count, reports error

### Execution Details
- Pass execution order: iterate all layers, then all passes in each layer
- Each pass: `Begin()` → `ResourceBarrier()` → user's `Executor` lambda → `End()`
- Queue submit: per-layer, per-command-type batch submit with:
  - Wait on ALL current fence values (cross-queue sync)
  - Signal this queue's fence with incremented value
- After all layers: wait on the **previous** frame's fence values (not current — allows overlap)
- `RetireNodes()` checks front of `m_RetiredNodes` deque against GPU-completed fence values; transient resources are recycled to the `TransientResourcePool` before being popped

### BlackBoard
`BlackBoard` is an `EnumArray<unordered_map<string, size_t>, NodeType>` that maps resource names to node indices. Used by `GetBufferHandle(name)`, `GetTextureHandle(name)`, etc.

### MoveFrom
`MoveFrom(handle)` creates a new node that aliases the same underlying resource as the original. Only `GPUBuffer` and `Texture` support Move. Used for temporal effects (e.g., reprojection from previous frame's buffer).

### Transient Resource Pool
`RenderGraph` maintains an internal `TransientResourcePool` that caches GPU resources across frames to avoid per-frame allocation/deallocation churn.

**Pool key**: Descriptor hash **excluding the `name` field** — resources with the same structure (size, format, usages, etc.) but different names are considered matching. Helper functions `buffer_pool_key()` / `texture_pool_key()` and `buffer_pool_match()` / `texture_pool_match()` are defined in `render_graph.cpp`.

**Lifecycle integration**:
- `GPUBufferNode::Initialize()` / `TextureNode::Initialize()` call `RenderGraph::AcquireTransientBuffer()` / `AcquireTransientTexture()` instead of `Device::Create*()` directly. These methods check the pool first; on miss, they fall through to `Device::Create*()`.
- `RenderGraph::RetireNodes()` calls `RecycleTransientResource()` for each retired node. For transient (non-imported, non-moved) GPUBuffer/Texture nodes, the resource is moved into the pool and the node's `m_Resource` is set to `nullptr` to prevent double-recycling.
- `EvictStalePoolEntries()` runs at the end of `RetireNodes()` and removes entries unused for more than `max_unused_frames` (default: 3 frames).

**Data structure**: `TransientResourcePool` contains `unordered_multimap<size_t, CachedBuffer>` and `unordered_multimap<size_t, CachedTexture>`, keyed by the pool-key hash. Each cached entry stores the descriptor, the `shared_ptr` to the GPU resource, and the last-used frame index.

**Exclusions**: Imported resources and MoveFrom/MoveTo resources are never pooled.

## Required Vulkan Extensions

Defined in `vulkan/vk_configs.cppm`:
- `VK_KHR_SWAPCHAIN` — swapchain support
- `VK_KHR_DYNAMIC_RENDERING` — renderpass-less rendering
- `VK_EXT_DESCRIPTOR_INDEXING` — bindless descriptor access
- `VK_EXT_HOST_IMAGE_COPY` — CPU-side image data upload

Debug-only: `VK_EXT_DEBUG_UTILS`, `VK_LAYER_KHRONOS_validation`

Platform surface: `VK_KHR_WIN32_SURFACE` (Windows) or `VK_KHR_WAYLAND_SURFACE` (Linux)

Physical device selection: scored by `compute_physical_device_score()` (discrete GPU preference); must have a queue family with >= 3 queues supporting Graphics+Compute+Transfer.

## Invariants & Constraints

- `Device` creates exactly 3 `CommandQueue`s (Graphics, Compute, Copy) at initialization — they live for the device's lifetime
- `RenderGraph` is **per-frame** — never reuse nodes across frames. Call `Reset()` (automatic after `Execute()`)
- Resources track barrier state internally (`m_CurrentAccess`, `m_CurrentStage`, `m_CurrentLayout`). `Transition()` returns the barrier AND updates the tracked state — calling it twice gives different results
- `Format` enum values mirror `DXGI_FORMAT` numerically (same integer values)
- All command lists must call `Begin()` before recording and `End()` before submission
- `BindlessHandle` is valid only between `CreateBindlessHandle()` and `DiscardBindlessHandle()` — the render graph manages this automatically per-pass
- `SwapChain::AcquireTextureForRendering()` must be called exactly once per frame before using the back buffer
- Shaders are compiled via DXC in both backends — HLSL is the source language, compiled to DXIL (DX12) or SPIR-V (Vulkan)
- All shared_ptr resources imported into a `RenderGraph` must remain alive until the GPU finishes consuming them (managed via retired nodes + fence guarding)

## Anti-patterns (DO NOT)

- **DO NOT** create GPU resources inside a PassNode executor callback — all resources must be Created/Imported before `Compile()`
- **DO NOT** hold raw pointers to `Resource` across frames — always use `shared_ptr`
- **DO NOT** call `Transition()` on a resource without subsequently using the returned barrier — the internal state has already been updated
- **DO NOT** mix command types in a single `CommandQueue::Submit()` — contexts must match the queue's type (enforced with runtime check)
- **DO NOT** skip `Compile()` before `Execute()` — the graph will warn and return early
- **DO NOT** assume Vulkan queue family indices — the engine selects a single family with 3+ queues supporting all types
- **DO NOT** manually manage swapchain semaphores in Vulkan — `VulkanCommandQueue::Submit()` automatically handles them based on command buffer type

## Common Modification Guide

### Adding a New GPU Resource Type
1. `base/types.cppm` — add to `ResourceType` enum
2. `base/gpu_resource.cppm` — add `NewResourceDesc` struct + class (extend `ResourceWithDesc<NewResourceDesc>`), update `GetType()` constexpr if-chain
3. `base/device.cppm` — add `virtual auto CreateNewResource(...) -> std::shared_ptr<NewResource>` to `Device`
4. `dx12/dx12_resource.cppm` — add `DX12NewResource` implementation
5. `vulkan/vk_resource.cppm` — add `VulkanNewResource` implementation
6. `mock/mock_resource.cppm` — add `MockNewResource` (no-op)
7. `dx12/dx12_device.cpp`, `vulkan/vk_device.cpp`, `mock/mock_device.cpp` — implement `CreateNewResource`
8. If render-graph-visible: add node type in `render_graph/resource_node.cppm`, handle alias in `render_graph/type.cppm`, builder methods in `render_graph/pass_builder.cppm`

### Adding a New Pass Type
1. `render_graph/pass_node.cppm` — add new `PassNode` subclass with `Executor` typedef
2. `render_graph/pass_builder.cppm` — add corresponding builder class
3. `render_graph/render_graph.cppm` — add builder creation method
4. `render_graph/render_graph.cpp` — handle in `Compile()` and `Execute()`

### Modifying the Barrier System
1. `base/types.cppm` — modify `BarrierAccess`, `PipelineStage`, or `TextureLayout` enums
2. `dx12/dx12_utils.cppm` — update `to_d3d_barrier_access()`, `to_d3d_pipeline_stage()`, `to_d3d_texture_layout()`
3. `vulkan/vk_utils.cppm` — update `to_vk_access_flags()`, `to_vk_pipeline_stage2()`, `to_vk_image_layout()`
4. Update barrier construction in `dx12/dx12_command_list.cpp` and `vulkan/vk_command_buffer.cpp`

### Adding a New Vulkan Extension Requirement
1. `vulkan/vk_configs.cppm` — add to `required_device_extensions` array
2. `vulkan/vk_device.cpp` — enable corresponding features in device creation
3. `vulkan/vk_utils.cppm` — update `is_physical_suitable()` if the extension affects device selection

### Adding a New Backend
1. Create `hitagi/gfx/new_backend/` directory with module `gfx.new_backend`
2. Implement all abstract classes: Device, all Resources, CommandContexts, CommandQueue, Fence, BindlessUtils
3. Add factory case in `gfx.cpp` `create_device()`
4. Add `Device::Type` enum value in `base/device.cppm`
5. Add xmake.lua build target, include from `hitagi/gfx/xmake.lua`

## External Documentation References

- [DirectX 12 Specs](https://microsoft.github.io/DirectX-Specs/) — Enhanced Barriers, Resource Binding, Pipeline State
- [D3D12 Enhanced Barriers](https://microsoft.github.io/DirectX-Specs/d3d/D3D12EnhancedBarriers.html) — the barrier model used by this engine
- [D3D12 Memory Allocator](https://gpuopen.com/d3d12-memory-allocator/) — D3D12MA library
- [Vulkan Specification](https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html) — official Vulkan spec
- [VK_KHR_synchronization2](https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#VK_KHR_synchronization2) — Vulkan barrier model used
- [VK_KHR_dynamic_rendering](https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#VK_KHR_dynamic_rendering) — renderpass-less rendering
- [VK_EXT_descriptor_indexing](https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#VK_EXT_descriptor_indexing) — bindless descriptors
- [VK_EXT_host_image_copy](https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#VK_EXT_host_image_copy) — CPU-side texture upload
- [Vulkan Memory Allocator](https://gpuopen.com/vulkan-memory-allocator/) — VMA library
- [DirectX Shader Compiler](https://github.com/microsoft/DirectXShaderCompiler) — DXC used for both DXIL and SPIR-V

## Maintenance

**When to update this document** — update when any of the following changes occur:
- New/removed resource types, pass types, or command types
- Changes to the `Device` virtual interface
- Changes to the barrier/synchronization model
- New Vulkan extensions or D3D12 features adopted
- Changes to render graph lifecycle (Compile/Execute flow)
- New backend added or existing backend removed
- Significant changes to bindless descriptor strategy
- Changes to resource ownership model or memory allocation strategy

**How to update** — keep the same structure. Prefer tables over prose. Include exact file paths. Verify API mapping accuracy against the actual conversion functions in `dx12_utils.cppm` and `vk_utils.cppm`.
