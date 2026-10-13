export module render:types;
import std;
import utils;
import math;
import core;
import gfx;
import asset;

export namespace hitagi::render {

struct TextDrawCommand {
    std::pmr::string text;
    math::vec2f      position  = {0.0f, 0.0f};
    float            font_size = 24.0f;
    math::Color      color     = math::Color::White();
};

struct RenderContext {
    gfx::Device&                    device;
    rg::RenderGraph&                graph;
    std::shared_ptr<gfx::SwapChain> swap_chain = nullptr;
};

struct RenderView {
    math::vec3f camera_position = {};
    math::mat4f view            = math::mat4f::identity();
    math::mat4f projection      = math::mat4f::identity();
};

struct RenderDrawItem {
    std::shared_ptr<asset::Mesh> mesh;
    math::mat4f                  transform = math::mat4f::identity();
    std::uint32_t                object_id = 0;
};

struct RenderLight {
    math::vec3f position;
    math::Color color     = math::Color::White();
    float       intensity = 1.0f;
};

struct RenderFrame {
    RenderView                      view;
    std::span<const RenderDrawItem> draw_items;
    std::span<const RenderLight>    lights;
};

struct RenderOutputMask {
    bool depth         = false;
    bool linear_depth  = false;
    bool object_id     = false;
    bool normal        = false;
    bool motion_vector = false;
};

enum struct RenderGraphDebugView : std::uint8_t {
    Final,
    BaseColor,
    Normal,
    Metallic,
    Roughness,
    Occlusion,
    MaterialId,
    Emissive,
};

struct RenderRequest {
    RenderFrame       frame;
    rg::TextureHandle target = {};
    RenderOutputMask  requested_outputs;
    // Selects the deferred renderer output for this call only.
    RenderGraphDebugView debug_view = RenderGraphDebugView::Final;
};

struct RenderResult {
    rg::TextureHandle color         = {};
    rg::TextureHandle depth         = {};
    rg::TextureHandle linear_depth  = {};
    rg::TextureHandle object_id     = {};
    rg::TextureHandle normal        = {};
    rg::TextureHandle motion_vector = {};
};

struct ViewConstant {
    math::vec4f camera_pos;
    math::mat4f view;
    math::mat4f projection;
    math::mat4f proj_view;
    math::mat4f inv_view;
    math::mat4f inv_projection;
    math::mat4f inv_proj_view;
};

struct InstanceConstant {
    math::mat4f model;
};

constexpr auto RenderGraphDebugViewName(RenderGraphDebugView view) noexcept -> std::string_view {
    switch (view) {
        case RenderGraphDebugView::BaseColor:
            return "albedo";
        case RenderGraphDebugView::Normal:
            return "normal";
        case RenderGraphDebugView::Metallic:
        case RenderGraphDebugView::Roughness:
        case RenderGraphDebugView::Occlusion:
            return "material";
        case RenderGraphDebugView::MaterialId:
            return "material_id";
        case RenderGraphDebugView::Emissive:
            return "emissive";
        case RenderGraphDebugView::Final:
            return "final";
    }
    return "final";
}

struct MeshInfo {
    std::shared_ptr<asset::Mesh>                                  mesh;
    utils::EnumArray<rg::GPUBufferHandle, asset::VertexAttribute> vertices;
    rg::GPUBufferHandle                                           indices;
    gfx::Format                                                   index_format = gfx::Format::R32_UINT;
};

struct InstanceInfo {
    std::shared_ptr<asset::Mesh> mesh;
    InstanceConstant             instance_data;
    std::size_t                  instance_index;
    std::uint32_t                object_id = 0;
};

struct RenderGeometry {
    std::pmr::unordered_map<asset::Mesh*, MeshInfo> mesh_infos;
    std::pmr::vector<InstanceInfo>                  instance_infos;
};

// Shader source text together with the path it was read from. Passes receive
// the text directly so they never touch the file system; the path is kept for
// compiler diagnostics and relative #include resolution.
struct ShaderSource {
    std::filesystem::path path;
    std::pmr::string      code;
};

auto LoadShaderSource(core::FileIOManager& file_io, std::filesystem::path path) -> ShaderSource;

}  // namespace hitagi::render

namespace hitagi::render {
auto LoadShaderSource(core::FileIOManager& file_io, std::filesystem::path path) -> ShaderSource {
    std::pmr::string code(file_io.SyncOpenAndReadBinary(path).Str());
    return {.path = std::move(path), .code = std::move(code)};
}
}  // namespace hitagi::render
