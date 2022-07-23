#include <hitagi/debugger/debug_manager.hpp>
#include <hitagi/core/memory_manager.hpp>
#include <hitagi/resource/mesh_factory.hpp>
#include <hitagi/resource/asset_manager.hpp>
#include <hitagi/resource/renderable.hpp>
#include <hitagi/graphics/graphics_manager.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <fmt/chrono.h>

#include <algorithm>
#include <iterator>

using namespace hitagi::math;
using namespace hitagi::resource;

namespace hitagi {
std::unique_ptr<debugger::DebugManager> g_DebugManager = std::make_unique<debugger::DebugManager>();
}
namespace hitagi::debugger {

int DebugManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("DebugManager");
    m_Logger->info("Initialize...");

    m_LineMaterial = g_AssetManager->ImportMaterial("assets/material/debug_line.json");

    m_DebugPrimitives.emplace("x_axis", MeshFactory::Line({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}));
    m_DebugPrimitives.emplace("y_axis", MeshFactory::Line({0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}));
    m_DebugPrimitives.emplace("z_axis", MeshFactory::Line({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}));

    m_DebugPrimitives["x_axis"].material = m_LineMaterial;
    m_DebugPrimitives["y_axis"].material = m_LineMaterial;
    m_DebugPrimitives["z_axis"].material = m_LineMaterial;
    m_DebugPrimitives["x_axis"].SetName("x_axis");
    m_DebugPrimitives["y_axis"].SetName("y_axis");
    m_DebugPrimitives["z_axis"].SetName("z_axis");

    m_DebugPrimitives.emplace("box", MeshFactory::BoxWireframe(vec3f(-0.5f, -0.5f, -0.5f), vec3f(0.5f, 0.5f, 0.5f), {0.0f, 0.0f, 0.0f, 1.0f}));
    m_DebugPrimitives["box"].material = m_LineMaterial;
    m_DebugPrimitives["box"].SetName("box");

    return 0;
}

void DebugManager::Finalize() {
    m_LineMaterial = nullptr;
    m_DebugPrimitives.clear();

    m_Logger->info("Finalized.");
    m_Logger = nullptr;
}

void DebugManager::Tick() {
    RetiredPrimitive();
    DrawPrimitive();
}

void DebugManager::ToggleDebugInfo() {
    m_DrawDebugInfo = !m_DrawDebugInfo;
}

void DebugManager::DrawLine(const vec3f& from, const vec3f& to, const vec4f& color, const std::chrono::seconds duration, bool depth_enabled) {
    Geometry line(std::make_shared<Transform>());
    line.meshes.emplace_back(MeshFactory::Line(from, to, color));

    AddPrimitive(std::move(line), duration, depth_enabled);
}

void DebugManager::DrawAxis(const mat4f& transform, bool depth_enabled) {
    Geometry axis(std::make_shared<Transform>(decompose(transform)));
    axis.meshes.emplace_back(m_DebugPrimitives["x_axis"]);
    axis.meshes.emplace_back(m_DebugPrimitives["y_axis"]);
    axis.meshes.emplace_back(m_DebugPrimitives["z_axis"]);

    AddPrimitive(std::move(axis), std::chrono::seconds(0), depth_enabled);
}

void DebugManager::DrawBox(const mat4f& transform, const vec4f& color, const std::chrono::seconds duration, bool depth_enabled) {
    Geometry box(std::make_shared<Transform>(decompose(transform)));
    box.meshes.emplace_back(m_DebugPrimitives["box"]);

    AddPrimitive(std::move(box), duration, depth_enabled);
}

void DebugManager::AddPrimitive(Geometry&& geometry, std::chrono::seconds duration, bool depth_enabled) {
    m_DebugDrawItems.emplace_back(DebugPrimitive{std::move(geometry), std::chrono::high_resolution_clock::now() + duration});
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

void DebugManager::DrawPrimitive() const {
    std::pmr::vector<Renderable> renderables;
    for (const auto& primitive : m_DebugDrawItems) {
        auto& geometry = primitive.geometry;
        for (const auto& mesh : geometry.meshes) {
            Renderable item;
            item.type      = Renderable::Type::Debug;
            item.mesh      = mesh;
            item.material  = mesh.material->GetMaterial().lock();
            item.transform = geometry.transform;
            renderables.emplace_back(std::move(item));
        }
    }

    g_GraphicsManager->AppendRenderables(renderables);
}

}  // namespace hitagi::debugger