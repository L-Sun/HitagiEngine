#pragma once
#include <hitagi/core/buffer.hpp>
#include <hitagi/gfx/gpu_resource.hpp>

#if defined(_WIN32)
#include <unknwn.h>
#include <wrl.h>
#include <dxc/dxcapi.h>
using namespace Microsoft::WRL;
#elif defined(__linux__)
#include <dxc/dxcapi.h>
template <typename T>
struct ComPtr : public CComPtr<T> {
    inline auto Get() const noexcept { return this->p; }
};
#endif

namespace spdlog {
class logger;
}

namespace hitagi::gfx {
class ShaderCompiler {
public:
    ShaderCompiler(std::string_view name);

    auto CompileToDXIL(const ShaderDesc& desc) const -> core::Buffer;
    auto CompileToSPIRV(const ShaderDesc& desc) const -> core::Buffer;
    auto CompileWithArgs(std::string_view source_code, const std::pmr::vector<std::pmr::wstring>& args) const -> core::Buffer;

private:
    std::shared_ptr<spdlog::logger> m_Logger;
    ComPtr<IDxcUtils>               m_DxcUtils;
    ComPtr<IDxcCompiler3>           m_ShaderCompiler;
};
}  // namespace hitagi::gfx