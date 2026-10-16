export module gfx.base:shader_compiler;
import interop.slang;
import interop.spdlog;
import std;
import utils;
import core;
import :types;
import :gpu_resource;
import :utils;

namespace hitagi::gfx {
namespace sl = interop::slang;
template <typename T>
using SlangPtr = interop::SlangPtr<T>;

struct ShaderCompilerState {
    std::mutex                      mutex;
    SlangPtr<sl::IGlobalSession>    global;
    std::shared_ptr<spdlog::logger> logger;

    void Check(int result, const SlangPtr<sl::IBlob>& diagnostics, std::string_view operation) const {
        std::string message;
        if (diagnostics && diagnostics->getBufferSize())
            message.assign(static_cast<const char*>(diagnostics->getBufferPointer()), diagnostics->getBufferSize());
        if (interop::SlangFailed(result)) throw std::runtime_error(std::format("Slang {}: {}", operation, message));
        if (!message.empty()) logger->warn("Slang {}: {}", operation, message);
    }
};
}  // namespace hitagi::gfx

export namespace hitagi::gfx {

// A session owns a coherent module snapshot. Compile several entry points or
// specializations from this program to reuse Slang's front-end work.
class ShaderProgram {
public:
    ~ShaderProgram();
    ShaderProgram(const ShaderProgram&)            = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;
    auto           Compile(std::string_view entry, ShaderType stage,
                           std::span<const std::pmr::string> specialization_types = {}) const -> CompiledShader;

private:
    friend class ShaderCompiler;
    explicit ShaderProgram(std::shared_ptr<ShaderCompilerState> state) : m_State(std::move(state)) {}
    std::shared_ptr<ShaderCompilerState> m_State;
    SlangPtr<sl::ISession>               m_Session;
    std::vector<sl::IModule*>            m_Modules;
};

class ShaderCompiler {
public:
    explicit ShaderCompiler(std::string_view name);
    auto LoadProgram(const ShaderProgramDesc& desc, ShaderTarget target) const -> std::shared_ptr<ShaderProgram>;

private:
    std::shared_ptr<ShaderCompilerState> m_State;
};
}  // namespace hitagi::gfx

namespace hitagi::gfx {
auto SlangStage(ShaderType stage) {
    switch (stage) {
        case ShaderType::Vertex:
            return interop::slang_vertex;
        case ShaderType::Pixel:
            return interop::slang_fragment;
        case ShaderType::Geometry:
            return interop::slang_geometry;
        case ShaderType::Compute:
            return interop::slang_compute;
        default:
            throw std::invalid_argument("Unsupported shader stage");
    }
}

auto VertexFormat(sl::TypeReflection* type) -> Format {
    if (type->getKind() != sl::TypeReflection::Kind::Scalar && type->getKind() != sl::TypeReflection::Kind::Vector)
        throw std::invalid_argument("Vertex inputs must be scalar or vector values");
    const auto count = type->getKind() == sl::TypeReflection::Kind::Vector ? type->getColumnCount() : 1;
    if (count < 1 || count > 4) throw std::invalid_argument("Unsupported vertex input shape");
    switch (type->getScalarType()) {
        case sl::TypeReflection::ScalarType::Float32: {
            constexpr std::array formats{Format::R32_FLOAT, Format::R32G32_FLOAT, Format::R32G32B32_FLOAT, Format::R32G32B32A32_FLOAT};
            return formats[count - 1];
        }
        case sl::TypeReflection::ScalarType::Int32: {
            constexpr std::array formats{Format::R32_SINT, Format::R32G32_SINT, Format::R32G32B32_SINT, Format::R32G32B32A32_SINT};
            return formats[count - 1];
        }
        case sl::TypeReflection::ScalarType::UInt32: {
            constexpr std::array formats{Format::R32_UINT, Format::R32G32_UINT, Format::R32G32B32_UINT, Format::R32G32B32A32_UINT};
            return formats[count - 1];
        }
        default:
            throw std::invalid_argument("Unsupported vertex input scalar type");
    }
}

void ReflectVertexInput(sl::VariableLayoutReflection* variable, std::uint32_t parent_location, VertexLayout& result) {
    auto*      layout   = variable->getTypeLayout();
    const auto location = parent_location + static_cast<std::uint32_t>(variable->getOffset(sl::ParameterCategory::VaryingInput));
    if (layout->getKind() == sl::TypeReflection::Kind::Struct) {
        for (unsigned i = 0; i < layout->getFieldCount(); ++i)
            ReflectVertexInput(layout->getFieldByIndex(i), location, result);
        return;
    }
    const char* semantic = variable->getSemanticName();
    if (!semantic) return;
    std::string name(semantic);
    std::ranges::transform(name, name.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    if (name.starts_with("SV_")) return;
    bool input = false;
    for (unsigned i = 0; i < layout->getCategoryCount(); ++i)
        input |= layout->getCategoryByIndex(i) == sl::ParameterCategory::VaryingInput;
    if (!input) return;
    const auto format = VertexFormat(variable->getType());
    result.emplace_back(VertexAttribute{
        .semantic = std::pmr::string(std::format("{}{}", name, variable->getSemanticIndex())),
        .format   = format,
        .binding  = static_cast<std::uint32_t>(result.size()),
        .stride   = get_format_byte_size(format),
        .location = location,
    });
}

ShaderCompiler::ShaderCompiler(std::string_view name) : m_State(std::make_shared<ShaderCompilerState>()) {
    m_State->logger = utils::try_create_logger(std::format("ShaderCompiler{}", utils::add_parentheses(name)));
    m_State->Check(sl::createGlobalSession(m_State->global.writeRef()), {}, "create global session");
}

auto ShaderCompiler::LoadProgram(const ShaderProgramDesc& desc, ShaderTarget target) const -> std::shared_ptr<ShaderProgram> {
    if (desc.modules.empty()) throw std::invalid_argument("Shader program has no modules");
    auto                     program = std::shared_ptr<ShaderProgram>(new ShaderProgram(m_State));
    std::scoped_lock         lock(m_State->mutex);
    std::vector<std::string> paths{"assets/shaders"};
    for (const auto& module : desc.modules) {
        if (!module.path.empty()) paths.emplace_back(module.path.parent_path().string());
    }
    std::vector<const char*> search_paths;
    for (const auto& path : paths) search_paths.push_back(path.c_str());
    const sl::PreprocessorMacroDesc      macro{"HITAGI_SPIRV", target == ShaderTarget::SPIRV ? "1" : "0"};
    std::vector<sl::CompilerOptionEntry> options = {
        {sl::CompilerOptionName::VulkanUseEntryPointName, {sl::CompilerOptionValueKind::Int, 1}},
        {sl::CompilerOptionName::MatrixLayoutRow, {sl::CompilerOptionValueKind::Int, 1}},
#ifdef HITAGI_DEBUG
        {sl::CompilerOptionName::Optimization, {sl::CompilerOptionValueKind::Int, 0}},
        {sl::CompilerOptionName::DebugInformation, {sl::CompilerOptionValueKind::Int, 1}},
#endif
    };
    if (target == ShaderTarget::SPIRV)
        options.push_back({sl::CompilerOptionName::BindlessSpaceIndex, {sl::CompilerOptionValueKind::Int, 0}});
    sl::TargetDesc target_desc{};
    target_desc.format  = target == ShaderTarget::DXIL ? interop::slang_dxil : interop::slang_spirv;
    target_desc.profile = m_State->global->findProfile(target == ShaderTarget::DXIL ? "sm_6_7" : "spirv_1_5");
    sl::SessionDesc session_desc{};
    session_desc.targets                  = &target_desc;
    session_desc.targetCount              = 1;
    session_desc.defaultMatrixLayoutMode  = interop::slang_row_major;
    session_desc.searchPaths              = search_paths.data();
    session_desc.searchPathCount          = search_paths.size();
    session_desc.preprocessorMacros       = &macro;
    session_desc.preprocessorMacroCount   = 1;
    session_desc.compilerOptionEntries    = options.data();
    session_desc.compilerOptionEntryCount = std::size(options);
    m_State->Check(m_State->global->createSession(session_desc, program->m_Session.writeRef()), {}, "create session");
    std::unordered_set<std::string> names;
    for (std::size_t i = 0; i < desc.modules.size(); ++i) {
        const auto&       source = desc.modules[i];
        const std::string name   = source.name.empty() ? std::format("module_{}", i) : std::string(source.name);
        if (!names.insert(name).second) throw std::invalid_argument("Duplicate shader module name: " + name);
        SlangPtr<sl::IBlob> diagnostics;
        sl::IModule*        module = nullptr;
        if (source.source_code.empty()) {
            if (source.name.empty()) throw std::invalid_argument("File modules require an import name");
            module = program->m_Session->loadModule(name.c_str(), diagnostics.writeRef());
        } else {
            const auto path = source.path.empty() ? name + ".slang" : source.path.string();
            module          = program->m_Session->loadModuleFromSourceString(name.c_str(), path.c_str(), source.source_code.c_str(), diagnostics.writeRef());
        }
        m_State->Check(module ? 0 : -1, diagnostics, "load module " + name);
        program->m_Modules.push_back(module);
    }
    return program;
}

ShaderProgram::~ShaderProgram() {
    std::scoped_lock lock(m_State->mutex);
    m_Session.setNull();
}

auto ShaderProgram::Compile(std::string_view entry, ShaderType stage,
                            std::span<const std::pmr::string> specialization_types) const -> CompiledShader {
    std::scoped_lock          lock(m_State->mutex);
    SlangPtr<sl::IBlob>       diagnostics;
    SlangPtr<sl::IEntryPoint> entry_point;
    const std::string         entry_name(entry);
    auto                      status = m_Modules.back()->findAndCheckEntryPoint(entry_name.c_str(), SlangStage(stage), entry_point.writeRef(), diagnostics.writeRef());
    m_State->Check(status, diagnostics, "find entry " + entry_name);
    std::vector<sl::IComponentType*> components(m_Modules.begin(), m_Modules.end());
    components.push_back(entry_point.get());
    SlangPtr<sl::IComponentType> composed;
    status = m_Session->createCompositeComponentType(components.data(), components.size(), composed.writeRef(), diagnostics.writeRef());
    m_State->Check(status, diagnostics, "compose " + entry_name);
    if (!specialization_types.empty()) {
        std::vector<sl::SpecializationArg> args;
        auto*                              layout = composed->getLayout();
        for (const auto& name : specialization_types) {
            auto* type = layout->findTypeByName(name.c_str());
            if (!type) throw std::invalid_argument("Unknown shader specialization type: " + std::string(name));
            args.push_back(sl::SpecializationArg::fromType(type));
        }
        SlangPtr<sl::IComponentType> specialized;
        status = composed->specialize(args.data(), args.size(), specialized.writeRef(), diagnostics.writeRef());
        m_State->Check(status, diagnostics, "specialize " + entry_name);
        composed = specialized;
    }
    SlangPtr<sl::IComponentType> linked;
    status = composed->link(linked.writeRef(), diagnostics.writeRef());
    m_State->Check(status, diagnostics, "link " + entry_name);
    SlangPtr<sl::IBlob> code;
    status = linked->getEntryPointCode(0, 0, code.writeRef(), diagnostics.writeRef());
    m_State->Check(status, diagnostics, "compile " + entry_name);
    if (!code || !code->getBufferSize()) throw std::runtime_error("Slang produced empty code for " + entry_name);
    CompiledShader result{
        .bytecode = core::Buffer(code->getBufferSize(), static_cast<const std::byte*>(code->getBufferPointer())),
        .entry    = std::pmr::string(entry),
    };
    if (stage == ShaderType::Vertex) {
        auto* reflection = linked->getLayout()->getEntryPointByIndex(0);
        for (unsigned i = 0; i < reflection->getParameterCount(); ++i)
            ReflectVertexInput(reflection->getParameterByIndex(i), 0, result.vertex_layout);
    }
    return result;
}
}  // namespace hitagi::gfx
