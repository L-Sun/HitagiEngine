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
#include "hitagi/resource/mesh.hpp"

using namespace hitagi::math;
using namespace hitagi::resource;

namespace hitagi {
debugger::DebugManager* debug_manager = nullptr;
}
namespace hitagi::debugger {

bool DebugManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("DebugManager");
    m_Logger->info("Initialize...");

    auto material_instance = asset_manager->ImportMaterial("assets/material/debug_line.json");
    if (!material_instance.has_value()) {
        m_Logger->error("Can not load debug material!");
        return false;
    }
    m_LineMaterial = material_instance->GetMaterial().lock();

    auto x_axis = MeshFactory::Line({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f});
    auto y_axis = MeshFactory::Line({0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    auto z_axis = MeshFactory::Line({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
    auto box    = MeshFactory::BoxWireframe(vec3f(-0.5f, -0.5f, -0.5f), vec3f(0.5f, 0.5f, 0.5f));

    m_MeshPrototype = merge_meshes({x_axis, y_axis, z_axis, box});

    for (auto& sub_meshe : m_MeshPrototype.sub_meshes) {
        sub_meshe.material = m_LineMaterial->CreateInstance();
    }

    m_MeshPrototype.sub_meshes[0].material.SetParameter("color", vec4f(1, 0, 0, 1));
    m_MeshPrototype.sub_meshes[1].material.SetParameter("color", vec4f(0, 1, 0, 1));
    m_MeshPrototype.sub_meshes[2].material.SetParameter("color", vec4f(0, 0, 1, 1));

    m_MeshPrototype.vertices->name = "Debug vertices";
    m_MeshPrototype.indices->name  = "Debug indices";

    return true;
}

void DebugManager::Finalize() {
    m_LineMaterial = nullptr;
    m_DebugDrawItems.clear();
    m_DrawItemColors.clear();
    m_MeshPrototype = {};

    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

void DebugManager::Tick() {
    if (m_DrawDebugInfo)
        DrawPrimitive();

    m_DrawItemColors.clear();
}

void DebugManager::ToggleDebugInfo() {
    m_DrawDebugInfo = !m_DrawDebugInfo;
}

void DebugManager::DrawLine(const vec3f& from, const vec3f& to, const vec4f& color, bool depth_enabled) {
    if (!m_DrawDebugInfo) return;
    vec3f dir = normalize(to - from);

    vec3f axis(0);
    axis[min_index(dir)] = 1;

    mat4f transform = translate(from) * rotate(std::numbers::pi_v<float>, normalize(dir + axis)) * scale((to - from).norm());

    m_DebugDrawItems.emplace_back(Renderable{
        .type              = Renderable::Type::Debug,
        .vertices          = m_MeshPrototype.vertices.get(),
        .indices           = m_MeshPrototype.indices.get(),
        .index_count       = m_MeshPrototype.sub_meshes[min_index(dir)].index_count,
        .vertex_offset     = m_MeshPrototype.sub_meshes[min_index(dir)].vertex_offset,
        .index_offset      = m_MeshPrototype.sub_meshes[min_index(dir)].index_offset,
        .material          = m_LineMaterial.get(),
        .material_instance = &m_MeshPrototype.sub_meshes[min_index(dir)].material,
        .transform         = transform,
    });
}

void DebugManager::DrawAxis(const mat4f& transform, bool depth_enabled) {
    if (!m_DrawDebugInfo) return;
    for (std::size_t axis = 0; axis < 3; axis++) {
        m_DebugDrawItems.emplace_back(Renderable{
            .type              = Renderable::Type::Debug,
            .vertices          = m_MeshPrototype.vertices.get(),
            .indices           = m_MeshPrototype.indices.get(),
            .index_count       = m_MeshPrototype.sub_meshes[axis].index_count,
            .vertex_offset     = m_MeshPrototype.sub_meshes[axis].vertex_offset,
            .index_offset      = m_MeshPrototype.sub_meshes[axis].index_offset,
            .material          = m_LineMaterial.get(),
            .material_instance = &m_MeshPrototype.sub_meshes[axis].material,
            .transform         = transform,
        });
    }
}

void DebugManager::DrawBox(const mat4f& transform, const vec4f& color, bool depth_enabled) {
    if (!m_DrawDebugInfo) return;
    auto& material_instance = m_DrawItemColors.emplace_back(m_MeshPrototype.sub_meshes[3].material);
    material_instance.SetParameter("color", color);

    m_DebugDrawItems.emplace_back(Renderable{
        .type              = Renderable::Type::Debug,
        .vertices          = m_MeshPrototype.vertices.get(),
        .indices           = m_MeshPrototype.indices.get(),
        .index_count       = m_MeshPrototype.sub_meshes[3].index_count,
        .vertex_offset     = m_MeshPrototype.sub_meshes[3].vertex_offset,
        .index_offset      = m_MeshPrototype.sub_meshes[3].index_offset,
        .material          = m_LineMaterial.get(),
        .material_instance = &material_instance,
        .transform         = transform,
    });
}

void DebugManager::DrawPrimitive() {
    graphics_manager->AppendRenderables(std::move(m_DebugDrawItems));
}

}  // namespace hitagi::debugger