#include <hitagi/debugger/debug_manager.hpp>
#include <hitagi/core/memory_manager.hpp>
#include <hitagi/ecs/schedule.hpp>
#include <hitagi/resource/mesh_factory.hpp>
#include <hitagi/resource/asset_manager.hpp>
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

    auto x_axis = MeshFactory::Line({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f});
    auto y_axis = MeshFactory::Line({0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f});
    auto z_axis = MeshFactory::Line({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f});

    m_DebugPrimitivePrototypes["axis"] = merge_meshes({x_axis, y_axis, z_axis});
    m_DebugPrimitivePrototypes["box"]  = MeshFactory::BoxWireframe(vec3f(-0.5f, -0.5f, -0.5f), vec3f(0.5f, 0.5f, 0.5f), {0.0f, 0.0f, 0.0f, 1.0f});

    m_DebugPrimitivePrototypes["axis"].vertices->name = "debug_axis";
    m_DebugPrimitivePrototypes["box"].vertices->name  = "debug_box";
    m_DebugPrimitivePrototypes["axis"].indices->name  = "debug_axis";
    m_DebugPrimitivePrototypes["box"].indices->name   = "debug_box";

    for (auto& [name, mesh] : m_DebugPrimitivePrototypes) {
        for (auto& sub_mesh : mesh.sub_meshes) {
            sub_mesh.material = m_LineMaterialInstance;
        }
    }

    return true;
}

void DebugManager::Finalize() {
    m_LineMaterialInstance = {};
    m_DebugDrawItems.clear();
    m_DebugPrimitivePrototypes.clear();

    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

void DebugManager::Tick() {
    RetiredPrimitive();
    if (m_DrawDebugInfo)
        DrawPrimitive();
}

void DebugManager::ToggleDebugInfo() {
    m_DrawDebugInfo = !m_DrawDebugInfo;
}

void DebugManager::DrawLine(const vec3f& from, const vec3f& to, const vec4f& color, const std::chrono::seconds duration, bool depth_enabled) {
    AddPrimitive(MeshFactory::Line(from, to, color), {}, duration, depth_enabled);
}

void DebugManager::DrawAxis(const mat4f& transform, bool depth_enabled) {
    AddPrimitive(m_DebugPrimitivePrototypes["axis"], {transform}, std::chrono::seconds(0), depth_enabled);
}

void DebugManager::DrawBox(const mat4f& transform, const vec4f& color, const std::chrono::seconds duration, bool depth_enabled) {
    AddPrimitive(m_DebugPrimitivePrototypes["box"], {transform}, duration, depth_enabled);
}

void DebugManager::RetiredPrimitive() {
    std::sort(m_DebugDrawItems.begin(), m_DebugDrawItems.end(),
              [](const DebugPrimitive& lhs, const DebugPrimitive& rhs) -> bool {
                  return lhs.expires_at > rhs.expires_at;
              });

    auto iter = std::find_if(m_DebugDrawItems.begin(), m_DebugDrawItems.end(),
                             [](DebugPrimitive& item) {
                                 if (item.dirty) {
                                     item.dirty = false;
                                     return false;
                                 }
                                 return item.expires_at < std::chrono::high_resolution_clock::now();
                             });
    m_DebugDrawItems.erase(iter, m_DebugDrawItems.end());
}

void DebugManager::AddPrimitive(const Mesh& mesh, Transform transform, std::chrono::seconds duration, bool depth_enabled) {
    DebugPrimitive item;
    item.type       = Renderable::Type::Debug;
    item.vertices   = mesh.vertices;
    item.indices    = mesh.indices;
    item.transform  = transform;
    item.material   = m_LineMaterialInstance.GetMaterial().lock();
    item.expires_at = std::chrono::high_resolution_clock::now() + duration;
    item.dirty      = true;

    for (const auto& sub_mesh : mesh.sub_meshes) {
        item.sub_mesh = sub_mesh;
        m_DebugDrawItems.emplace_back(item);
    }
}

void DebugManager::DrawPrimitive() const {
    std::pmr::vector<Renderable> result;
    result.reserve(m_DebugDrawItems.size());
    std::transform(m_DebugDrawItems.begin(), m_DebugDrawItems.end(), std::back_inserter(result), [](const DebugPrimitive& item) {
        return static_cast<Renderable>(item);
    });
    graphics_manager->AppendRenderables(result);
}

}  // namespace hitagi::debugger