export module editor:material_graph;
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

}  // namespace hitagi

namespace hitagi {
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

}  // namespace hitagi
