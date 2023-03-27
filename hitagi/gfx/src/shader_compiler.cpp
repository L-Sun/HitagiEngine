#include <hitagi/gfx/shader_compiler.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>

namespace hitagi::gfx {
inline constexpr auto get_shader_model_version(ShaderType type) noexcept {
    switch (type) {
        case ShaderType::Vertex:
            return L"vs_6_7";
        case ShaderType::Pixel:
            return L"ps_6_7";
        case ShaderType::Geometry:
            return L"gs_6_7";
        case ShaderType::Compute:
            return L"cs_6_7";
    }
}

inline auto create_compile_args(const ShaderDesc& desc, bool spirv = false) noexcept -> std::pmr::vector<std::pmr::wstring> {
    std::pmr::vector<std::pmr::wstring> args;

    const std::filesystem::path asset_shader_dir = "assets/shaders";

    // shader file
    if (desc.path.empty()) {
        args.emplace_back(std::pmr::wstring(desc.name.begin(), desc.name.end()));

    } else {
        std::filesystem::path include_dir = std::filesystem::is_directory(desc.path) ? desc.path : desc.path.parent_path();
        if (include_dir != asset_shader_dir) {
            args.emplace_back(L"-I");
            args.emplace_back(std::pmr::wstring(include_dir.wstring()));
        }
    }

    args.emplace_back(L"-I");
    args.emplace_back(std::pmr::wstring(asset_shader_dir.wstring()));

    // shader entry
    args.emplace_back(L"-E");
    args.emplace_back(std::pmr::wstring(desc.entry.begin(), desc.entry.end()));

    // shader model
    args.emplace_back(L"-T");
    args.emplace_back(get_shader_model_version(desc.type));

    if (spirv) args.emplace_back(L"-spirv");

    args.emplace_back(L"-HV 2021");

    // We use row major order
    args.emplace_back(DXC_ARG_PACK_MATRIX_ROW_MAJOR);

    // make sure all resource bound
    args.emplace_back(DXC_ARG_ALL_RESOURCES_BOUND);
#ifdef HITAGI_DEBUG
    args.emplace_back(DXC_ARG_OPTIMIZATION_LEVEL0);
    args.emplace_back(DXC_ARG_DEBUG);
    args.emplace_back(L"-Qembed_debug");
#endif
    return args;
}

ShaderCompiler::ShaderCompiler(std::string_view name) : m_Logger(spdlog::stdout_color_mt(std::string(name))) {
    m_Logger->trace("Create shader compiler...");
    {
        if (FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_DxcUtils)))) {
            throw std::runtime_error("Failed to create DXC utils!");
        }
        if (FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_ShaderCompiler)))) {
            throw std::runtime_error("Failed to create DXC shader compiler!");
        }
    }
}

auto ShaderCompiler::CompileToDXIL(const ShaderDesc& desc) const -> core::Buffer {
    return CompileWithArgs(desc.source_code, create_compile_args(desc));
}

auto ShaderCompiler::CompileToSPIRV(const ShaderDesc& desc) const -> core::Buffer {
    // We use SPIR-V here
    return CompileWithArgs(desc.source_code, create_compile_args(desc, true));
}

auto ShaderCompiler::CompileWithArgs(std::string_view source_code, const std::pmr::vector<std::pmr::wstring>& args) const -> core::Buffer {
    std::pmr::string compile_command;
    for (const auto& arg : args) {
        compile_command.append(arg.begin(), arg.end());
        compile_command.push_back(' ');
    }
    m_Logger->info("Compile Shader: {}", compile_command);

    std::pmr::vector<const wchar_t*> p_args;
    std::transform(
        args.begin(), args.end(),
        std::back_inserter(p_args),
        [](const auto& arg) { return arg.c_str(); });

    ComPtr<IDxcIncludeHandler> include_handler;

    m_DxcUtils->CreateDefaultIncludeHandler(&include_handler);
    DxcBuffer source_buffer{
        .Ptr      = source_code.data(),
        .Size     = source_code.size(),
        .Encoding = DXC_CP_ACP,
    };

    ComPtr<IDxcResult> compile_result;

    if (FAILED(m_ShaderCompiler->Compile(
            &source_buffer,
            p_args.data(),
            p_args.size(),
            include_handler.Get(),
            IID_PPV_ARGS(&compile_result)))) {
        m_Logger->error("Failed to compile shader.");
        return {};
    }

    ComPtr<IDxcBlobUtf8> compile_errors;
    compile_result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&compile_errors), nullptr);
    // Note that DirectX compiler would return null if no errors or warnings are present.
    // IDxcCompiler3::Compile will always return an error buffer, but its length will be zero if there are no warnings or errors.
    if (compile_errors != nullptr && compile_errors->GetStringLength() != 0) {
        m_Logger->warn("Warnings and Errors:\n{}\n", compile_errors->GetStringPointer());
    }

    HRESULT hr;
    compile_result->GetStatus(&hr);
    if (FAILED(hr)) {
        m_Logger->error("Failed to compile shader.");
        return {};
    }

    ComPtr<IDxcBlob>     shader_buffer;
    ComPtr<IDxcBlobWide> shader_name;

    compile_result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader_buffer), &shader_name);
    if (shader_buffer == nullptr) {
        m_Logger->error("Failed to compile shader.");
        return {};
    }

    return {shader_buffer->GetBufferSize(), reinterpret_cast<const std::byte*>(shader_buffer->GetBufferPointer())};
}

}  // namespace hitagi::gfx