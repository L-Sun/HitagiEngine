#include "SceneNode.hpp"

using namespace hitagi::math;

namespace hitagi::asset {

auto SceneNode::GetCalculatedTransformation() -> mat4f {
    if (m_TransformDirty) {
        m_RuntimeTransform = translate(rotate(scale(mat4f(1.0f), m_Scaling), m_Rotation), m_Translation);
        if (auto parent = m_Parent.lock()) {
            m_RuntimeTransform = parent->GetCalculatedTransformation() * m_RuntimeTransform;
        }
        m_TransformDirty = false;
    }
    return m_RuntimeTransform;
}

auto SceneNode::SetTRS(const vec3f& translation, const quatf& routation, const vec3f& scaling) -> void {
    m_Translation = translation;
    m_Rotation    = routation;
    m_Scaling     = scaling;

    SetTransformDirty();
}

auto SceneNode::ApplyTransform(const mat4f& mat) -> void {
    auto [translation, rotation, scaling] = decompose(mat);

    m_Rotation = euler_to_quaternion(rotation) * m_Rotation;
    m_Translation += translation;
    m_Scaling = m_Scaling * scaling;

    SetTransformDirty();
}

auto SceneNode::Translate(const vec3f& translate) -> void {
    m_Translation += translate;
    SetTransformDirty();
}

auto SceneNode::Rotate(const vec3f& eular) -> void {
    m_Rotation = euler_to_quaternion(eular) * m_Rotation;
    SetTransformDirty();
}
auto SceneNode::Scale(const vec3f& value) -> void {
    m_Scaling = m_Scaling * value;
    SetTransformDirty();
}

auto SceneNode::SetTransformDirty() -> void {
    m_TransformDirty = true;
    for (auto&& child : m_Children) {
        child->SetTransformDirty();
    }
}

std::ostream& operator<<(std::ostream& out, const SceneNode& node) {
    static unsigned indent = 0;
    out << fmt::format(
        "{0:{1}}{2:-^30}\n"
        "{0:{1}}{3:<15}{4:>15}\n",
        "", indent, "Scene Node", "Name:", node.m_Name);

    node.Dump(out, indent);
    out << fmt::format("{0:{1}}{2}\n", "", indent, "Children:");
    indent += 4;
    for (const std::shared_ptr<SceneNode>& sub_node : node.m_Children) {
        out << *sub_node << std::endl;
    }

    indent -= 4;
    return out << fmt::format("{0:{1}}{2:-^30}", "", indent, "End");
}

}  // namespace hitagi::asset