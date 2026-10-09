module;
#include <d3d12.h>
#include <d3d12shader.h>
#include <d3dx12/d3dx12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <comdef.h>

export module interop.dx12;

// These declarations remain owned by the global module (the SDK's ABI).
export {
    // Windows.h supplies IID_PPV_ARGS; its WRL overload must also be visible.
    using ::GetRequiredIntermediateSize;
    using ::IID_PPV_ARGS_Helper;
    using ::UpdateSubresources;
    using ::operator|;
    using ::operator|=;
    using ::operator&;
    using ::operator&=;
    using ::operator^;
    using ::operator^=;
    using ::operator~;
    using ::D3D12_BARRIER_ACCESS;
    using enum ::D3D12_BARRIER_ACCESS;
    using ::D3D12_BARRIER_LAYOUT;
    using enum ::D3D12_BARRIER_LAYOUT;
    using ::D3D12_BARRIER_SYNC;
    using enum ::D3D12_BARRIER_SYNC;
    using ::D3D12_BARRIER_TYPE;
    using enum ::D3D12_BARRIER_TYPE;
    using ::D3D12_BLEND;
    using enum ::D3D12_BLEND;
    using ::D3D12_BLEND_OP;
    using enum ::D3D12_BLEND_OP;
    using ::D3D12_BUFFER_SRV_FLAGS;
    using enum ::D3D12_BUFFER_SRV_FLAGS;
    using ::D3D12_BUFFER_UAV_FLAGS;
    using enum ::D3D12_BUFFER_UAV_FLAGS;
    using ::D3D12_CLEAR_FLAGS;
    using enum ::D3D12_CLEAR_FLAGS;
    using ::D3D12_COMMAND_LIST_TYPE;
    using enum ::D3D12_COMMAND_LIST_TYPE;
    using ::D3D12_COMMAND_QUEUE_FLAGS;
    using enum ::D3D12_COMMAND_QUEUE_FLAGS;
    using ::D3D12_COMPARISON_FUNC;
    using enum ::D3D12_COMPARISON_FUNC;
    using ::D3D12_CONSERVATIVE_RASTERIZATION_MODE;
    using enum ::D3D12_CONSERVATIVE_RASTERIZATION_MODE;
    using ::D3D12_CULL_MODE;
    using enum ::D3D12_CULL_MODE;
    using ::D3D12_DEPTH_WRITE_MASK;
    using enum ::D3D12_DEPTH_WRITE_MASK;
    using ::D3D12_DESCRIPTOR_HEAP_FLAGS;
    using enum ::D3D12_DESCRIPTOR_HEAP_FLAGS;
    using ::D3D12_DESCRIPTOR_HEAP_TYPE;
    using enum ::D3D12_DESCRIPTOR_HEAP_TYPE;
    using ::D3D12_DEVICE_FACTORY_FLAGS;
    using enum ::D3D12_DEVICE_FACTORY_FLAGS;
    using ::D3D12_DSV_DIMENSION;
    using enum ::D3D12_DSV_DIMENSION;
    using ::D3D12_FENCE_FLAGS;
    using enum ::D3D12_FENCE_FLAGS;
    using ::D3D12_FILL_MODE;
    using enum ::D3D12_FILL_MODE;
    using ::D3D12_FILTER_REDUCTION_TYPE;
    using enum ::D3D12_FILTER_REDUCTION_TYPE;
    using ::D3D12_FILTER_TYPE;
    using enum ::D3D12_FILTER_TYPE;
    using ::D3D12_HEAP_FLAGS;
    using enum ::D3D12_HEAP_FLAGS;
    using ::D3D12_HEAP_TYPE;
    using enum ::D3D12_HEAP_TYPE;
    using ::D3D12_INPUT_CLASSIFICATION;
    using enum ::D3D12_INPUT_CLASSIFICATION;
    using ::D3D12_LOGIC_OP;
    using enum ::D3D12_LOGIC_OP;
    using ::D3D12_MESSAGE_CALLBACK_FLAGS;
    using enum ::D3D12_MESSAGE_CALLBACK_FLAGS;
    using ::D3D12_MESSAGE_SEVERITY;
    using enum ::D3D12_MESSAGE_SEVERITY;
    using ::D3D12_PRIMITIVE_TOPOLOGY_TYPE;
    using enum ::D3D12_PRIMITIVE_TOPOLOGY_TYPE;
    using ::D3D12_RESOURCE_BINDING_TIER;
    using enum ::D3D12_RESOURCE_BINDING_TIER;
    using ::D3D12_RESOURCE_FLAGS;
    using enum ::D3D12_RESOURCE_FLAGS;
    using ::D3D12_RESOURCE_STATES;
    using enum ::D3D12_RESOURCE_STATES;
    using ::D3D12_RLDO_FLAGS;
    using enum ::D3D12_RLDO_FLAGS;
    using ::D3D12_ROOT_SIGNATURE_FLAGS;
    using enum ::D3D12_ROOT_SIGNATURE_FLAGS;
    using ::D3D12_RTV_DIMENSION;
    using enum ::D3D12_RTV_DIMENSION;
    using ::D3D12_SHADER_VISIBILITY;
    using enum ::D3D12_SHADER_VISIBILITY;
    using ::D3D12_SRV_DIMENSION;
    using enum ::D3D12_SRV_DIMENSION;
    using ::D3D12_STENCIL_OP;
    using enum ::D3D12_STENCIL_OP;
    using ::D3D12_TEXTURE_ADDRESS_MODE;
    using enum ::D3D12_TEXTURE_ADDRESS_MODE;
    using ::D3D12_TEXTURE_BARRIER_FLAGS;
    using enum ::D3D12_TEXTURE_BARRIER_FLAGS;
    using ::D3D12_UAV_DIMENSION;
    using enum ::D3D12_UAV_DIMENSION;
    using ::D3D_FEATURE_LEVEL;
    using enum ::D3D_FEATURE_LEVEL;
    using ::D3D_PRIMITIVE_TOPOLOGY;
    using enum ::D3D_PRIMITIVE_TOPOLOGY;
    using ::D3D_REGISTER_COMPONENT_TYPE;
    using enum ::D3D_REGISTER_COMPONENT_TYPE;
    using ::D3D_ROOT_SIGNATURE_VERSION;
    using enum ::D3D_ROOT_SIGNATURE_VERSION;
    using ::D3D_SHADER_MODEL;
    using enum ::D3D_SHADER_MODEL;
    using ::DXGI_ADAPTER_FLAG;
    using enum ::DXGI_ADAPTER_FLAG;
    using ::DXGI_ALPHA_MODE;
    using enum ::DXGI_ALPHA_MODE;
    using ::DXGI_FEATURE;
    using enum ::DXGI_FEATURE;
    using ::DXGI_FORMAT;
    using enum ::DXGI_FORMAT;
    using ::DXGI_GPU_PREFERENCE;
    using enum ::DXGI_GPU_PREFERENCE;
    using ::DXGI_SCALING;
    using enum ::DXGI_SCALING;
    using ::DXGI_SWAP_CHAIN_FLAG;
    using enum ::DXGI_SWAP_CHAIN_FLAG;
    using ::DXGI_SWAP_EFFECT;
    using enum ::DXGI_SWAP_EFFECT;

    using ::_com_error;
    using ::CD3DX12_BOX;
    using ::CD3DX12_CPU_DESCRIPTOR_HANDLE;
    using ::CD3DX12_RESOURCE_DESC;
    using ::CD3DX12_ROOT_PARAMETER1;
    using ::CD3DX12_TEXTURE_COPY_LOCATION;
    using ::CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC;
    using ::CD3DX12FeatureSupport;
    using ::CLSID_D3D12Debug;
    using ::CLSID_D3D12SDKConfiguration;
    using ::CreateDXGIFactory2;
    using ::D3D12_BARRIER_GROUP;
    using ::D3D12_BLEND_DESC;
    using ::D3D12_BUFFER_BARRIER;
    using ::D3D12_CLEAR_VALUE;
    using ::D3D12_COLOR_WRITE_ENABLE;
    using ::D3D12_COMMAND_QUEUE_DESC;
    using ::D3D12_COMPUTE_PIPELINE_STATE_DESC;
    using ::D3D12_CPU_DESCRIPTOR_HANDLE;
    using ::D3D12_DEPTH_STENCIL_DESC;
    using ::D3D12_DEPTH_STENCIL_VIEW_DESC;
    using ::D3D12_DEPTH_STENCILOP_DESC;
    using ::D3D12_DESCRIPTOR_HEAP_DESC;
    using ::D3D12_GLOBAL_BARRIER;
    using ::D3D12_GRAPHICS_PIPELINE_STATE_DESC;
    using ::D3D12_INDEX_BUFFER_VIEW;
    using ::D3D12_INPUT_ELEMENT_DESC;
    using ::D3D12_INPUT_LAYOUT_DESC;
    using ::D3D12_MESSAGE_CATEGORY;
    using ::D3D12_MESSAGE_ID;
    using ::D3D12_PLACED_SUBRESOURCE_FOOTPRINT;
    using ::D3D12_RASTERIZER_DESC;
    using ::D3D12_RECT;
    using ::D3D12_RENDER_TARGET_VIEW_DESC;
    using ::D3D12_SAMPLER_DESC;
    using ::D3D12_SHADER_BYTECODE;
    using ::D3D12_SHADER_RESOURCE_VIEW_DESC;
    using ::D3D12_SUBRESOURCE_DATA;
    using ::D3D12_TEXTURE_BARRIER;
    using ::D3D12_UNORDERED_ACCESS_VIEW_DESC;
    using ::D3D12_VERTEX_BUFFER_VIEW;
    using ::D3D12_VIEWPORT;
    using ::D3D12CalcSubresource;
    using ::D3D12GetInterface;
    using ::D3DX12SerializeVersionedRootSignature;
    using ::DXGI_ADAPTER_DESC1;
    using ::DXGI_SWAP_CHAIN_DESC1;
    using ::ID3D12CommandAllocator;
    using ::ID3D12CommandList;
    using ::ID3D12CommandQueue;
    using ::ID3D12Debug;
    using ::ID3D12Debug3;
    using ::ID3D12DebugDevice1;
    using ::ID3D12DescriptorHeap;
    using ::ID3D12Device;
    using ::ID3D12DeviceFactory;
    using ::ID3D12Fence;
    using ::ID3D12GraphicsCommandList;
    using ::ID3D12GraphicsCommandList7;
    using ::ID3D12InfoQueue1;
    using ::ID3D12Object;
    using ::ID3D12PipelineState;
    using ::ID3D12Resource;
    using ::ID3D12RootSignature;
    using ::ID3D12SDKConfiguration1;
    using ::ID3DBlob;
    using ::IDXGIAdapter1;
    using ::IDXGIAdapter4;
    using ::IDXGIFactory2;
    using ::IDXGIFactory5;
    using ::IDXGIFactory6;
    using ::IDXGISwapChain1;
    using ::IDXGISwapChain4;
}

export namespace Microsoft::WRL {
using ::Microsoft::WRL::ComPtr;
}

export namespace hitagi::interop {
constexpr bool failed(HRESULT result) noexcept { return FAILED(result); }
constexpr bool succeeded(HRESULT result) noexcept { return SUCCEEDED(result); }

inline constexpr auto default_shader_4_component_mapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
inline constexpr auto raw_uav_srv_byte_alignment         = D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT;
inline constexpr auto dxgi_create_factory_debug          = DXGI_CREATE_FACTORY_DEBUG;
inline constexpr auto dxgi_mwa_no_alt_enter              = DXGI_MWA_NO_ALT_ENTER;
inline constexpr auto dxgi_present_allow_tearing         = DXGI_PRESENT_ALLOW_TEARING;
inline constexpr auto dxgi_usage_render_target_output    = DXGI_USAGE_RENDER_TARGET_OUTPUT;

constexpr auto encode_basic_filter(D3D12_FILTER_TYPE min, D3D12_FILTER_TYPE mag,
                                   D3D12_FILTER_TYPE mip, D3D12_FILTER_REDUCTION_TYPE reduction) noexcept {
    return D3D12_ENCODE_BASIC_FILTER(min, mag, mip, reduction);
}
}  // namespace hitagi::interop
