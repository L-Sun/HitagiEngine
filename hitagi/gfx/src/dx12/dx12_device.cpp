#include "dx12_device.hpp"
#include "dx12_command_queue.hpp"
#include "dx12_command_list.hpp"
#include "dx12_resource.hpp"
#include "utils.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <fmt/color.h>
#include <tracy/Tracy.hpp>

#include <algorithm>

extern "C" {
_declspec(dllexport) extern const UINT D3D12SDKVersion = D3D12SDK_VERSION;
}
extern "C" {
_declspec(dllexport) extern const char* D3D12SDKPath = "./D3D12/";
}

namespace hitagi::gfx {

ComPtr<ID3D12DebugDevice1> g_debug_interface;

DX12Device::DX12Device(std::string_view name)
    : Device(Type::DX12, name),
      m_ShaderCompiler(fmt::format("{}-ShaderCompiler", name))

{
    unsigned dxgi_factory_flags = 0;

#ifdef HITAGI_DEBUG
    {
        ComPtr<ID3D12Debug> debug_controller;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)))) {
            m_Logger->debug("Enabled D3D12 debug layer.");

            debug_controller->EnableDebugLayer();
            // Enable additional debug layers.
            dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    if (FAILED(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&m_Factory)))) {
        m_Logger->error("Failed to create DXGI factory");
        throw std::runtime_error("Failed to create DXGI factory.");
    }

    m_Logger->trace("Pick GPU...");
    {
        std::pmr::vector<ComPtr<IDXGIAdapter>> adapters;
        ComPtr<IDXGIAdapter>                   p_adapter;
        for (UINT index = 0; m_Factory->EnumAdapterByGpuPreference(index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&p_adapter)) != DXGI_ERROR_NOT_FOUND; index++) {
            DXGI_ADAPTER_DESC desc;
            p_adapter->GetDesc(&desc);
            std::pmr::wstring description = desc.Description;
            m_Logger->trace("\t {}", std::pmr::string(description.begin(), description.end()));
            adapters.emplace_back(std::move(p_adapter));
        }
        // m_Logger->trace("Filter by feature support");
        // std::erase_if(adapters, [this](const ComPtr<IDXGIAdapter>& adapter) -> bool {
        //     ComPtr<ID3D12Device> device;
        //     if (FAILED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&device)))) {
        //         return true;
        //     }

        //     CD3DX12FeatureSupport feature_support;
        //     if (FAILED(feature_support.Init(device.Get()))) {
        //         return true;
        //     }

        //     if (!feature_support.EnhancedBarriersSupported()) {
        //         m_Logger->error("EnhancedBarriers Not Supported");
        //         return true;
        //     }

        //     return false;
        // });
        // if (adapters.empty()) {
        //     m_Logger->error("No suitable GPU found.");
        //     throw std::runtime_error("No suitable GPU found.");
        // }
        // {
        //     DXGI_ADAPTER_DESC desc;
        //     adapters.front()->GetDesc(&desc);
        //     std::pmr::wstring description = desc.Description;
        //     m_Logger->trace("Pick: {}", std::pmr::string(description.begin(), description.end()));
        // }
        adapters.front().As(&m_Adapter);
    }

    m_Logger->trace("Create D3D12 device...");
    {
        if (FAILED(D3D12CreateDevice(m_Adapter.Get(), D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&m_Device)))) {
            m_Logger->error("Failed to create D3D12 device.");
            throw std::runtime_error("Failed to create D3D12 device.");
        }

        if (!name.empty()) {
            m_Device->SetName(std::wstring(name.begin(), name.end()).c_str());
        }
    }

    m_Logger->trace("Initial logger for D3D12");
    // IntegrateD3D12Logger();

    m_Logger->trace("Create D3D12 Memory Allocator");
    {
        m_CustomAllocationCallback.pAllocate =
            [](std::size_t size, std::size_t alignment, void* p_this) {
                auto allocator = std::pmr::get_default_resource();
                auto ptr       = allocator->allocate(size, alignment);
                reinterpret_cast<DX12Device*>(p_this)->m_CustomAllocationInfos.emplace(ptr, std::make_pair(size, alignment));
                return ptr;
            };
        m_CustomAllocationCallback.pFree =
            [](void* ptr, void* p_this) {
                if (ptr == nullptr) return;
                auto [size, alignment] = reinterpret_cast<DX12Device*>(p_this)->m_CustomAllocationInfos.at(ptr);
                reinterpret_cast<DX12Device*>(p_this)->m_CustomAllocationInfos.erase(ptr);
                auto allocator = std::pmr::get_default_resource();
                allocator->deallocate(ptr, size, alignment);
            };
        m_CustomAllocationCallback.pPrivateData = this;

        D3D12MA::ALLOCATOR_DESC desc{
            .Flags                = D3D12MA::ALLOCATOR_FLAG_NONE,
            .pDevice              = m_Device.Get(),
            .pAllocationCallbacks = &m_CustomAllocationCallback,
            .pAdapter             = m_Adapter.Get(),
        };
        D3D12MA::CreateAllocator(&desc, &m_MemoryAllocator);
    }

    m_Logger->trace("Create Command Queues");
    magic_enum::enum_for_each<CommandType>([this](CommandType type) {
        m_CommandQueues[type] = std::static_pointer_cast<DX12CommandQueue>(
            std::make_shared<DX12CommandQueue>(
                *this,
                type,
                fmt::format("Builtin-{}-CommandQueue", magic_enum::enum_name(type))));
    });

    m_Logger->trace("Create Descriptor Allocators...");
    for (std::size_t index = 0; index < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; index++) {
        m_DescriptorAllocators[index] = std::make_unique<DescriptorAllocator>(*this, static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(index), false);
    }
    m_BindlessUtils = std::make_unique<DX12BindlessUtils>(*this, "DX12-BindlessUtils");
}

DX12Device::~DX12Device() {
    // UnregisterIntegratedD3D12Logger();
#ifdef HITAGI_DEBUG
    if (FAILED(m_Device->QueryInterface(g_debug_interface.ReleaseAndGetAddressOf()))) {
        m_Logger->error("Failed to get debug interface.");
    }
    report_debug_error_after_destroy_fn = []() { DX12Device::ReportDebugLog(); };
#endif
}

void DX12Device::WaitIdle() {
    for (auto& queue : m_CommandQueues) {
        queue->WaitIdle();
    }
}

auto DX12Device::CreateFence(std::uint64_t initial_value, std::string_view name) -> std::shared_ptr<Fence> {
    return std::make_shared<DX12Fence>(*this, initial_value, name);
}

auto DX12Device::GetCommandQueue(CommandType type) const -> CommandQueue& {
    return *m_CommandQueues[type];
}

auto DX12Device::CreateGraphicsContext(std::string_view name) -> std::shared_ptr<GraphicsCommandContext> {
    return std::make_shared<DX12GraphicsCommandList>(*this, name);
}

auto DX12Device::CreateComputeContext(std::string_view name) -> std::shared_ptr<ComputeCommandContext> {
    return std::make_shared<DX12ComputeCommandList>(*this, name);
}

auto DX12Device::CreateCopyContext(std::string_view name) -> std::shared_ptr<CopyCommandContext> {
    return std::make_shared<DX12CopyCommandList>(*this, name);
}

auto DX12Device::CreateSwapChain(SwapChainDesc desc) -> std::shared_ptr<SwapChain> {
    return std::make_shared<DX12SwapChain>(*this, desc);
}

auto DX12Device::CreateGPUBuffer(GPUBufferDesc desc, std::span<const std::byte> initial_data) -> std::shared_ptr<GPUBuffer> {
    return std::make_shared<DX12GPUBuffer>(*this, desc, initial_data);
}

auto DX12Device::CreateTexture(TextureDesc desc, std::span<const std::byte> initial_data) -> std::shared_ptr<Texture> {
    return std::make_shared<DX12Texture>(*this, desc, initial_data);
}

auto DX12Device::CreatSampler(SamplerDesc desc) -> std::shared_ptr<Sampler> {
    return std::make_shared<DX12Sampler>(*this, desc);
}

auto DX12Device::CreateShader(ShaderDesc desc, std::span<const std::byte> binary_program) -> std::shared_ptr<Shader> {
    return std::make_shared<DX12Shader>(*this, std::move(desc), binary_program);
}

auto DX12Device::CreateRenderPipeline(RenderPipelineDesc desc) -> std::shared_ptr<RenderPipeline> {
    return std::make_shared<DX12RenderPipeline>(*this, std::move(desc));
}

auto DX12Device::CreateComputePipeline(ComputePipelineDesc desc) -> std::shared_ptr<ComputePipeline> {
    return std::make_shared<DX12ComputePipeline>(*this, std::move(desc));
}

auto DX12Device::GetBindlessUtils() -> BindlessUtils& {
    return *m_BindlessUtils;
}

void DX12Device::Profile(std::size_t frame_index) const {
    static bool configured = false;
    if (!configured) {
        TracyPlotConfig("GPU Allocations", tracy::PlotFormatType::Number, true, true, 0);
        TracyPlotConfig("GPU Memory", tracy::PlotFormatType::Memory, false, true, 0);
        configured = true;
    }
    m_MemoryAllocator->SetCurrentFrameIndex(frame_index);
    D3D12MA::Budget local_budget;
    m_MemoryAllocator->GetBudget(&local_budget, nullptr);
    TracyPlot("GPU Allocations", static_cast<std::int64_t>(local_budget.Stats.AllocationCount));
    TracyPlot("GPU Memory", static_cast<std::int64_t>(local_budget.Stats.AllocationBytes));
}

void DX12Device::ReportDebugLog() {
#ifdef HITAGI_DEBUG
    g_debug_interface->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
#endif
}

void DX12Device::IntegrateD3D12Logger() {
    ComPtr<ID3D12InfoQueue1> info_queue;
    if (SUCCEEDED(m_Device->QueryInterface(IID_PPV_ARGS(&info_queue)))) {
        m_Logger->debug("Enabled D3D12 debug logger");
        info_queue->RegisterMessageCallback(
            [](D3D12_MESSAGE_CATEGORY, D3D12_MESSAGE_SEVERITY severity, D3D12_MESSAGE_ID, LPCSTR description, void* context) {
                auto p_this = reinterpret_cast<DX12Device*>(context);
                switch (severity) {
                    case D3D12_MESSAGE_SEVERITY_CORRUPTION:
                        p_this->m_Logger->critical(description);
                        break;
                    case D3D12_MESSAGE_SEVERITY_ERROR:
                        p_this->m_Logger->error(description);
                        throw std::runtime_error(description);
                        break;
                    case D3D12_MESSAGE_SEVERITY_WARNING:
                        p_this->m_Logger->warn(description);
                        break;
                    case D3D12_MESSAGE_SEVERITY_INFO:
                    case D3D12_MESSAGE_SEVERITY_MESSAGE:
                        p_this->m_Logger->info(description);
                        break;
                }
            },
            D3D12_MESSAGE_CALLBACK_FLAG_NONE,
            this,
            &m_DebugCookie);
    }
}

void DX12Device::UnregisterIntegratedD3D12Logger() {
    m_Logger->debug("Unregister D3D12 Logger");
    ComPtr<ID3D12InfoQueue1> info_queue;
    if (SUCCEEDED(m_Device->QueryInterface(IID_PPV_ARGS(&info_queue)))) {
        m_Logger->debug("Unable D3D12 debug logger");
        info_queue->UnregisterMessageCallback(m_DebugCookie);
    }
}

}  // namespace hitagi::gfx