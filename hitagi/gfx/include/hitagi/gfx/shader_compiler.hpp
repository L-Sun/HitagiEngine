#pragma once
#include <hitagi/core/buffer.hpp>
#include <hitagi/gfx/gpu_resource.hpp>

namespace spdlog {
class logger;
}
struct IDxcUtils;
struct IDxcCompiler3;
struct IDxcResult;

namespace hitagi::gfx {
class ShaderCompiler {
public:
    ShaderCompiler(std::string_view name);
    ~ShaderCompiler();

    auto CompileToDXIL(const ShaderDesc& desc) const -> core::Buffer;
    auto CompileToSPIRV(const ShaderDesc& desc) const -> core::Buffer;

    auto ExtractVertexLayout(const ShaderDesc& desc) const -> VertexLayout;

private:
    auto CompileWithArgs(std::string_view source_code, const std::pmr::vector<std::pmr::wstring>& args) const -> IDxcResult*;
    auto GetShaderBuffer(IDxcResult* result) const -> core::Buffer;

    std::shared_ptr<spdlog::logger> m_Logger;
    IDxcUtils*                      m_DxcUtils       = nullptr;
    IDxcCompiler3*                  m_ShaderCompiler = nullptr;
};
}  // namespace hitagi::gfx