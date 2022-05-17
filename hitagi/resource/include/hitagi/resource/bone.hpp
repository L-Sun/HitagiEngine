#pragma once
#include <hitagi/resource/scene_object.hpp>
#include <hitagi/math/matrix.hpp>

#include <map>

namespace hitagi::resource {
class Bone : public SceneObject {
public:
    Bone() = default;

    inline void SetWeight(size_t vertex_index, float weight) {
        m_Weights[vertex_index] = weight;
    }

    inline void SetBindTransformMatrix(math::mat4f transform) {
        m_BindTransform = transform;
    }

    inline float GetWeight(size_t vertex_index) const {
        return m_Weights.count(vertex_index)
                   ? m_Weights.at(vertex_index)
                   : 0.0f;
    }

    inline const auto& GetBindTransform() const noexcept {
        return m_BindTransform;
    }

private:
    std::pmr::map<size_t, float> m_Weights;
    math::mat4f                  m_BindTransform;
};

}  // namespace hitagi::resource
