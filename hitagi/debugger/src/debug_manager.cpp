#include <hitagi/debugger/debug_manager.hpp>
#include <hitagi/core/memory_manager.hpp>
#include <hitagi/core/config_manager.hpp>
#include <hitagi/ecs/schedule.hpp>
#include <hitagi/resource/mesh_factory.hpp>
#include <hitagi/resource/asset_manager.hpp>
#include <hitagi/resource/scene_manager.hpp>
#include <hitagi/graphics/graphics_manager.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <fmt/chrono.h>

#include <algorithm>
#include <iterator>

using namespace hitagi::math;
using namespace hitagi::resource;

namespace hitagi {
debugger::DebugManager* debug_manager = nullptr;
}
namespace hitagi::debugger {

bool DebugManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("DebugManager");
    m_Logger->info("Initialize...");

    auto material = asset_manager->ImportMaterial("assets/material/debug_line.json");
    if (!material.has_value()) {
        m_Logger->error("Can not load debug material!");
        return false;
    }
    m_LineMaterialInstance = material.value();

    auto box = MeshFactory::BoxWireframe(vec3f(-0.5f, -0.5f, -0.5f), vec3f(0.5f, 0.5f, 0.5f), {0.0f, 0.0f, 0.0f, 1.0f});

    m_MeshBuffer.vertices       = std::make_shared<VertexArray>(1);
    m_MeshBuffer.indices        = std::make_shared<IndexArray>(1, IndexType::UINT32);
    m_MeshBuffer.vertices->name = "Debug vertices";
    m_MeshBuffer.indices->name  = "Debug indices";

    m_VertexOffset = m_MeshBuffer.vertices->vertex_count;
    m_IndexOffset  = m_MeshBuffer.indices->index_count;

    for (auto& sub_mesh : m_MeshBuffer.sub_meshes) {
        sub_mesh.material = m_LineMaterialInstance;
    }

    return true;
}

void DebugManager::Finalize() {
    m_LineMaterialInstance = {};
    m_DebugDrawItems.clear();
    m_MeshBuffer = {};

    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

void DebugManager::Tick() {
    if (m_DrawDebugInfo)
        DrawPrimitive();

    m_VertexOffset = 0;
    m_IndexOffset  = 0;
    m_DebugDrawItems.clear();
}

void DebugManager::ToggleDebugInfo() {
    m_DrawDebugInfo = !m_DrawDebugInfo;
}

void DebugManager::DrawLine(const vec3f& from, const vec3f& to, const vec4f& color, bool depth_enabled) {
    if (!m_DrawDebugInfo) return;
    AddPrimitive(MeshFactory::Line(from, to, color), {}, depth_enabled);
}

void DebugManager::DrawAxis(const mat4f& transform, bool depth_enabled) {
    if (!m_DrawDebugInfo) return;
    auto axis = merge_meshes({
        MeshFactory::Line({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}),
        MeshFactory::Line({0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}),
        MeshFactory::Line({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}),
    });

    AddPrimitive(axis, {transform}, depth_enabled);
}

void DebugManager::DrawBox(const mat4f& transform, const vec4f& color, bool depth_enabled) {
    if (!m_DrawDebugInfo) return;
    AddPrimitive(MeshFactory::BoxWireframe({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}), {transform}, depth_enabled);
}

void DebugManager::AddPrimitive(Mesh mesh, Transform transform, bool depth_enabled) {
    // Update mesh buffer
    if (m_MeshBuffer.vertices->vertex_count - m_VertexOffset < mesh.vertices->vertex_count) {
        m_MeshBuffer.vertices->Resize(2 * (m_MeshBuffer.vertices->vertex_count) + mesh.vertices->vertex_count);
    }
    if (m_MeshBuffer.indices->index_count - m_IndexOffset < mesh.indices->index_count) {
        m_MeshBuffer.indices->Resize(2 * (m_MeshBuffer.indices->index_count) + mesh.indices->index_count);
    }
    m_MeshBuffer.vertices->Modify<VertexAttribute::Position, VertexAttribute::Color0>([&](auto positions, auto colors) {
        auto _pos = mesh.vertices->GetVertices<VertexAttribute::Position>();
        std::copy(_pos.begin(), _pos.end(), std::next(positions.begin(), m_VertexOffset));

        auto color = mesh.vertices->GetVertices<VertexAttribute::Color0>();
        std::copy(color.begin(), color.end(), std::next(colors.begin(), m_VertexOffset));
    });

    m_MeshBuffer.indices->Modify<IndexType::UINT32>([&](auto array) {
        auto _array = mesh.indices->GetIndices<IndexType::UINT32>();
        std::copy(_array.begin(), _array.end(), std::next(array.begin(), m_IndexOffset));
    });

    vec4u view_port,
        scissor;
    {
        auto&         camera = scene_manager->CurrentScene().GetCurrentCamera();
        auto&         config = config_manager->GetConfig();
        std::uint32_t height = config.height;
        std::uint32_t width  = height * camera.aspect;
        if (width > config.width) {
            width     = config.width;
            height    = config.width / camera.aspect;
            view_port = {0, (config.height - height) >> 1, width, height};
        } else {
            view_port = {(config.width - width) >> 1, 0, width, height};
        }
        scissor = {view_port.x, view_port.y, view_port.x + width, view_port.y + height};
    }
    resource::Renderable item{
        .type                = Renderable::Type::Debug,
        .vertices            = m_MeshBuffer.vertices.get(),
        .indices             = m_MeshBuffer.indices.get(),
        .material            = m_LineMaterialInstance.GetMaterial().lock().get(),
        .material_instance   = &m_LineMaterialInstance,
        .transform           = transform,
        .pipeline_parameters = {.view_port = view_port, .scissor_react = scissor},
    };

    for (const auto& sub_mesh : mesh.sub_meshes) {
        item.index_count   = sub_mesh.index_count;
        item.index_offset  = m_IndexOffset + sub_mesh.index_offset;
        item.vertex_offset = m_VertexOffset + sub_mesh.vertex_offset;
        m_DebugDrawItems.emplace_back(item);
    }

    m_VertexOffset += mesh.vertices->vertex_count;
    m_IndexOffset += mesh.indices->index_count;
}

void DebugManager::DrawPrimitive() {
    graphics_manager->AppendRenderables(std::move(m_DebugDrawItems));
}

}  // namespace hitagi::debugger