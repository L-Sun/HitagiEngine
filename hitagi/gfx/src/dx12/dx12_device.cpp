#include "dx12_device.hpp"
#include "dx12_command_queue.hpp"
#include "dx12_command_list.hpp"
#include "dx12_resource.hpp"

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

DX12Device::DX12Device(std::string_view name)
    : Device(Type::DX12, name),
      m_ShaderCompiler(fmt::format("{}-ShaderCompiler", name))

{
    unsigned dxgi_factory_flags = 0;

#ifdef HITAGI_DEBUG
    {
        ComPtr<ID3D12Debug> debug_controller;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)))) {
            if (ComPtr<ID3D12Debug3> debug_controller_3;
                SUCCEEDED(debug_controller->QueryInterface(IID_PPV_ARGS(&debug_controller_3)))) {
                m_Logger->trace("Enabled GPU Based validation");
                debug_controller_3->SetEnableGPUBasedValidation(true);
                m_Logger->trace("Enabled Synchronized command queue validation");
                debug_controller_3->SetEnableSynchronizedCommandQueueValidation(true);
                debug_controller_3->EnableDebugLayer();
            } else {
                m_Logger->trace("Enabled D3D12 debug layer.");
                debug_controller->EnableDebugLayer();
            }
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
        ComPtr<IDXGIAdapter> p_adapter;
        if (m_Factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&p_adapter)) != DXGI_ERROR_NOT_FOUND) {
            DXGI_ADAPTER_DESC desc;
            p_adapter->GetDesc(&desc);
            std::pmr::wstring description = desc.Description;
            m_Logger->info("Pick: {}", std::pmr::string(description.begin(), description.end()));
            p_adapter.As(&m_Adapter);
        }
    }

    m_Logger->trace("Create D3D12 device...");
    {
        if (FAILED(D3D12CreateDevice(m_Adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&m_Device)))) {
            m_Logger->error("Failed to create D3D12 device.");
            throw std::runtime_error("Failed to create D3D12 device.");
        }

        CD3DX12FeatureSupport feature_support;
        if (FAILED(feature_support.Init(m_Device.Get()))) {
            m_Logger->error("Failed to init feature support.");
            throw std::runtime_error("Failed to init feature support.");
        }

        if (!feature_support.EnhancedBarriersSupported()) {
            m_Logger->error("EnhancedBarriers Not Supported");
            throw std::runtime_error("EnhancedBarriers Not Supported");
        }

        if (!name.empty()) {
            m_Device->SetName(std::wstring(name.begin(), name.end()).c_str());
        }
    }

    m_Logger->trace("Initial logger for D3D12");
    IntegrateD3D12Logger();

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
        m_CommandQueues[type] = std::make_shared<DX12CommandQueue>(
            *this,
            type,
            fmt::format("Builtin-{}-CommandQueue", magic_enum::enum_name(type)));
    });

    m_Logger->trace("Create RTV DSV Descriptor Allocators...");
    {
        m_RTVDescriptorAllocator = std::make_unique<DescriptorAllocator>(*this, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        m_DSVDescriptorAllocator = std::make_unique<DescriptorAllocator>(*this, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    }
    m_BindlessUtils = std::make_unique<DX12BindlessUtils>(*this, "DX12-BindlessUtils");
}

DX12Device::~DX12Device() {
    WaitIdle();
    UnregisterIntegratedD3D12Logger();
#ifdef HITAGI_DEBUG
    report_debug_error_after_destroy_fn = [device = m_Device]() { DX12Device::ReportDebugLog(device); };
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

auto DX12Device::CreateCommandContext(CommandType type, std::string_view name) -> std::shared_ptr<CommandContext> {
    switch (type) {
        case CommandType::Graphics:
            return std::make_shared<DX12GraphicsCommandList>(*this, name);
        case CommandType::Compute:
            return std::make_shared<DX12ComputeCommandList>(*this, name);
        case CommandType::Copy:
            return std::make_shared<DX12CopyCommandList>(*this, name);
        default:
            throw std::runtime_error("Invalid command type.");
    }
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

void DX12Device::ReportDebugLog(const ComPtr<ID3D12Device>& device) {
#ifdef HITAGI_DEBUG
    ComPtr<ID3D12DebugDevice1> debug_interface;
    if (FAILED(device->QueryInterface(debug_interface.ReleaseAndGetAddressOf()))) {
        return;
    }
    debug_interface->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
#endif
}

void DX12Device::IntegrateD3D12Logger() {
    ComPtr<ID3D12InfoQueue1> info_queue;
    if (SUCCEEDED(m_Device->QueryInterface(IID_PPV_ARGS(&info_queue)))) {
        m_Logger->trace("Enabled D3D12 debug logger");
        info_queue->RegisterMessageCallback(
            [](D3D12_MESSAGE_CATEGORY, D3D12_MESSAGE_SEVERITY severity, D3D12_MESSAGE_ID, LPCSTR description, void* context) {
                auto p_this = reinterpret_cast<DX12Device*>(context);
                switch (severity) {
                    case D3D12_MESSAGE_SEVERITY_CORRUPTION:
                        p_this->m_Logger->critical(description);
                        break;
                    case D3D12_MESSAGE_SEVERITY_ERROR:
                        p_this->m_Logger->error(description);
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
    m_Logger->trace("Unregister D3D12 Logger");
    ComPtr<ID3D12InfoQueue1> info_queue;
    if (SUCCEEDED(m_Device->QueryInterface(IID_PPV_ARGS(&info_queue)))) {
        m_Logger->trace("Unable D3D12 debug logger");
        info_queue->UnregisterMessageCallback(m_DebugCookie);
    }
}

}  // namespace hitagi::gfx