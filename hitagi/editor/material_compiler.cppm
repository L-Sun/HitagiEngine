export module editor:material_compiler;
import engine;
import :material_graph;

export namespace hitagi {

struct MaterialGraphCompileOptions {
    std::pmr::string name     = "GeneratedMaterial";
    std::pmr::string terminal = "surface";
};

struct MaterialGraphCompileResult {
    MaterialGraphValidationResult validation;
    std::pmr::string              slang;
    asset::MaterialPass           pass;

    bool     Valid() const noexcept { return validation.Valid(); }
    explicit operator bool() const noexcept { return Valid(); }
};

// Lowers the graph into a Slang module defining `Material : IMaterial` and
// specializes every stage of `pass_template` with it. Shading semantics belong
// to the template's shell: inputs of the Surface-producing node assign Surface
// fields of the same name, and geometry nodes read SurfaceInput fields.
// `defaults` supply the typed binding default of every parameter node.
auto CompileMaterialGraph(
    const MaterialGraph&                      graph,
    const asset::MaterialPassTemplate&        pass_template,
    std::span<const asset::MaterialParameter> defaults,
    const MaterialGraphCompileOptions&        options = {}) -> MaterialGraphCompileResult;

}  // namespace hitagi

namespace hitagi::material_compiler {

auto SlangType(ShaderValueType type) noexcept -> std::string_view {
    switch (type) {
        case ShaderValueType::Float:
            return "float";
        case ShaderValueType::UInt32:
            return "uint";
        case ShaderValueType::Float2:
            return "float2";
        case ShaderValueType::Float3:
            return "float3";
        case ShaderValueType::Float4:
        case ShaderValueType::Color:
            return "float4";
        case ShaderValueType::Texture2D:
            return "hitagi::Texture";
        case ShaderValueType::Sampler:
            return "SamplerState";
        case ShaderValueType::Surface:
            return "Surface";
    }
    return "float";
}

auto DefaultLiteral(ShaderValueType type) noexcept -> std::string_view {
    switch (type) {
        case ShaderValueType::Float:
            return "0.0f";
        case ShaderValueType::UInt32:
            return "0u";
        case ShaderValueType::Float2:
            return "float2(0.0f, 0.0f)";
        case ShaderValueType::Float3:
            return "float3(0.0f, 0.0f, 0.0f)";
        case ShaderValueType::Float4:
        case ShaderValueType::Color:
            return "float4(1.0f, 1.0f, 1.0f, 1.0f)";
        case ShaderValueType::Texture2D:
        case ShaderValueType::Sampler:
        case ShaderValueType::Surface:
            return "";
    }
    return "";
}

auto ConstantLiteral(const MaterialGraphConstant& value) -> std::string {
    const auto scalar = [](float component) {
        auto text = std::format("{}", component);
        if (!text.contains('.') && !text.contains('e')) text += ".0";
        return text + "f";
    };
    return std::visit(
        utils::Overloaded{
            [&](float component) { return scalar(component); },
            [](std::uint32_t component) { return std::format("{}u", component); },
            [&]<unsigned D>(const math::Vector<float, D>& vector) {
                auto text = std::format("float{}(", D);
                for (unsigned i = 0; i < D; ++i) text += std::format("{}{}", i == 0 ? "" : ", ", scalar(vector[i]));
                return text + ")";
            },
        },
        value);
}

auto MatchesType(const asset::MaterialParameterValue& value, ShaderValueType type) noexcept -> bool {
    switch (type) {
        case ShaderValueType::Float:
            return std::holds_alternative<float>(value);
        case ShaderValueType::UInt32:
            return std::holds_alternative<std::uint32_t>(value);
        case ShaderValueType::Float2:
            return std::holds_alternative<math::vec2f>(value);
        case ShaderValueType::Float3:
            return std::holds_alternative<math::vec3f>(value);
        case ShaderValueType::Float4:
            return std::holds_alternative<math::vec4f>(value);
        case ShaderValueType::Color:
            return std::holds_alternative<math::Color>(value);
        case ShaderValueType::Texture2D:
            return std::holds_alternative<std::shared_ptr<asset::Texture>>(value);
        case ShaderValueType::Sampler:
        case ShaderValueType::Surface:
            return false;
    }
    return false;
}

auto ToIdentifier(std::string_view value) -> std::string {
    std::string result;
    for (const char ch : value) result.push_back(std::isalnum(static_cast<unsigned char>(ch)) ? ch : '_');
    if (result.empty() || std::isdigit(static_cast<unsigned char>(result.front()))) result.insert(result.begin(), '_');
    return result;
}

auto IsIdentifier(std::string_view value) -> bool { return ToIdentifier(value) == value; }

auto SwizzleType(std::string_view mask) noexcept -> std::optional<ShaderValueType> {
    const auto within = [mask](std::string_view channels) { return std::ranges::all_of(mask, [&](char ch) { return channels.contains(ch); }); };
    if (!within("rgba") && !within("xyzw")) return std::nullopt;
    switch (mask.size()) {
        case 1:
            return ShaderValueType::Float;
        case 2:
            return ShaderValueType::Float2;
        case 3:
            return ShaderValueType::Float3;
        case 4:
            return ShaderValueType::Float4;
        default:
            return std::nullopt;
    }
}

}  // namespace hitagi::material_compiler

namespace hitagi {

auto CompileMaterialGraph(
    const MaterialGraph&                      graph,
    const asset::MaterialPassTemplate&        pass_template,
    std::span<const asset::MaterialParameter> defaults,
    const MaterialGraphCompileOptions&        options) -> MaterialGraphCompileResult {
    using namespace material_compiler;
    if (pass_template.shell.name.empty()) throw std::invalid_argument("Material pass template shell requires an import name");

    MaterialGraphCompileResult result{.validation = graph.Validate()};
    const auto fail = [&](MaterialGraphValidationErrorCode code, std::string message) {
        result.validation.errors.push_back({code, std::pmr::string(message)});
    };
    if (!result.validation) return result;

    const auto terminal = std::ranges::find_if(graph.GetTerminals(), [&](const auto& candidate) { return candidate.name == options.terminal; });
    if (terminal == graph.GetTerminals().end() || terminal->type != ShaderValueType::Surface) {
        fail(MaterialGraphValidationErrorCode::MissingTerminal, std::format("material graph has no Surface terminal '{}'", options.terminal));
        return result;
    }

    // Slang expression of each defined output socket.
    std::pmr::unordered_map<std::pmr::string, std::pmr::string> expressions;
    const auto key = [](MaterialGraphNodeId node, std::string_view socket) { return std::pmr::string(std::format("{}.{}", node, socket)); };
    const auto define = [&](const MaterialGraphNode& node, const MaterialGraphSocket& output, std::string_view expression) {
        expressions.insert_or_assign(key(node.id, output.name), std::pmr::string(expression));
    };
    const auto input = [&](const MaterialGraphNode& node, std::string_view socket) -> std::string {
        const auto edge = std::ranges::find_if(graph.GetEdges(), [&](const auto& candidate) { return candidate.to_node == node.id && candidate.to_input == socket; });
        if (edge == graph.GetEdges().end()) return {};
        const auto iter = expressions.find(key(edge->from_node, edge->from_output));
        return iter == expressions.end() ? std::string{} : std::string(iter->second);
    };
    const auto input_or = [&](const MaterialGraphNode& node, std::string_view socket, std::string_view fallback) {
        auto expression = input(node, socket);
        return expression.empty() ? std::string(fallback) : expression;
    };

    std::string body;
    std::string surface_body;
    for (const auto node_id : graph.CollectTerminalDependencies(options.terminal)) {
        const MaterialGraphNode& node = *graph.FindNode(node_id);
        const std::string_view   kind = node.kind;
        if (node.outputs.empty()) {
            fail(MaterialGraphValidationErrorCode::UnsupportedNode, std::format("node '{}' has no outputs", node.name));
            continue;
        }
        const MaterialGraphSocket& output = node.outputs.front();

        if (output.type == ShaderValueType::Surface) {
            for (const auto& socket : node.inputs) {
                const auto expression = input(node, socket.name);
                if (expression.empty()) continue;
                if (!IsIdentifier(socket.name)) {
                    fail(MaterialGraphValidationErrorCode::MissingSocket, std::format("Surface input '{}' is not a field name", socket.name));
                    continue;
                }
                surface_body += std::format("        surface.{} = {};\n", socket.name, expression);
            }
        } else if (kind == "parameter") {
            const auto parameter = std::ranges::find(defaults, node.name, &asset::MaterialParameter::name);
            if (!IsIdentifier(node.name) || parameter == defaults.end() || !MatchesType(parameter->value, output.type)) {
                fail(MaterialGraphValidationErrorCode::TypeMismatch, std::format("Parameter '{}' requires a matching typed contract default", node.name));
                continue;
            }
            if (std::ranges::none_of(result.pass.bindings, [&](const auto& binding) { return binding.name == node.name; }))
                result.pass.bindings.emplace_back(node.name, parameter->value);
            define(node, output, std::format("material_data.{}", node.name));
        } else if (kind == "constant") {
            if (!node.constant) {
                fail(MaterialGraphValidationErrorCode::UnsupportedNode, std::format("constant node '{}' has no value", node.name));
                continue;
            }
            define(node, output, ConstantLiteral(*node.constant));
        } else if (kind == "uv") {
            define(node, output, "input.uv");
        } else if (kind == "normal") {
            define(node, output, "input.normal");
        } else if (kind == "vertex_color") {
            define(node, output, "input.color");
        } else if (kind == "texture_sample") {
            const auto texture = input(node, "texture");
            if (texture.empty()) {
                fail(MaterialGraphValidationErrorCode::MissingSocket, std::format("texture_sample node '{}' has no texture input", node.name));
                continue;
            }
            const auto type  = SlangType(output.type);
            const auto local = std::format("{}_{}", ToIdentifier(node.name), ToIdentifier(output.name));
            // A null texture binding means "no texture", never a descriptor to sample.
            body += std::format("        {} {} = {};\n", type, local, input_or(node, "fallback", DefaultLiteral(output.type)));
            body += std::format("        if (hitagi::valid({0}.handle)) {1} = {0}.sample<{2}>(context.sampler, {3});\n",
                                texture, local, type, input_or(node, "uv", "input.uv"));
            define(node, output, local);
        } else if (kind == "normal_map") {
            const auto sample = input(node, "sample");
            define(node, output, sample.empty() ? "float3(0.0f, 0.0f, 1.0f)" : std::format("normalize({}.xyz * 2.0f - 1.0f)", sample));
        } else if (kind == "mix") {
            const auto fallback = DefaultLiteral(output.type);
            define(node, output, std::format("lerp({}, {}, {})", input_or(node, "a", fallback), input_or(node, "b", fallback), input_or(node, "t", "0.5f")));
        } else if (kind == "swizzle") {
            const auto value = input(node, "in");
            if (value.empty()) {
                fail(MaterialGraphValidationErrorCode::MissingSocket, std::format("swizzle node '{}' has no input", node.name));
                continue;
            }
            // Output socket names are channel masks.
            for (const auto& socket : node.outputs) {
                const auto type = SwizzleType(socket.name);
                if (type != socket.type && !(type == ShaderValueType::Float4 && socket.type == ShaderValueType::Color)) {
                    fail(MaterialGraphValidationErrorCode::TypeMismatch, std::format("swizzle output '{}' does not match its type", socket.name));
                    continue;
                }
                define(node, socket, std::format("({}).{}", value, socket.name));
            }
        } else {
            fail(MaterialGraphValidationErrorCode::UnsupportedNode, std::format("node '{}' has unsupported kind '{}'", node.name, kind));
        }
    }
    if (!result.validation) return result;

    std::string material_data;
    if (!result.pass.bindings.empty()) {
        const auto fields = asset::MaterialDataFields(result.pass.bindings);
        material_data     = gfx::GenerateShaderStruct("MaterialData", fields, fields.back().offset + fields.back().size);
        body.insert(0, "        const MaterialData material_data = context.material_data.load<MaterialData>();\n");
    }
    result.slang = std::format(
        "import bindless;\n"
        "import {};\n\n"
        "{}"
        "struct Material : IMaterial {{\n"
        "    static void evaluate(SurfaceInput input, MaterialContext context, inout Surface surface) {{\n"
        "{}{}"
        "    }}\n"
        "}};\n",
        pass_template.shell.name, material_data, body, surface_body);

    const gfx::ShaderModuleDesc                      material_module{.name = "material", .source_code = result.slang};
    std::pmr::vector<std::shared_ptr<asset::Shader>> shaders;
    for (const auto& stage : pass_template.stages) {
        shaders.emplace_back(std::make_shared<asset::Shader>(gfx::ShaderDesc{
            .name                 = std::pmr::string(std::format("{}.{}", options.name, stage.entry)),
            .type                 = stage.type,
            .entry                = stage.entry,
            .program              = {.modules = {material_module, pass_template.shell}},
            .specialization_types = {"Material"},
        }));
    }
    auto pipeline_desc = pass_template.pipeline;
    pipeline_desc.name = std::format("{}Pipeline", options.name);

    result.pass.pass_contract = pass_template.contract;
    result.pass.pipeline      = std::make_shared<asset::RenderPipeline>(std::move(pipeline_desc), std::move(shaders));
    return result;
}

}  // namespace hitagi
