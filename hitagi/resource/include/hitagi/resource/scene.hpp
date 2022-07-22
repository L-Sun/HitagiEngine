#pragma once
#include <hitagi/resource/geometry.hpp>
#include <hitagi/resource/camera.hpp>
#include <hitagi/resource/light.hpp>
#include <hitagi/resource/renderable.hpp>

#include <magic_enum.hpp>

namespace hitagi::resource {
class Scene : public ResourceObject {
public:
    Scene() = default;

    std::pmr::vector<Renderable> GetRenderable();

    std::pmr::vector<Geometry>                       geometries;
    std::pmr::unordered_set<std::shared_ptr<Light>>  lights;
    std::pmr::unordered_set<std::shared_ptr<Camera>> cameras;
};

}  // namespace hitagi::resource