module;
#include <d3d12.h>
#include <wrl.h>
#include <D3D12MemAlloc.h>
#include <dxgi1_6.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <tracy/Tracy.hpp>
#include <d3dx12/d3dx12.h>

export module gfx.dx12:device;
import std;
import core;
import utils;
import math;
import gfx.base;
import magic_enum;
import :types;
import :utils;
import :descriptor_heap;

using namespace Microsoft::WRL;

export namespace hitagi::gfx {

class DX12Device final : public Device {
public:
    DX12Device(std::string_view name);
    ~DX12Device() final;

    void Tick() final;

    inline auto  GetFactory() const noexcept { return m_Factory; }
    inline auto  GetAdapter() const noexcept { return m_Adapter; }
    inline auto  GetDevice() const noexcept { return m_Device; }
    inline auto  GetAllocator() const noexcept { return m_MemoryAllocator; }
    inline auto& GetRTVDescriptorAllocator() const noexcept { return *m_RTVDescriptorAllocator; }
    inline auto& GetDSVDescriptorAllocator() const noexcept { return *m_DSVDescriptorAllocator; }

private:
    auto GetStorageBufferViewRequirements() const noexcept -> StorageViewRequirements final {
        return {.offset_alignment = D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT, .size_alignment = 4};
    }

    static void ReportDebugLog(const ComPtr<ID3D12Device>& device);
    void        Profile() const;

    void IntegrateD3D12Logger();
    void UnregisterIntegratedD3D12Logger();

    ComPtr<IDXGIFactory2>       m_Factory;
    ComPtr<IDXGIAdapter4>       m_Adapter;
    ComPtr<ID3D12DeviceFactory> m_DeviceFactory;
    ComPtr<ID3D12Device>        m_Device;

    D3D12MA::ALLOCATION_CALLBACKS                                       m_CustomAllocationCallback;
    std::pmr::unordered_map<void*, std::pair<std::size_t, std::size_t>> m_CustomAllocationInfos;
    ComPtr<D3D12MA::Allocator>                                          m_MemoryAllocator;

    DWORD m_DebugCookie;

    std::unique_ptr<DescriptorAllocator> m_RTVDescriptorAllocator;
    std::unique_ptr<DescriptorAllocator> m_DSVDescriptorAllocator;
};

}  // namespace hitagi::gfx

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

    m_Logger->trace("Create RTV DSV Descriptor Allocators...");
    {
        m_RTVDescriptorAllocator = std::make_unique<DescriptorAllocator>(*m_Device.Get(), m_Logger, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        m_DSVDescriptorAllocator = std::make_unique<DescriptorAllocator>(*m_Device.Get(), m_Logger, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    }

    m_Logger->trace("Initialized.");
}

DX12Device::~DX12Device() {
    UnregisterIntegratedD3D12Logger();
#ifdef HITAGI_DEBUG
    report_debug_error_after_destroy_fn = [device = m_Device]() { DX12Device::ReportDebugLog(device); };
#endif
}

void DX12Device::Tick() {
    Device::Tick();
    if (m_EnableProfile) {
        Profile();
    }
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
