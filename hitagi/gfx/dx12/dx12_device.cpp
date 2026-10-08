module;

#include <spdlog/sinks/stdout_color_sinks.h>
#include <tracy/Tracy.hpp>
#include <d3dx12/d3dx12.h>
#include <dxgi1_6.h>
#include <D3D12MemAlloc.h>

module gfx.dx12;
import magic_enum;

namespace hitagi::gfx {

DX12Device::DX12Device(std::string_view name) : Device(Type::DX12, name) {
    ComPtr<ID3D12SDKConfiguration1> sdk_configuration;
    if (const auto result = D3D12GetInterface(CLSID_D3D12SDKConfiguration, IID_PPV_ARGS(&sdk_configuration)); FAILED(result)) {
        const auto error_message = std::format("Failed to get D3D12 SDK configuration (HRESULT 0x{:08X}).", static_cast<unsigned long>(result));
        m_Logger->error(error_message);
        throw std::runtime_error(error_message);
    }

    const auto sdk_directory = [this]() -> std::string {
        std::string executable(MAX_PATH, '\0');
        for (;;) {
            const auto length = GetModuleFileNameA(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
            if (length == 0) {
                m_Logger->error("Failed to locate the executable for the Agility SDK.");
                throw std::runtime_error("Failed to locate the executable for the Agility SDK.");
            }
            if (length < executable.size()) {
                executable.resize(length);
                break;
            }
            executable.resize(executable.size() * 2);
        }
        return (std::filesystem::path(executable).parent_path() / "D3D12").string() + "\\";
    }();
    if (const auto result = sdk_configuration->CreateDeviceFactory(D3D12SDK_VERSION, sdk_directory.c_str(), IID_PPV_ARGS(&m_DeviceFactory)); FAILED(result)) {
        const auto error_message = std::format("Failed to create D3D12 device factory (HRESULT 0x{:08X}).", static_cast<unsigned long>(result));
        m_Logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    // A singleton fallback would copy the factory's debug configuration into global state.
    if (const auto result = m_DeviceFactory->SetFlags(D3D12_DEVICE_FACTORY_FLAG_DISALLOW_STORING_NEW_DEVICE_AS_SINGLETON); FAILED(result)) {
        const auto error_message = std::format("Failed to require independent D3D12 devices (HRESULT 0x{:08X}).", static_cast<unsigned long>(result));
        m_Logger->error(error_message);
        throw std::runtime_error(error_message);
    }

    unsigned dxgi_factory_flags = 0;

#ifdef HITAGI_DEBUG
    {
        ComPtr<ID3D12Debug> debug_controller;
        if (SUCCEEDED(m_DeviceFactory->GetConfigurationInterface(CLSID_D3D12Debug, IID_PPV_ARGS(&debug_controller)))) {
            dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
            m_Logger->trace("Enabled factory-local D3D12 debug layer.");
            debug_controller->EnableDebugLayer();

            // if (ComPtr<ID3D12Debug3> debug_controller_3;
            //     SUCCEEDED(debug_controller.As(&debug_controller_3))) {
            //     m_Logger->trace("Enabled GPU Based validation");
            //     debug_controller_3->SetEnableGPUBasedValidation(true);
            // }
        }
    }
#endif

    if (FAILED(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&m_Factory)))) {
        m_Logger->error("Failed to create DXGI factory");
        throw std::runtime_error("Failed to create DXGI factory.");
    }

    m_Logger->trace("Pick GPU...");
    {
        ComPtr<IDXGIFactory6> factory_6;
        if (FAILED(m_Factory.As(&factory_6))) {
            m_Logger->error("Failed to get IDXGIFactory6");
            throw std::runtime_error("Failed to get IDXGIFactory6.");
        }

        ComPtr<IDXGIAdapter1> p_adapter = nullptr;
        for (UINT adapter_index = 0; SUCCEEDED(factory_6->EnumAdapterByGpuPreference(adapter_index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&p_adapter))); adapter_index++) {
            DXGI_ADAPTER_DESC1 desc;
            p_adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                continue;
            }

            if (SUCCEEDED(m_DeviceFactory->CreateDevice(p_adapter.Get(), D3D_FEATURE_LEVEL_12_1, __uuidof(ID3D12Device), nullptr))) {
                std::pmr::wstring description = desc.Description;
                m_Logger->info("Pick: {}", std::pmr::string(description.begin(), description.end()));
                p_adapter.As(&m_Adapter);
                break;
            }
        }

        if (m_Adapter == nullptr) {
            m_Logger->warn("Fail to pick high performance gpu.");

            std::optional<UINT> warp_adapter_index;
            for (UINT adapter_index = 0; SUCCEEDED(m_Factory->EnumAdapters1(adapter_index, &p_adapter)); adapter_index++) {
                DXGI_ADAPTER_DESC1 desc;
                p_adapter->GetDesc1(&desc);

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                    warp_adapter_index = adapter_index;
                    continue;
                }

                if (SUCCEEDED(m_DeviceFactory->CreateDevice(p_adapter.Get(), D3D_FEATURE_LEVEL_12_1, __uuidof(ID3D12Device), nullptr))) {
                    std::pmr::wstring description = desc.Description;
                    m_Logger->info("Pick: {}", std::pmr::string(description.begin(), description.end()));
                    p_adapter.As(&m_Adapter);
                    break;
                }
            }

            if (m_Adapter == nullptr && warp_adapter_index.has_value()) {
                m_Logger->error("Use wrap device.");

                if (FAILED(m_Factory->EnumAdapters1(warp_adapter_index.value(), &p_adapter))) {
                    m_Logger->error("Failed to get warp adapter.");
                    throw std::runtime_error("Failed to get warp adapter.");
                }
                p_adapter.As(&m_Adapter);
            }

            if (m_Adapter == nullptr) {
                m_Logger->error("Failed to get adapter.");
                throw std::runtime_error("Failed to get adapter.");
            }
        }
    }

    m_Logger->trace("Create D3D12 device...");
    {
        if (const auto result = m_DeviceFactory->CreateDevice(m_Adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&m_Device)); FAILED(result)) {
            const auto error_message = std::format("Failed to create independent D3D12 device (HRESULT 0x{:08X}).", static_cast<unsigned long>(result));
            m_Logger->error(error_message);
            throw std::runtime_error(error_message);
        }

        CD3DX12FeatureSupport feature_support;
        if (FAILED(feature_support.Init(m_Device.Get()))) {
            m_Logger->error("Failed to init feature support.");
            throw std::runtime_error("Failed to init feature support.");
        }

        if (feature_support.HighestShaderModel() < D3D_SHADER_MODEL_6_7 ||
            feature_support.ResourceBindingTier() < D3D12_RESOURCE_BINDING_TIER_3) {
            throw std::runtime_error("Bindless requires Shader Model 6.7 and Resource Binding Tier 3");
        }

        if (!feature_support.EnhancedBarriersSupported()) {
            m_Logger->error("EnhancedBarriers Not Supported");
            throw std::runtime_error("EnhancedBarriers Not Supported");
        }

        if (!feature_support.GPUUploadHeapSupported()) {
            m_Logger->error("GPU Upload Heap Not Supported");
            throw std::runtime_error("GPU Upload Heap Not Supported");
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
        if (FAILED(D3D12MA::CreateAllocator(&desc, &m_MemoryAllocator))) {
            m_Logger->error("Failed to create D3D12 Memory Allocator");
            throw std::runtime_error("Failed to create D3D12 Memory Allocator");
        }
    }

    m_Logger->trace("Create Command Queues");
    magic_enum::enum_for_each<CommandType>([this](CommandType type) {
        m_CommandQueues[type] = std::make_shared<DX12CommandQueue>(
            *this,
            type,
            std::format("Builtin-{}-CommandQueue", magic_enum::enum_name(type)));
    });

    m_Logger->trace("Create RTV DSV Descriptor Allocators...");
    {
        m_RTVDescriptorAllocator = std::make_unique<DescriptorAllocator>(*this, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        m_DSVDescriptorAllocator = std::make_unique<DescriptorAllocator>(*this, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    }
    m_BindlessUtils = std::make_unique<DX12BindlessUtils>(*this, "DX12-BindlessUtils");

    m_Logger->trace("Initialized.");
}

DX12Device::~DX12Device() {
    WaitIdle();
    UnregisterIntegratedD3D12Logger();
#ifdef HITAGI_DEBUG
    report_debug_error_after_destroy_fn = [device = m_Device]() { DX12Device::ReportDebugLog(device); };
#endif
}

void DX12Device::Tick() {
    Device::Tick();
#ifdef TRACY_ENABLE
    for (const auto& queue : m_CommandQueues) {
        queue->NewFrame();
    }
#endif
    if (m_EnableProfile) {
        Profile();
    }
}

void DX12Device::WaitIdle() {
    ZoneScopedNS("DX12Device::WaitIdle", 8);
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
    return std::make_shared<DX12SwapChain>(*this, std::move(desc));
}

auto DX12Device::CreateGPUBuffer(GPUBufferDesc desc, std::span<const std::byte> initial_data) -> std::shared_ptr<GPUBuffer> {
    return std::make_shared<DX12GPUBuffer>(*this, std::move(desc), initial_data);
}

auto DX12Device::CreateGPUBufferView(GPUBufferViewDesc desc) -> std::shared_ptr<GPUBufferView> {
    return std::make_shared<DX12GPUBufferView>(*this, std::move(desc));
}

auto DX12Device::CreateTexture(TextureDesc desc, std::span<const std::byte> initial_data) -> std::shared_ptr<Texture> {
    return std::make_shared<DX12Texture>(*this, std::move(desc), initial_data);
}

auto DX12Device::CreateTextureView(TextureViewDesc desc) -> std::shared_ptr<TextureView> {
    return std::make_shared<DX12TextureView>(*this, std::move(desc));
}

auto DX12Device::CreateSampler(SamplerDesc desc) -> std::shared_ptr<Sampler> {
    return std::make_shared<DX12Sampler>(*this, std::move(desc));
}

auto DX12Device::CreateShader(ShaderDesc desc) -> std::shared_ptr<Shader> {
    return std::make_shared<DX12Shader>(*this, std::move(desc));
}

auto DX12Device::CreateRenderPipeline(RenderPipelineDesc desc, const std::pmr::vector<std::shared_ptr<Shader>>& shaders) -> std::shared_ptr<RenderPipeline> {
    return std::make_shared<DX12RenderPipeline>(*this, std::move(desc), shaders);
}

auto DX12Device::CreateComputePipeline(ComputePipelineDesc desc, const std::shared_ptr<Shader>& cs) -> std::shared_ptr<ComputePipeline> {
    return std::make_shared<DX12ComputePipeline>(*this, std::move(desc), cs);
}

auto DX12Device::GetBindlessUtils() -> BindlessUtils& {
    return *m_BindlessUtils;
}

void DX12Device::Profile() const {
    static bool configured = false;
    if (!configured) {
        TracyPlotConfig("GPU Allocations", tracy::PlotFormatType::Number, true, true, 0);
        TracyPlotConfig("GPU Memory", tracy::PlotFormatType::Memory, false, true, 0);
        configured = true;
    }
    m_MemoryAllocator->SetCurrentFrameIndex(m_FrameIndex);
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
                        throw std::runtime_error(description);
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
