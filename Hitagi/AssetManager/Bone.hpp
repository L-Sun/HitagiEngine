#pragma once
#include "SceneObject.hpp"

#include <Math/Matrix.hpp>

#include <map>

namespace Hitagi::Asset {
class Bone : public SceneObject {
public:
    using SceneObject::SceneObject;

    inline void SetWeight(size_t vertex_index, float weight) {
        m_Weights[vertex_index] = weight;
    }

    inline void SetBindTransformMatrix(Math::mat4f transform) {
        m_BindTransform = std::move(transform);
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
    std::map<size_t, float> m_Weights;
    Math::mat4f             m_BindTransform;
};

}  // namespace Hitagi::Asset
