#include <hitagi/resource/scene.hpp>
#include <hitagi/core/config_manager.hpp>
#include <hitagi/ecs/schedule.hpp>

#include <algorithm>

namespace hitagi::resource {
Scene::Scene(std::string_view name)
    : name(name),
      world(name),
      root(world.CreateEntity(MetaInfo{std::pmr::string(name), xg::newGuid()})) {
    world.RegisterSystem<TransformSystem>("TransformSystem");
}

std::pmr::vector<Renderable> Scene::GetRenderables() const {
    std::pmr::vector<Renderable> result;

    math::vec4u view_port, scissor;
    {
        auto          camera = GetCurrentCamera();
        auto          config = config_manager->GetConfig();
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

    std::size_t offset = 0;
    for (const auto& archetype : world.GetArchetypes<Mesh, Transform>()) {
        auto meshes = archetype->GetComponentArray<Mesh>();

        // the first element indicated the index of the visiable mesh
        // the last two elemnets indicated the range of the submeshes from the visiable in the result vector
        std::pmr::vector<std::tuple<std::size_t, std::size_t, std::size_t>> ranges;

        for (std::size_t i = 0; i < meshes.size(); i++) {
            const auto& mesh = meshes[i];
            if (mesh.visiable) {
                for (const auto& sub_mesh : mesh.sub_meshes) {
                    result.emplace_back(Renderable{
                        .vertices            = mesh.vertices.get(),
                        .indices             = mesh.indices.get(),
                        .index_count         = sub_mesh.index_count,
                        .vertex_offset       = sub_mesh.vertex_offset,
                        .index_offset        = sub_mesh.index_offset,
                        .material            = sub_mesh.material.GetMaterial().lock().get(),
                        .material_instance   = &sub_mesh.material,
                        .pipeline_parameters = {.view_port     = scissor,
                                                .scissor_react = scissor},
                    });
                }
                ranges.emplace_back(i, offset, offset + mesh.sub_meshes.size());
                offset += mesh.sub_meshes.size();
            }
        }
        auto transforms = archetype->GetComponentArray<Transform>();
        for (auto [index, left, right] : ranges) {
            for (std::size_t i = left; i < right; i++) {
                result.at(i).transform = transforms[index].world_matrix;
            }
        }
    }

    return result;
}

Camera& Scene::GetCurrentCamera() {
    if (cameras.empty()) {
        cameras.emplace_back(world.CreateEntity(MetaInfo{.name = "Default Camera"}, Hierarchy{.parentID = root}, Transform{}, Camera{}));
        curr_camera_index = 0;
    }
    return world.AccessEntity<Camera>(cameras.at(curr_camera_index))->get();
}
const Camera& Scene::GetCurrentCamera() const {
    return const_cast<Scene*>(this)->GetCurrentCamera();
}
}  // namespace hitagi::resource
