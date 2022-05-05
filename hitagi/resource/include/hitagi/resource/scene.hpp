#pragma once
#include <hitagi/resource/scene_object.hpp>
#include <hitagi/resource/light.hpp>

namespace hitagi::resource {
class Scene : public SceneObject {
public:
    void AddLight(std::shared_ptr<Light> light) {}

private:
    std::pmr::vector<std::shared_ptr<Camera>> m_Cameras{};
};
}  // namespace hitagi::resource