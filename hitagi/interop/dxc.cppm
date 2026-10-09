module;
#ifdef _WIN32
#include <unknwn.h>
#include <wrl.h>
#endif
#include <dxc/dxcapi.h>
#include <d3d12shader.h>

#ifndef _WIN32
namespace hitagi::interop {
template <typename T>
inline auto dxc_uuidof() { return __uuidof(T); }
}
#endif

export module interop.dxc;

export {
    using ::D3D12_SHADER_DESC;
    using ::D3D12_SIGNATURE_PARAMETER_DESC;
    using ::D3D_REGISTER_COMPONENT_FLOAT32;
    using ::D3D_REGISTER_COMPONENT_SINT32;
    using ::D3D_REGISTER_COMPONENT_UINT32;
    using ::D3D_REGISTER_COMPONENT_UNKNOWN;
    using ::DXC_OUT_ERRORS;
    using ::DXC_OUT_OBJECT;
    using ::DXC_OUT_REFLECTION;
    using ::DxcBuffer;
    using ::DxcCreateInstance;
    using ::HRESULT;
    using ::ID3D12ShaderReflection;
    using ::IDxcBlob;
    using ::IDxcBlobUtf8;
    using ::IDxcCompiler3;
    using ::IDxcIncludeHandler;
    using ::IDxcResult;
    using ::IDxcUtils;
    using ::UINT;
}
#ifdef _WIN32
export namespace Microsoft::WRL {
using ::Microsoft::WRL::ComPtr;
}
export {
    using ::IID_PPV_ARGS_Helper;
}
#else
export {
    using ::CComPtr;
}
export namespace hitagi::interop { using ::hitagi::interop::dxc_uuidof; }
#endif

export namespace hitagi::interop {
constexpr bool        dxc_failed(HRESULT result) noexcept { return FAILED(result); }
inline const auto     clsid_dxccompiler             = CLSID_DxcCompiler;
inline const auto     clsid_dxcutils                = CLSID_DxcUtils;
inline constexpr auto dxc_arg_debug                 = DXC_ARG_DEBUG;
inline constexpr auto dxc_arg_optimization_level0   = DXC_ARG_OPTIMIZATION_LEVEL0;
inline constexpr auto dxc_arg_pack_matrix_row_major = DXC_ARG_PACK_MATRIX_ROW_MAJOR;
inline constexpr auto dxc_cp_acp                    = DXC_CP_ACP;
}  // namespace hitagi::interop
