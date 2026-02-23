module;
#if defined(_WIN32)
#include <unknwn.h>
#include <dxc/dxcapi.h>
#endif
#include <spdlog/logger.h>

export module gfx.base:shader_compiler;

import std;
import core;
import :types;
import :gpu_resource;

export namespace hitagi::gfx {

class ShaderCompiler {
public:
    ShaderCompiler(std::string_view name);
    ~ShaderCompiler();

    auto CompileToDXIL(const ShaderDesc& desc) const -> core::Buffer;
    auto CompileToSPIRV(const ShaderDesc& desc) const -> core::Buffer;

    auto ExtractVertexLayout(const ShaderDesc& desc) const -> VertexLayout;

private:
    auto CompileWithArgs(std::string_view source_code, const std::pmr::vector<std::pmr::wstring>& args) const -> ::IDxcResult*;
    auto GetShaderBuffer(::IDxcResult* result) const -> core::Buffer;

    std::shared_ptr<spdlog::logger> m_Logger;
    ::IDxcUtils*                    m_DxcUtils       = nullptr;
    ::IDxcCompiler3*                m_ShaderCompiler = nullptr;
};

}  // namespace hitagi::gfx
