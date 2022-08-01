#include <hitagi/resource/scene.hpp>
#include <hitagi/core/config_manager.hpp>
#include <hitagi/ecs/schedule.hpp>

#include <algorithm>

namespace hitagi::resource {
Scene::Scene(std::string_view name) : name(name), world(name) {}

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
                        .vertices            = mesh.vertices,
                        .indices             = mesh.indices,
                        .sub_mesh            = sub_mesh,
                        .material            = sub_mesh.material.GetMaterial().lock(),
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

const Camera& Scene::GetCurrentCamera() const {
    return world.AccessEntity<Camera>(cameras.at(curr_camera_index))->get();
}

Camera& Scene::GetCurrentCamera() {
    return const_cast<Camera&>(const_cast<const Scene*>(this)->GetCurrentCamera());
}

}  // namespace hitagi::resource
