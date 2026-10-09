export module editor:material_compiler;
import engine;

export namespace hitagi {

enum struct ShaderValueType : std::uint8_t {
    Float,
    UInt32,
    Float2,
    Float3,
    Float4,
    Color,
    Texture2D,
    Sampler,
    Surface,
};

using MaterialGraphNodeId = std::uint32_t;

struct ShaderIRMetadata {
    std::pmr::string source_node_path;
    std::pmr::string source_shader_id;
    std::pmr::string source_socket_name;
};

struct MaterialGraphSocket {
    MaterialGraphSocket(std::string_view name = {}, ShaderValueType type = ShaderValueType::Float, ShaderIRMetadata metadata = {});

    std::pmr::string name;
    ShaderValueType  type = ShaderValueType::Float;
    ShaderIRMetadata metadata;
};

struct MaterialGraphNode {
    MaterialGraphNodeId                   id = 0;
    std::pmr::string                      name;
    std::pmr::string                      kind;
    std::pmr::vector<MaterialGraphSocket> inputs;
    std::pmr::vector<MaterialGraphSocket> outputs;
    ShaderIRMetadata                      metadata;
};

struct MaterialGraphEdge {
    MaterialGraphNodeId from_node = 0;
    std::pmr::string    from_output;
    MaterialGraphNodeId to_node = 0;
    std::pmr::string    to_input;
    ShaderIRMetadata    metadata;
};

struct MaterialGraphTerminal {
    std::pmr::string    name;
    MaterialGraphNodeId node = 0;
    std::pmr::string    output;
    ShaderValueType     type = ShaderValueType::Surface;
    ShaderIRMetadata    metadata;
};

enum struct MaterialGraphValidationErrorCode : std::uint8_t {
    MissingNode,
    MissingSocket,
    TypeMismatch,
    MissingTerminal,
    Cycle,
};

struct MaterialGraphValidationError {
    MaterialGraphValidationErrorCode code = MaterialGraphValidationErrorCode::MissingNode;
    std::pmr::string                 message;
};

struct MaterialGraphValidationResult {
    std::pmr::vector<MaterialGraphValidationError> errors;

    bool     Valid() const noexcept { return errors.empty(); }
    explicit operator bool() const noexcept { return Valid(); }
};

class MaterialGraph {
public:
    auto AddNode(
        std::string_view                           name,
        std::string_view                           kind,
        std::initializer_list<MaterialGraphSocket> inputs   = {},
        std::initializer_list<MaterialGraphSocket> outputs  = {},
        ShaderIRMetadata                           metadata = {}) -> MaterialGraphNodeId;
    void AddEdge(
        MaterialGraphNodeId from_node,
        std::string_view    from_output,
        MaterialGraphNodeId to_node,
        std::string_view    to_input,
        ShaderIRMetadata    metadata = {});
    void AddTerminal(
        std::string_view    name,
        MaterialGraphNodeId node,
        std::string_view    output,
        ShaderValueType     type,
        ShaderIRMetadata    metadata = {});

    auto Validate() const -> MaterialGraphValidationResult;
    auto TopologicalSort() const -> std::pmr::vector<MaterialGraphNodeId>;
    auto CollectTerminalDependencies(std::string_view terminal_name) const -> std::pmr::vector<MaterialGraphNodeId>;

    auto FindNode(MaterialGraphNodeId id) const noexcept -> const MaterialGraphNode*;
    auto FindInputSocket(MaterialGraphNodeId id, std::string_view name) const noexcept -> const MaterialGraphSocket*;
    auto FindOutputSocket(MaterialGraphNodeId id, std::string_view name) const noexcept -> const MaterialGraphSocket*;

    const auto& GetNodes() const noexcept { return m_Nodes; }
    const auto& GetEdges() const noexcept { return m_Edges; }
    const auto& GetTerminals() const noexcept { return m_Terminals; }

private:
    auto TopologicalSort(std::span<const MaterialGraphNodeId> node_subset) const -> std::pmr::vector<MaterialGraphNodeId>;

    MaterialGraphNodeId                     m_NextNodeId = 1;
    std::pmr::vector<MaterialGraphNode>     m_Nodes;
    std::pmr::vector<MaterialGraphEdge>     m_Edges;
    std::pmr::vector<MaterialGraphTerminal> m_Terminals;
};

struct MaterialGraphCompileOptions {
    std::pmr::string shader_name          = "GeneratedMaterialPS";
    std::pmr::string entry_point          = "main";
    std::pmr::string surface_terminal     = "surface";
    std::uint64_t    pipeline_state_hash  = 0;
    bool             force_recompile      = false;
};

struct MaterialGraphCompileResult {
    MaterialGraphValidationResult validation;
    std::pmr::string              hlsl;
    asset::MaterialPass           pass;
    gfx::ShaderDesc               shader;
    gfx::RenderPipelineDesc       pipeline_desc;
    std::uint64_t                 graph_hash  = 0;
    std::uint64_t                 source_hash = 0;
    std::uint64_t                 cache_key   = 0;

    bool     Valid() const noexcept { return validation.Valid() && !hlsl.empty() && cache_key != 0; }
    explicit operator bool() const noexcept { return Valid(); }
};

class MaterialGraphCompiler {
public:
    auto Compile(const MaterialGraph& graph, MaterialGraphCompileOptions options = {}) const -> MaterialGraphCompileResult;
};

class MaterialShaderCache {
public:
    auto CompileOrGet(const MaterialGraph& graph, const MaterialGraphCompileOptions& options = {}) -> MaterialGraphCompileResult;
    auto Recompile(const MaterialGraph& graph, MaterialGraphCompileOptions options = {}) -> MaterialGraphCompileResult;
    bool Invalidate(std::uint64_t cache_key);
    void Clear() noexcept;
    void SetHotReloadEnabled(bool enabled) noexcept { m_HotReloadEnabled = enabled; }
    bool HotReloadEnabled() const noexcept { return m_HotReloadEnabled; }

private:
    MaterialGraphCompiler                                              m_Compiler;
    std::pmr::unordered_map<std::uint64_t, MaterialGraphCompileResult> m_Cache;
    bool                                                               m_HotReloadEnabled = false;
};

}  // namespace hitagi

namespace hitagi {
namespace {

constexpr std::uint64_t kFnvOffset                    = 14695981039346656037ull;
constexpr std::uint64_t kFnvPrime                     = 1099511628211ull;

void hash_bytes(std::uint64_t& hash, const void* data, std::size_t size) noexcept {
    const auto* bytes = static_cast<const std::byte*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        hash = (hash ^ static_cast<std::uint64_t>(bytes[i])) * kFnvPrime;
    }
}

template <typename T>
void hash_value(std::uint64_t& hash, const T& value) noexcept {
    hash_bytes(hash, std::addressof(value), sizeof(T));
}

void hash_string_value(std::uint64_t& hash, std::string_view value) noexcept {
    hash_bytes(hash, value.data(), value.size());
}

auto hash_string(std::string_view value) noexcept -> std::uint64_t {
    std::uint64_t hash = kFnvOffset;
    hash_string_value(hash, value);
    return hash;
}

auto shader_value_hlsl_type(ShaderValueType type) noexcept -> std::string_view {
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
            return "Texture2D";
        case ShaderValueType::Sampler:
            return "SamplerState";
        case ShaderValueType::Surface:
            return "Surface";
    }
    return "float";
}

auto shader_value_is_material_data(ShaderValueType type) noexcept -> bool {
    switch (type) {
        case ShaderValueType::Float:
        case ShaderValueType::UInt32:
        case ShaderValueType::Float2:
        case ShaderValueType::Float3:
        case ShaderValueType::Float4:
        case ShaderValueType::Color:
        case ShaderValueType::Texture2D:
            return true;
        case ShaderValueType::Sampler:
        case ShaderValueType::Surface:
            return false;
    }
    return false;
}

auto material_data_field_type(ShaderValueType type) noexcept -> std::string_view {
    return type == ShaderValueType::Texture2D ? "hitagi::Texture" : shader_value_hlsl_type(type);
}

auto default_literal(ShaderValueType type) noexcept -> std::string_view {
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
        case ShaderValueType::Surface:
            return "MakeDefaultSurface()";
        case ShaderValueType::Texture2D:
        case ShaderValueType::Sampler:
            return "";
    }
    return "0.0f";
}

auto sanitize_hlsl_identifier(std::string_view value) -> std::pmr::string {
    std::pmr::string result;
    result.reserve(value.size() + 1);
    for (const auto ch : value) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_')
            result.push_back(ch);
        else
            result.push_back('_');
    }
    if (result.empty() || (result.front() >= '0' && result.front() <= '9')) result.insert(result.begin(), '_');
    return result;
}

auto socket_key(MaterialGraphNodeId node, std::string_view socket) -> std::pmr::string {
    return std::pmr::string(std::format("{}.{}", node, socket));
}

auto find_connected_expression(
    const MaterialGraph&                                               graph,
    MaterialGraphNodeId                                                to_node,
    std::string_view                                                   to_input,
    const std::pmr::unordered_map<std::pmr::string, std::pmr::string>& expressions) -> std::pmr::string {
    const auto edge = std::ranges::find_if(graph.GetEdges(), [&](const auto& candidate) {
        return candidate.to_node == to_node && candidate.to_input == to_input;
    });
    if (edge == graph.GetEdges().end()) return {};
    const auto iter = expressions.find(socket_key(edge->from_node, edge->from_output));
    return iter == expressions.end() ? std::pmr::string{} : iter->second;
}

struct MaterialDataField {
    std::pmr::string name;
    ShaderValueType  type = ShaderValueType::Float;
};

void add_material_data_field(
    asset::MaterialPass&                 pass,
    std::pmr::vector<MaterialDataField>& material_data_fields,
    std::string_view                     name,
    ShaderValueType                      type) {
    if (std::ranges::any_of(material_data_fields, [name](const auto& field) { return field.name == name; })) return;
    if (!shader_value_is_material_data(type)) return;

    if (std::ranges::none_of(pass.bindings, [name](const auto& binding) { return binding == name; })) {
        pass.bindings.emplace_back(name);
    }
    material_data_fields.emplace_back(MaterialDataField{
        .name = std::pmr::string(name),
        .type = type,
    });
}

auto hash_material_graph(const MaterialGraph& graph) noexcept -> std::uint64_t {
    std::uint64_t hash = kFnvOffset;
    for (const auto& node : graph.GetNodes()) {
        hash_value(hash, node.id);
        hash_string_value(hash, node.name);
        hash_string_value(hash, node.kind);
        for (const auto& input : node.inputs) {
            hash_string_value(hash, input.name);
            hash_value(hash, input.type);
        }
        for (const auto& output : node.outputs) {
            hash_string_value(hash, output.name);
            hash_value(hash, output.type);
        }
    }
    for (const auto& edge : graph.GetEdges()) {
        hash_value(hash, edge.from_node);
        hash_string_value(hash, edge.from_output);
        hash_value(hash, edge.to_node);
        hash_string_value(hash, edge.to_input);
    }
    for (const auto& terminal : graph.GetTerminals()) {
        hash_string_value(hash, terminal.name);
        hash_value(hash, terminal.node);
        hash_string_value(hash, terminal.output);
        hash_value(hash, terminal.type);
    }
    return hash;
}

auto hash_material_pass_layout(const asset::MaterialPass& pass) noexcept -> std::uint64_t {
    std::uint64_t hash = kFnvOffset;
    hash_string_value(hash, pass.pass_contract);
    for (const auto& binding : pass.bindings) hash_string_value(hash, binding);
    if (!pass.pipeline) return hash;

    const auto& pipeline = pass.pipeline->GetDesc();
    hash_string_value(hash, pipeline.name);
    hash_value(hash, pipeline.assembly_state.primitive);
    hash_value(hash, pipeline.rasterization_state.cull_mode);
    hash_value(hash, pipeline.rasterization_state.front_counter_clockwise);
    hash_value(hash, pipeline.depth_stencil_state.depth_test_enable);
    hash_value(hash, pipeline.depth_stencil_state.depth_write_enable);
    hash_value(hash, pipeline.depth_stencil_state.depth_compare_op);
    hash_value(hash, pipeline.render_format);
    hash_value(hash, pipeline.depth_stencil_format);

    for (const auto& shader_asset : pass.pipeline->GetShaders()) {
        if (!shader_asset) continue;
        const auto& shader = shader_asset->GetDesc();
        hash_string_value(hash, shader.name);
        hash_value(hash, shader.type);
        hash_string_value(hash, shader.entry);
        hash_string_value(hash, shader.path.generic_string());
    }
    return hash;
}

auto generated_shader_path(std::string_view shader_name) -> std::filesystem::path {
    auto file_name = sanitize_hlsl_identifier(shader_name);
    file_name += ".generated.hlsl";
    return std::filesystem::path{"assets/generated"} / file_name;
}

}  // namespace

MaterialGraphSocket::MaterialGraphSocket(std::string_view name, ShaderValueType type, ShaderIRMetadata metadata)
    : name(name), type(type), metadata(std::move(metadata)) {}

auto MaterialGraph::AddNode(
    std::string_view                           name,
    std::string_view                           kind,
    std::initializer_list<MaterialGraphSocket> inputs,
    std::initializer_list<MaterialGraphSocket> outputs,
    ShaderIRMetadata                           metadata) -> MaterialGraphNodeId {
    const auto        id = m_NextNodeId++;
    MaterialGraphNode node{
        .id       = id,
        .name     = std::pmr::string(name),
        .kind     = std::pmr::string(kind),
        .metadata = std::move(metadata),
    };
    node.inputs.assign(inputs.begin(), inputs.end());
    node.outputs.assign(outputs.begin(), outputs.end());
    m_Nodes.emplace_back(std::move(node));
    return id;
}

void MaterialGraph::AddEdge(
    MaterialGraphNodeId from_node,
    std::string_view    from_output,
    MaterialGraphNodeId to_node,
    std::string_view    to_input,
    ShaderIRMetadata    metadata) {
    m_Edges.emplace_back(MaterialGraphEdge{
        .from_node   = from_node,
        .from_output = std::pmr::string(from_output),
        .to_node     = to_node,
        .to_input    = std::pmr::string(to_input),
        .metadata    = std::move(metadata),
    });
}

void MaterialGraph::AddTerminal(
    std::string_view    name,
    MaterialGraphNodeId node,
    std::string_view    output,
    ShaderValueType     type,
    ShaderIRMetadata    metadata) {
    m_Terminals.emplace_back(MaterialGraphTerminal{
        .name     = std::pmr::string(name),
        .node     = node,
        .output   = std::pmr::string(output),
        .type     = type,
        .metadata = std::move(metadata),
    });
}

auto MaterialGraph::FindNode(MaterialGraphNodeId id) const noexcept -> const MaterialGraphNode* {
    const auto iter = std::ranges::find_if(m_Nodes, [id](const auto& node) { return node.id == id; });
    return iter == m_Nodes.end() ? nullptr : std::addressof(*iter);
}

auto MaterialGraph::FindInputSocket(MaterialGraphNodeId id, std::string_view name) const noexcept -> const MaterialGraphSocket* {
    const auto* node = FindNode(id);
    if (!node) return nullptr;
    const auto iter = std::ranges::find_if(node->inputs, [name](const auto& socket) { return socket.name == name; });
    return iter == node->inputs.end() ? nullptr : std::addressof(*iter);
}

auto MaterialGraph::FindOutputSocket(MaterialGraphNodeId id, std::string_view name) const noexcept -> const MaterialGraphSocket* {
    const auto* node = FindNode(id);
    if (!node) return nullptr;
    const auto iter = std::ranges::find_if(node->outputs, [name](const auto& socket) { return socket.name == name; });
    return iter == node->outputs.end() ? nullptr : std::addressof(*iter);
}

auto MaterialGraph::Validate() const -> MaterialGraphValidationResult {
    MaterialGraphValidationResult result;
    const auto                    add_error = [&result](MaterialGraphValidationErrorCode code, std::string_view message) {
        result.errors.emplace_back(MaterialGraphValidationError{.code = code, .message = std::pmr::string(message)});
    };

    if (m_Terminals.empty()) add_error(MaterialGraphValidationErrorCode::MissingTerminal, "material graph has no terminals");

    for (const auto& edge : m_Edges) {
        const auto* from_node = FindNode(edge.from_node);
        const auto* to_node   = FindNode(edge.to_node);
        if (!from_node) add_error(MaterialGraphValidationErrorCode::MissingNode, std::format("edge references missing source node {}", edge.from_node));
        if (!to_node) add_error(MaterialGraphValidationErrorCode::MissingNode, std::format("edge references missing destination node {}", edge.to_node));

        const auto* from_socket = from_node ? FindOutputSocket(edge.from_node, edge.from_output) : nullptr;
        const auto* to_socket   = to_node ? FindInputSocket(edge.to_node, edge.to_input) : nullptr;
        if (from_node && !from_socket) add_error(MaterialGraphValidationErrorCode::MissingSocket, std::format("node {} has no output socket '{}'", edge.from_node, std::string_view(edge.from_output)));
        if (to_node && !to_socket) add_error(MaterialGraphValidationErrorCode::MissingSocket, std::format("node {} has no input socket '{}'", edge.to_node, std::string_view(edge.to_input)));
        if (from_socket && to_socket && from_socket->type != to_socket->type) add_error(MaterialGraphValidationErrorCode::TypeMismatch, "material graph edge has incompatible socket types");
    }

    for (const auto& terminal : m_Terminals) {
        const auto* node = FindNode(terminal.node);
        if (!node) {
            add_error(MaterialGraphValidationErrorCode::MissingTerminal, std::format("terminal '{}' references missing node {}", std::string_view(terminal.name), terminal.node));
            continue;
        }
        const auto* socket = FindOutputSocket(terminal.node, terminal.output);
        if (!socket) {
            add_error(MaterialGraphValidationErrorCode::MissingTerminal, std::format("terminal '{}' references missing output socket '{}'", std::string_view(terminal.name), std::string_view(terminal.output)));
            continue;
        }
        if (socket->type != terminal.type) add_error(MaterialGraphValidationErrorCode::TypeMismatch, "material graph terminal has incompatible socket type");
    }

    if (TopologicalSort().size() != m_Nodes.size()) add_error(MaterialGraphValidationErrorCode::Cycle, "material graph contains a cycle");
    return result;
}

auto MaterialGraph::TopologicalSort() const -> std::pmr::vector<MaterialGraphNodeId> {
    std::pmr::vector<MaterialGraphNodeId> node_ids;
    node_ids.reserve(m_Nodes.size());
    for (const auto& node : m_Nodes) node_ids.emplace_back(node.id);
    return TopologicalSort(node_ids);
}

auto MaterialGraph::TopologicalSort(std::span<const MaterialGraphNodeId> node_subset) const -> std::pmr::vector<MaterialGraphNodeId> {
    std::pmr::vector<MaterialGraphNodeId> result;
    result.reserve(node_subset.size());
    const auto contains = [node_subset](MaterialGraphNodeId id) { return std::ranges::find(node_subset, id) != node_subset.end(); };
    const auto emitted  = [&result](MaterialGraphNodeId id) { return std::ranges::find(result, id) != result.end(); };

    while (result.size() < node_subset.size()) {
        bool progressed = false;
        for (const auto& node : m_Nodes) {
            if (!contains(node.id) || emitted(node.id)) continue;
            const auto blocked = std::ranges::any_of(m_Edges, [&](const auto& edge) {
                return edge.to_node == node.id && contains(edge.from_node) && !emitted(edge.from_node);
            });
            if (blocked) continue;
            result.emplace_back(node.id);
            progressed = true;
        }
        if (!progressed) break;
    }
    return result;
}

auto MaterialGraph::CollectTerminalDependencies(std::string_view terminal_name) const -> std::pmr::vector<MaterialGraphNodeId> {
    const auto terminal = std::ranges::find_if(m_Terminals, [terminal_name](const auto& candidate) { return candidate.name == terminal_name; });
    if (terminal == m_Terminals.end()) return {};

    std::pmr::vector<MaterialGraphNodeId>          dependencies;
    const std::function<void(MaterialGraphNodeId)> collect = [&](MaterialGraphNodeId node_id) {
        if (std::ranges::find(dependencies, node_id) != dependencies.end()) return;
        dependencies.emplace_back(node_id);
        for (const auto& edge : m_Edges) {
            if (edge.to_node == node_id) collect(edge.from_node);
        }
    };
    collect(terminal->node);
    return TopologicalSort(dependencies);
}

auto MaterialGraphCompiler::Compile(const MaterialGraph& graph, MaterialGraphCompileOptions options) const -> MaterialGraphCompileResult {
    MaterialGraphCompileResult result{.validation = graph.Validate()};
    result.graph_hash = hash_material_graph(graph);
    if (!result.validation) return result;

    auto dependencies = graph.CollectTerminalDependencies(options.surface_terminal);
    if (dependencies.empty() && !graph.GetTerminals().empty()) dependencies = graph.CollectTerminalDependencies(graph.GetTerminals().front().name);
    if (dependencies.empty()) return result;

    std::pmr::unordered_map<std::pmr::string, std::pmr::string> expressions;
    std::pmr::string                                            declarations;
    std::pmr::string                                            body;
    std::pmr::vector<MaterialGraphNodeId>                       surface_nodes;
    std::pmr::vector<MaterialDataField>                         material_data_fields;
    bool                                                        uses_vertex_color   = false;

    for (const auto node_id : dependencies) {
        const auto* node = graph.FindNode(node_id);
        if (!node || node->outputs.empty()) continue;

        const auto output_type = node->outputs.front().type;
        const auto name        = sanitize_hlsl_identifier(node->name);
        const auto kind        = std::string_view(node->kind);

        if (kind == "parameter") {
            if (output_type == ShaderValueType::Texture2D) {
                add_material_data_field(result.pass, material_data_fields, node->name, output_type);
                expressions.emplace(socket_key(node->id, node->outputs.front().name), std::pmr::string(std::format("material_data.{}", name)));
            } else if (shader_value_is_material_data(output_type)) {
                add_material_data_field(result.pass, material_data_fields, node->name, output_type);
                expressions.emplace(socket_key(node->id, node->outputs.front().name), std::pmr::string(std::format("material_data.{}", name)));
            }
            continue;
        }
        if (kind == "texture" || kind == "texture_2d") {
            add_material_data_field(result.pass, material_data_fields, node->name, ShaderValueType::Texture2D);
            expressions.emplace(socket_key(node->id, node->outputs.front().name), std::pmr::string(std::format("material_data.{}", name)));
            continue;
        }
        if (kind == "constant") {
            expressions.emplace(socket_key(node->id, node->outputs.front().name), std::pmr::string(default_literal(output_type)));
            continue;
        }
        if (kind == "uv" || kind == "uv0" || kind == "primvar") {
            expressions.emplace(socket_key(node->id, node->outputs.front().name), "input.uv");
            continue;
        }
        if (kind == "vertex_color" || kind == "color" || kind == "color0") {
            uses_vertex_color = true;
            expressions.emplace(socket_key(node->id, node->outputs.front().name), "input.color");
            continue;
        }
        if (kind == "normal" || kind == "normal_vector") {
            expressions.emplace(socket_key(node->id, node->outputs.front().name), "normalize(input.normal_in_view)");
            continue;
        }
        if (kind == "texture_sample") {
            const auto texture_expr = find_connected_expression(graph, node->id, "texture", expressions);
            if (texture_expr.empty()) {
                result.validation.errors.emplace_back(MaterialGraphValidationError{
                    .code    = MaterialGraphValidationErrorCode::MissingSocket,
                    .message = std::pmr::string(std::format("texture_sample node '{}' has no texture expression", node->name)),
                });
                return result;
            }
            auto uv_expr = find_connected_expression(graph, node->id, "uv", expressions);
            if (uv_expr.empty()) uv_expr = "input.uv";

            const auto var_name    = std::pmr::string(std::format("{}_{}", name, node->outputs.front().name));
            const auto sample_expr = std::pmr::string(std::format("{}.sample<{}>(sampler, {})", texture_expr, shader_value_hlsl_type(output_type), uv_expr));
            body += std::format("    const {} {} = {};\n", shader_value_hlsl_type(output_type), var_name, sample_expr);
            expressions.emplace(socket_key(node->id, node->outputs.front().name), var_name);
            continue;
        }
        if (kind == "normal_map") {
            const auto sample_expr = find_connected_expression(graph, node->id, "sample", expressions);
            const auto var_name    = std::pmr::string(std::format("{}_{}", name, node->outputs.front().name));
            const auto expr        = sample_expr.empty() ? std::pmr::string("float4(0.5f, 0.5f, 1.0f, 1.0f)") : sample_expr;
            body += std::format("    const float3 {} = normalize({}.xyz * 2.0f - 1.0f);\n", var_name, expr);
            expressions.emplace(socket_key(node->id, node->outputs.front().name), var_name);
            continue;
        }
        if (kind == "mix" || kind == "lerp") {
            auto a = find_connected_expression(graph, node->id, "a", expressions);
            auto b = find_connected_expression(graph, node->id, "b", expressions);
            auto t = find_connected_expression(graph, node->id, "t", expressions);
            if (a.empty()) a = std::pmr::string(default_literal(output_type));
            if (b.empty()) b = std::pmr::string(default_literal(output_type));
            if (t.empty()) t = "0.5f";
            expressions.emplace(socket_key(node->id, node->outputs.front().name), std::pmr::string(std::format("lerp({}, {}, {})", a, b, t)));
            continue;
        }
        if (output_type == ShaderValueType::Surface) {
            surface_nodes.emplace_back(node->id);
            expressions.emplace(socket_key(node->id, node->outputs.front().name), "surface");
        }
    }

    declarations += "#include \"bindless.hlsl\"\n\n";
    declarations += "struct BindlessInfo {\n";
    declarations += "    hitagi::SimpleBuffer frame_constant;\n";
    declarations += "    hitagi::SimpleBuffer instance_constant;\n";
    declarations += "    uint instance_index;\n    uint instance_stride;\n";
    declarations += "    hitagi::SimpleBuffer material_data;\n";
    declarations += "    hitagi::Sampler      sampler;\n";
    declarations += "};\n\n";
    declarations += "struct FrameConstant {\n";
    declarations += "    float4 camera_pos;\n";
    declarations += "    matrix view;\n";
    declarations += "    matrix projection;\n";
    declarations += "    matrix proj_view;\n";
    declarations += "    matrix inv_view;\n";
    declarations += "    matrix inv_projection;\n";
    declarations += "    matrix inv_proj_view;\n";
    declarations += "    float4 light_position;\n";
    declarations += "    float4 light_pos_in_view;\n";
    declarations += "    float3 light_color;\n";
    declarations += "    float  light_intensity;\n";
    declarations += "};\n\n";
    declarations += "struct InstanceConstant {\n";
    declarations += "    matrix model;\n";
    declarations += "};\n\n";
    declarations += "struct MaterialData {\n";
    for (const auto& field : material_data_fields) {
        declarations += std::format("    {} {};\n", material_data_field_type(field.type), sanitize_hlsl_identifier(field.name));
    }
    declarations += "};\n\n";
    declarations += "struct VSInput {\n";
    declarations += "    float3 position : POSITION;\n";
    declarations += "    float3 normal : NORMAL;\n";
    declarations += "    float2 uv : TEXCOORD;\n";
    if (uses_vertex_color) declarations += "    float4 color : COLOR;\n";
    declarations += "};\n\n";
    declarations += "struct PSInput {\n";
    declarations += "    float4 position : SV_POSITION;\n";
    declarations += "    float3 pos_in_view : POSITION;\n";
    declarations += "    float3 normal_in_view : NORMAL;\n";
    declarations += "    float2 uv : TEXCOORD0;\n";
    if (uses_vertex_color) declarations += "    float4 color : COLOR0;\n";
    declarations += "};\n\n";
    declarations += "struct Surface {\n";
    declarations += "    float4 base_color;\n";
    declarations += "    float metallic;\n";
    declarations += "    float roughness;\n";
    declarations += "    float occlusion;\n";
    declarations += "    float opacity;\n";
    declarations += "    uint  alpha_cutout;\n";
    declarations += "    float alpha_cutoff;\n";
    declarations += "    float3 normal;\n";
    declarations += "    float3 emissive;\n";
    declarations += "};\n\n";
    declarations += "static const float kPi = 3.14159265359f;\n\n";
    declarations += "Surface MakeDefaultSurface() {\n";
    declarations += "    Surface surface;\n";
    declarations += "    surface.base_color = float4(1.0f, 1.0f, 1.0f, 1.0f);\n";
    declarations += "    surface.metallic = 0.0f;\n";
    declarations += "    surface.roughness = 0.5f;\n";
    declarations += "    surface.occlusion = 1.0f;\n";
    declarations += "    surface.opacity = 1.0f;\n";
    declarations += "    surface.alpha_cutout = 0;\n";
    declarations += "    surface.alpha_cutoff = 0.5f;\n";
    declarations += "    surface.normal = float3(0.0f, 0.0f, 1.0f);\n";
    declarations += "    surface.emissive = float3(0.0f, 0.0f, 0.0f);\n";
    declarations += "    return surface;\n";
    declarations += "}\n\n";
    declarations += "float3 SRGBToLinear(float3 color) { return pow(saturate(color), 2.2f); }\n\n";
    declarations += "float3 BuildNormalFromMap(float3 normal_in_view, float3 pos_in_view, float2 uv, float3 normal_sample) {\n";
    declarations += "    const float3 n = normalize(normal_in_view);\n";
    declarations += "    const float3 q1 = ddx(pos_in_view);\n";
    declarations += "    const float3 q2 = ddy(pos_in_view);\n";
    declarations += "    const float2 st1 = ddx(uv);\n";
    declarations += "    const float2 st2 = ddy(uv);\n";
    declarations += "    const float3 q2_perp = cross(q2, n);\n";
    declarations += "    const float3 q1_perp = cross(n, q1);\n";
    declarations += "    const float3 t = q2_perp * st1.x + q1_perp * st2.x;\n";
    declarations += "    const float3 b = q2_perp * st1.y + q1_perp * st2.y;\n";
    declarations += "    const float inv_len = rsqrt(max(dot(t, t), dot(b, b)) + 1e-6f);\n";
    declarations += "    const float3x3 tbn = float3x3(t * inv_len, b * inv_len, n);\n";
    declarations += "    return normalize(mul(normal_sample * 2.0f - 1.0f, tbn));\n";
    declarations += "}\n\n";
    declarations += "float DistributionGGX(float n_dot_h, float roughness) {\n";
    declarations += "    const float a = roughness * roughness;\n";
    declarations += "    const float a2 = a * a;\n";
    declarations += "    const float d = n_dot_h * n_dot_h * (a2 - 1.0f) + 1.0f;\n";
    declarations += "    return a2 / max(kPi * d * d, 1e-5f);\n";
    declarations += "}\n\n";
    declarations += "float GeometrySchlickGGX(float n_dot_v, float roughness) {\n";
    declarations += "    const float r = roughness + 1.0f;\n";
    declarations += "    const float k = (r * r) / 8.0f;\n";
    declarations += "    return n_dot_v / max(n_dot_v * (1.0f - k) + k, 1e-5f);\n";
    declarations += "}\n\n";
    declarations += "float3 FresnelSchlick(float cos_theta, float3 f0) {\n";
    declarations += "    return f0 + (1.0f - f0) * pow(saturate(1.0f - cos_theta), 5.0f);\n";
    declarations += "}\n\n";
    declarations += "PSInput VSMain(VSInput input) {\n";
    declarations += "    BindlessInfo resource = hitagi::load_bindless<BindlessInfo>();\n";
    declarations += "    FrameConstant frame_constant = resource.frame_constant.load<FrameConstant>();\n";
    declarations += "    InstanceConstant instance_constant = resource.instance_constant.load<InstanceConstant>(resource.instance_index * resource.instance_stride);\n";
    declarations += "    const float4 world_pos = mul(instance_constant.model, float4(input.position, 1.0f));\n";
    declarations += "    PSInput output;\n";
    declarations += "    output.position = mul(frame_constant.proj_view, world_pos);\n";
    declarations += "    output.pos_in_view = mul(frame_constant.view, world_pos).xyz;\n";
    declarations += "    output.normal_in_view = normalize(mul(frame_constant.view, mul(instance_constant.model, float4(input.normal, 0.0f))).xyz);\n";
    declarations += "    output.uv = input.uv;\n";
    if (uses_vertex_color) declarations += "    output.color = input.color;\n";
    declarations += "    return output;\n";
    declarations += "}\n\n";

    std::pmr::string surface_body;
    for (const auto node_id : surface_nodes) {
        const auto* node = graph.FindNode(node_id);
        if (!node) continue;
        const auto assign_if_connected = [&](std::string_view input_name, std::string_view field_name) {
            const auto expression = find_connected_expression(graph, node->id, input_name, expressions);
            if (!expression.empty()) surface_body += std::format("    surface.{} = {};\n", field_name, expression);
        };
        assign_if_connected("base_color", "base_color");
        assign_if_connected("color", "base_color");
        assign_if_connected("metallic", "metallic");
        assign_if_connected("roughness", "roughness");
        if (const auto expression = find_connected_expression(graph, node->id, "metallic_roughness", expressions); !expression.empty()) {
            surface_body += std::format("    surface.roughness = clamp({}.g, 0.04f, 1.0f);\n", expression);
            surface_body += std::format("    surface.metallic = saturate({}.b);\n", expression);
        }
        assign_if_connected("occlusion", "occlusion");
        assign_if_connected("opacity", "opacity");
        assign_if_connected("alpha_cutout", "alpha_cutout");
        assign_if_connected("alpha_cutoff", "alpha_cutoff");
        if (const auto expression = find_connected_expression(graph, node->id, "normal", expressions); !expression.empty()) {
            surface_body += std::format("    surface.normal = BuildNormalFromMap(input.normal_in_view, input.pos_in_view, input.uv, {});\n", expression);
        }
        if (const auto expression = find_connected_expression(graph, node->id, "emissive", expressions); !expression.empty()) {
            surface_body += std::format("    surface.emissive = {}.rgb;\n", expression);
        }
    }

    result.hlsl = std::format(
        "{}float4 {}(PSInput input) : SV_TARGET {{\n"
        "    const BindlessInfo resource = hitagi::load_bindless<BindlessInfo>();\n"
        "    const FrameConstant frame_constant = resource.frame_constant.load<FrameConstant>();\n"
        "    const MaterialData material_data = resource.material_data.load<MaterialData>();\n"
        "    const SamplerState sampler = resource.sampler.load();\n"
        "    Surface surface = MakeDefaultSurface();\n"
        "{}"
        "{}"
        "    surface.base_color.rgb = SRGBToLinear(surface.base_color.rgb);\n"
        "    const float alpha = saturate(surface.base_color.a * surface.opacity);\n"
        "    if (surface.alpha_cutout != 0 && alpha < surface.alpha_cutoff) discard;\n"
        "    const float3 light_vec = frame_constant.light_pos_in_view.xyz - input.pos_in_view;\n"
        "    const float distance = max(length(light_vec), 1e-3f);\n"
        "    const float3 l = light_vec / distance;\n"
        "    const float3 v = normalize(-input.pos_in_view);\n"
        "    const float3 h = normalize(v + l);\n"
        "    const float3 n = normalize(surface.normal);\n"
        "    const float n_dot_l = saturate(dot(n, l));\n"
        "    const float n_dot_v = saturate(dot(n, v));\n"
        "    const float n_dot_h = saturate(dot(n, h));\n"
        "    const float h_dot_v = saturate(dot(h, v));\n"
        "    const float3 albedo = saturate(surface.base_color.rgb);\n"
        "    const float metallic = saturate(surface.metallic);\n"
        "    const float roughness = clamp(surface.roughness, 0.04f, 1.0f);\n"
        "    const float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);\n"
        "    const float3 f = FresnelSchlick(h_dot_v, f0);\n"
        "    const float d = DistributionGGX(n_dot_h, roughness);\n"
        "    const float g = GeometrySchlickGGX(n_dot_v, roughness) * GeometrySchlickGGX(n_dot_l, roughness);\n"
        "    const float3 spec = (d * g * f) / max(4.0f * n_dot_v * n_dot_l, 1e-5f);\n"
        "    const float3 kd = (1.0f - f) * (1.0f - metallic);\n"
        "    const float attenuation = 1.0f / (distance * distance + 1.0f);\n"
        "    const float3 radiance = frame_constant.light_color * frame_constant.light_intensity * attenuation;\n"
        "    const float3 ambient = 0.03f * albedo * saturate(surface.occlusion);\n"
        "    const float3 color = ambient + surface.emissive + (kd * albedo / kPi + spec) * radiance * n_dot_l;\n"
        "    return float4(saturate(color), alpha);\n"
        "}}\n",
        declarations,
        sanitize_hlsl_identifier(options.entry_point),
        body,
        surface_body);

    result.source_hash                = hash_string(result.hlsl);
    const auto            shader_path = generated_shader_path(options.shader_name);
    const gfx::ShaderDesc vertex_shader{
        .name        = std::pmr::string(std::format("{}VS", options.shader_name)),
        .type        = gfx::ShaderType::Vertex,
        .entry       = "VSMain",
        .source_code = result.hlsl,
        .path        = shader_path,
    };
    result.shader = gfx::ShaderDesc{
        .name        = options.shader_name,
        .type        = gfx::ShaderType::Pixel,
        .entry       = options.entry_point,
        .source_code = result.hlsl,
        .path        = shader_path,
    };
    result.pipeline_desc = gfx::RenderPipelineDesc{
        .name                = std::pmr::string(std::format("{}Pipeline", options.shader_name)),
        .assembly_state      = {.primitive = gfx::PrimitiveTopology::TriangleList},
        .rasterization_state = {.cull_mode = gfx::CullMode::None},
        .depth_stencil_state = {
            .depth_test_enable  = true,
            .depth_write_enable = true,
            .depth_compare_op   = gfx::CompareOp::Less,
        },
        .render_format        = gfx::Format::R8G8B8A8_UNORM,
        .depth_stencil_format = gfx::Format::D32_FLOAT,
    };
    std::pmr::vector<std::shared_ptr<asset::Shader>> shaders;
    shaders.emplace_back(std::make_shared<asset::Shader>(vertex_shader));
    shaders.emplace_back(std::make_shared<asset::Shader>(result.shader));
    result.pass.pipeline = std::make_shared<asset::RenderPipeline>(result.pipeline_desc, std::move(shaders));

    result.cache_key = kFnvOffset;
    hash_value(result.cache_key, result.graph_hash);
    hash_value(result.cache_key, result.source_hash);
    hash_value(result.cache_key, hash_material_pass_layout(result.pass));
    hash_value(result.cache_key, options.pipeline_state_hash);
    return result;
}

auto MaterialShaderCache::CompileOrGet(const MaterialGraph& graph, const MaterialGraphCompileOptions& options) -> MaterialGraphCompileResult {
    auto compiled = m_Compiler.Compile(graph, options);
    if (!compiled) return compiled;
    if (!options.force_recompile) {
        if (const auto iter = m_Cache.find(compiled.cache_key); iter != m_Cache.end()) return iter->second;
    }
    m_Cache[compiled.cache_key] = compiled;
    return compiled;
}

auto MaterialShaderCache::Recompile(const MaterialGraph& graph, MaterialGraphCompileOptions options) -> MaterialGraphCompileResult {
    options.force_recompile = true;
    return CompileOrGet(graph, options);
}

bool MaterialShaderCache::Invalidate(std::uint64_t cache_key) {
    return m_Cache.erase(cache_key) != 0;
}

void MaterialShaderCache::Clear() noexcept {
    m_Cache.clear();
}

}  // namespace hitagi
