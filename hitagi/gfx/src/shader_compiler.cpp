#include <hitagi/gfx/shader_compiler.hpp>
#include <hitagi/gfx/utils.hpp>
#include <hitagi/utils/logger.hpp>

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

#include <d3d12shader.h>
#include <fmt/color.h>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/range/conversion.hpp>
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
        default:
            utils::unreachable();
    }
}

inline auto create_compile_args(const ShaderDesc& desc, bool spirv = false) noexcept -> std::pmr::vector<std::pmr::wstring> {
    std::pmr::vector<std::pmr::wstring> args;

    const std::filesystem::path asset_shader_dir = "assets/shaders";

    // shader file
    if (desc.path.empty()) {
        args.emplace_back(desc.name.begin(), desc.name.end());
    } else {
        args.emplace_back(desc.path.wstring());

        std::filesystem::path include_dir = std::filesystem::is_directory(desc.path) ? desc.path : desc.path.parent_path();
        if (include_dir != asset_shader_dir) {
            args.emplace_back(L"-I");
            args.emplace_back(include_dir.wstring());
        }
    }

    args.emplace_back(L"-I");
    args.emplace_back(asset_shader_dir.wstring());

    // shader entry
    args.emplace_back(L"-E");
    args.emplace_back(desc.entry.begin(), desc.entry.end());

    // shader model
    args.emplace_back(L"-T");
    args.emplace_back(get_shader_model_version(desc.type));

    if (spirv) {
        args.emplace_back(L"-spirv");
        args.emplace_back(L"-fvk-use-dx-layout");
        args.emplace_back(L"-fspv-use-legacy-buffer-matrix-order");
    }

    args.emplace_back(L"-HV 2021");

    // We use row major order
    args.emplace_back(DXC_ARG_PACK_MATRIX_ROW_MAJOR);

#ifdef HITAGI_DEBUG
    args.emplace_back(DXC_ARG_OPTIMIZATION_LEVEL0);
    args.emplace_back(DXC_ARG_DEBUG);
    args.emplace_back(L"-Qembed_debug");
#endif
    return args;
}

inline constexpr auto from_dxc_reflection_attribute(const D3D12_SIGNATURE_PARAMETER_DESC& desc) noexcept -> VertexAttribute {
    Format format = Format::UNKNOWN;
    switch (desc.ComponentType) {
        case D3D_REGISTER_COMPONENT_UNKNOWN: {
            format = Format::UNKNOWN;
        } break;
        case D3D_REGISTER_COMPONENT_UINT32: {
            if (desc.Mask == 0b00000001) {
                format = Format::R32_UINT;
            } else if (desc.Mask == 0b0000'0011) {
                format = Format::R32G32_UINT;
            } else if (desc.Mask == 0b0000'0111) {
                format = Format::R32G32B32_UINT;
            } else if (desc.Mask == 0b0000'1111) {
                format = Format::R32G32B32A32_UINT;
            }
        } break;
        case D3D_REGISTER_COMPONENT_SINT32: {
            if (desc.Mask == 0b00000001) {
                format = Format::R32_SINT;
            } else if (desc.Mask == 0b0000'0011) {
                format = Format::R32G32_SINT;
            } else if (desc.Mask == 0b0000'0111) {
                format = Format::R32G32B32_SINT;
            } else if (desc.Mask == 0b0000'1111) {
                format = Format::R32G32B32A32_SINT;
            }
        } break;
        case D3D_REGISTER_COMPONENT_FLOAT32: {
            if (desc.Mask == 0b00000001) {
                format = Format::R32_FLOAT;
            } else if (desc.Mask == 0b0000'0011) {
                format = Format::R32G32_FLOAT;
            } else if (desc.Mask == 0b0000'0111) {
                format = Format::R32G32B32_FLOAT;
            } else if (desc.Mask == 0b0000'1111) {
                format = Format::R32G32B32A32_FLOAT;
            }
        } break;
    }
    return VertexAttribute{
        .semantic = std::pmr::string(fmt::format("{}{}", desc.SemanticName, desc.SemanticIndex)),
        .format   = format,
        .binding  = desc.Register,
        .stride   = get_format_byte_size(format),
    };
}

ShaderCompiler::ShaderCompiler(std::string_view name)
    : m_Logger(utils::try_create_logger(fmt::format("ShaderCompiler{}", utils::add_parentheses(name)))) {
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

ShaderCompiler::~ShaderCompiler() {
    if (m_DxcUtils) m_DxcUtils->Release();
    if (m_ShaderCompiler) m_ShaderCompiler->Release();
}

auto ShaderCompiler::CompileToDXIL(const ShaderDesc& desc) const -> core::Buffer {
    return GetShaderBuffer(CompileWithArgs(desc.source_code, create_compile_args(desc, false)));
}

auto ShaderCompiler::CompileToSPIRV(const ShaderDesc& desc) const -> core::Buffer {
    // We use SPIR-V here
    return GetShaderBuffer(CompileWithArgs(desc.source_code, create_compile_args(desc, true)));
}

auto ShaderCompiler::ExtractVertexLayout(const ShaderDesc& desc) const -> VertexLayout {
    if (desc.type != ShaderType::Vertex) {
        m_Logger->warn(
            "Can not get vertex layout from not vertex shader(type: {}, Name: {})",
            fmt::styled(desc.type, fmt::fg(fmt::color::orange)),
            fmt::styled(desc.type, fmt::fg(fmt::color::orange)));
        return {};
    }

    ComPtr<IDxcResult> compiled_result = CompileWithArgs(desc.source_code, create_compile_args(desc, false));

    ComPtr<IDxcBlob> dxil_reflection_buffer = nullptr;
    if (FAILED(compiled_result->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&dxil_reflection_buffer), nullptr))) {
        m_Logger->error("Failed to get compiled reflection.");
        return {};
    }

    DxcBuffer dxc_buffer{
        .Ptr      = dxil_reflection_buffer->GetBufferPointer(),
        .Size     = dxil_reflection_buffer->GetBufferSize(),
        .Encoding = DXC_CP_ACP,
    };

    ComPtr<ID3D12ShaderReflection> shader_reflection = nullptr;
    if (FAILED(m_DxcUtils->CreateReflection(&dxc_buffer, IID_PPV_ARGS(&shader_reflection))) || shader_reflection == nullptr) {
        m_Logger->error("Fail to create reflection");
        return {};
    }

    D3D12_SHADER_DESC d3d12_shader_desc{};
    if (FAILED(shader_reflection->GetDesc(&d3d12_shader_desc))) {
        m_Logger->error("Fail to get shader desc");
        return {};
    }

    return ranges::views::iota(0) |
           ranges::views::take(d3d12_shader_desc.InputParameters) |
           ranges::views::transform([&](UINT location) {
               D3D12_SIGNATURE_PARAMETER_DESC input_desc{};
               shader_reflection->GetInputParameterDesc(location, &input_desc);
               return from_dxc_reflection_attribute(input_desc);
           }) |
           ranges::to<VertexLayout>();
}

auto ShaderCompiler::CompileWithArgs(std::string_view source_code, const std::pmr::vector<std::pmr::wstring>& args) const -> IDxcResult* {
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

    ComPtr<IDxcIncludeHandler> include_handler = nullptr;

    if (FAILED(m_DxcUtils->CreateDefaultIncludeHandler(&include_handler))) {
        m_Logger->error("failed to create include handler");
        return {};
    }
    DxcBuffer source_buffer{
        .Ptr      = source_code.data(),
        .Size     = source_code.size(),
        .Encoding = DXC_CP_ACP,
    };

    ComPtr<IDxcResult> compiled_result = nullptr;

    if (FAILED(m_ShaderCompiler->Compile(
            &source_buffer,
            p_args.data(),
            p_args.size(),
            include_handler.Get(),
            IID_PPV_ARGS(&compiled_result)))) {
        m_Logger->error("Failed to compile shader.");
        return {};
    }

    ComPtr<IDxcBlobUtf8> compile_errors = nullptr;
    compiled_result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&compile_errors), nullptr);
    // Note that DirectX compiler would return null if no errors or warnings are present.
    // IDxcCompiler3::Compile will always return an error buffer, but its length will be zero if there are no warnings or errors.
    if (compile_errors != nullptr && compile_errors->GetStringLength() != 0) {
        m_Logger->warn("Warnings and Errors:\n{}\n", compile_errors->GetStringPointer());
    }

    HRESULT hr;
    if (FAILED(compiled_result->GetStatus(&hr))) {
        m_Logger->error("Failed to get compile status.");
        return {};
    }
    if (FAILED(hr)) {
        m_Logger->error("Failed to compile shader.");
        return {};
    }

    return compiled_result.Detach();
}

auto ShaderCompiler::GetShaderBuffer(IDxcResult* _compiled_result) const -> core::Buffer {
    ComPtr<IDxcResult> compiled_result = _compiled_result;

    ComPtr<IDxcBlob> shader_buffer = nullptr;
    if (FAILED(compiled_result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader_buffer), nullptr)) || shader_buffer == nullptr) {
        m_Logger->error("Failed to get compiled output.");
        return {};
    }

    return core::Buffer{shader_buffer->GetBufferSize(), reinterpret_cast<std::byte*>(shader_buffer->GetBufferPointer())};
}

}  // namespace hitagi::gfx