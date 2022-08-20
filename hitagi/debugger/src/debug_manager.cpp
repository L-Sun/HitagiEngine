#include <hitagi/debugger/debug_manager.hpp>
#include <hitagi/core/memory_manager.hpp>
#include <hitagi/core/config_manager.hpp>
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

    auto x_axis = MeshFactory::Line({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f});
    auto y_axis = MeshFactory::Line({0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    auto z_axis = MeshFactory::Line({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
    auto box    = MeshFactory::BoxWireframe(vec3f(-0.5f, -0.5f, -0.5f), vec3f(0.5f, 0.5f, 0.5f));

    // Vertex offset:
    // 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13
    // |<- x |<- y |<- z |<- box
    // Indices offset:
    // 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, ... , 29
    // |<- x |<- y |<-z  |<- box
    m_DebugPrimitives    = merge_meshes({x_axis, y_axis, z_axis, box});
    m_DebugDrawData.mesh = m_DebugPrimitives;
    m_DebugDrawData.mesh.sub_meshes.clear();

    return true;
}

void DebugManager::Finalize() {
    m_DebugPrimitives = {};
    m_DebugDrawData   = {};

    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

void DebugManager::Tick() {
    if (m_DrawDebugInfo) {
        auto camera                  = scene_manager->CurrentScene().curr_camera->object;
        m_DebugDrawData.project_view = camera->GetProjectionView();
        m_DebugDrawData.view_port    = camera->GetViewPort(config_manager->GetConfig().width, config_manager->GetConfig().height);
        graphics_manager->DrawDebug(m_DebugDrawData);
    }

    m_DebugDrawData.constants.clear();
    m_DebugDrawData.mesh.sub_meshes.clear();
}

void DebugManager::DrawLine(const vec3f& from, const vec3f& to, const vec4f& color, bool depth_enabled) {
    if (!m_DrawDebugInfo) return;
    vec3f dir = normalize(to - from);

    vec3f axis(0);
    // The axis that the angle between it and dir is maximum
    axis[min_index(dir)] = 1;

    mat4f transform = translate(from) * rotate(std::numbers::pi_v<float>, normalize(dir + axis)) * scale((to - from).norm());

    m_DebugDrawData.constants.emplace_back(transform, color);
    m_DebugDrawData.mesh.sub_meshes.emplace_back(m_DebugPrimitives.sub_meshes[min_index(dir)]);
}

void DebugManager::DrawAxis(const mat4f& transform, bool depth_enabled) {
    if (!m_DrawDebugInfo) return;

    for (std::size_t axis = 0; axis < 3; axis++) {
        vec4f color = {0, 0, 0, 1};
        color[axis] = 1;

        m_DebugDrawData.constants.emplace_back(transform, color);
        m_DebugDrawData.mesh.sub_meshes.emplace_back(m_DebugPrimitives.sub_meshes[axis]);
    }
}

void DebugManager::DrawBox(const mat4f& transform, const vec4f& color, bool depth_enabled) {
    if (!m_DrawDebugInfo) return;

    m_DebugDrawData.constants.emplace_back(transform, color);
    m_DebugDrawData.mesh.sub_meshes.emplace_back(m_DebugPrimitives.sub_meshes.back());
}

}  // namespace hitagi::debugger