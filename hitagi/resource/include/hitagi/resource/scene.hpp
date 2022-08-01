#pragma once
#include <hitagi/resource/transform.hpp>
#include <hitagi/resource/material.hpp>
#include <hitagi/resource/camera.hpp>
#include <hitagi/resource/light.hpp>
#include <hitagi/resource/mesh.hpp>
#include <hitagi/resource/renderable.hpp>
#include <hitagi/ecs/world.hpp>

namespace hitagi::resource {
struct Scene {
    Scene(std::string_view name = "");
    std::pmr::vector<Renderable> GetRenderables() const;

    Camera&       GetCurrentCamera();
    const Camera& GetCurrentCamera() const;

    std::pmr::string name;
    ecs::World       world;

    std::pmr::vector<ecs::Entity> geometries;
    std::pmr::vector<ecs::Entity> cameras;
    std::pmr::vector<ecs::Entity> lights;

    std::size_t curr_camera_index = 0;
};

}  // namespace hitagi::resource