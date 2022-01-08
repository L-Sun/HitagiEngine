#pragma once
#include <fmt/format.h>

#include "SceneObject.hpp"

namespace Hitagi::Asset {

class BaseSceneNode {
protected:
    std::string                               m_Name;
    std::list<std::shared_ptr<BaseSceneNode>> m_Chlidren;
    std::list<mat4f>                          m_Transforms;
    mat4f                                     m_RuntimeTransform = mat4f(1.0f);
    bool                                      m_Dirty            = true;

    virtual void Dump(std::ostream& out, unsigned indent) const {}

public:
    BaseSceneNode() = default;
    BaseSceneNode(std::string_view name) : m_Name(name) {}

    virtual ~BaseSceneNode() = default;

    const std::string& GetName() const { return m_Name; }

    void AppendChild(std::shared_ptr<BaseSceneNode>&& sub_node) { m_Chlidren.push_back(std::move(sub_node)); }
    void AppendTransform(mat4f transform) {
        m_Transforms.push_back(std::move(transform));
    }

    mat4f GetCalculatedTransform() const {
        mat4f result(1.0f);

        for (auto trans : m_Transforms) {
            result = trans * result;
        }
        result = m_RuntimeTransform * result;
        return result;
    }
    // Get is the node updated
    bool Dirty() const { return m_Dirty; }
    void ClearDirty() { m_Dirty = false; }

    void ApplyTransform(const mat4f& trans) {
        m_RuntimeTransform = trans * m_RuntimeTransform;
        m_Dirty            = true;
    }

    void Reset(bool recursive = false) {
        m_RuntimeTransform = mat4f(1.0f);
        m_Dirty            = true;
        if (recursive)
            for (auto&& child : m_Chlidren)
                if (child) child->Reset(recursive);
    }

    friend std::ostream& operator<<(std::ostream& out, const BaseSceneNode& node) {
        static unsigned indent = 0;
        out << fmt::format(
            "{0:{1}}{2:-^30}\n"
            "{0:{1}}{3:<15}{4:>15}\n",
            "", indent, "Scene Node", "Name:", node.m_Name);

        node.Dump(out, indent);
        out << fmt::format("{0:{1}}{2}\n", "", indent, "Children:");
        indent += 4;
        for (const std::shared_ptr<BaseSceneNode>& sub_node : node.m_Chlidren) {
            out << *sub_node << std::endl;
        }

        indent -= 4;
        return out << fmt::format("{0:{1}}{2:-^30}", "", indent, "End");
    }
};

template <typename T>
class SceneNode : public BaseSceneNode {
protected:
    std::weak_ptr<T> m_SceneObjectRef;

    void Dump(std::ostream& out, unsigned indent) const override {
        if (auto obj = m_SceneObjectRef.lock()) {
            out << fmt::format("{0:{1}}{2:<15}{3:>15}\n", "", indent, "Obj Ref:", obj->GetGuid());
        }
    }

public:
    using BaseSceneNode::BaseSceneNode;
    SceneNode() = default;
    void             AddSceneObjectRef(std::weak_ptr<T> ref) { m_SceneObjectRef = ref; }
    std::weak_ptr<T> GetSceneObjectRef() { return m_SceneObjectRef; }
};

using SceneEmptyNode = BaseSceneNode;

class GeometryNode : public SceneNode<Geometry> {
protected:
    bool m_Visible = true;

    void Dump(std::ostream& out, unsigned indent) const override {
        SceneNode::Dump(out, indent);
        out << fmt::format("{0:{1}}{2:<15}{3:>15}\n", "", indent, "Visible:", m_Visible);
    }

public:
    using SceneNode::AddSceneObjectRef;
    using SceneNode::SceneNode;

    void       SetVisibility(bool visible) { m_Visible = visible; }
    const bool Visible() { return m_Visible; }
};

class LightNode : public SceneNode<Light> {
public:
    using SceneNode::SceneNode;
};

class CameraNode : public SceneNode<Camera> {
public:
    CameraNode(std::string_view name, const vec3f& position = vec3f(0.0f), const vec3f& up = vec3f(0, 0, 1),
               const vec3f& look_at = vec3f(0, -1, 0))
        : SceneNode(name),
          m_Position(position),
          m_LookAt(normalize(look_at)),
          m_Right(normalize(cross(look_at, up))),
          m_Up(normalize(cross(m_Right, m_LookAt))) {}

    mat4f GetViewMatrix() const { return look_at(m_Position, m_LookAt, m_Up) * inverse(GetCalculatedTransform()); }

    vec3f GetCameraPosition() const { return (GetCalculatedTransform() * vec4f(m_Position, 1)).xyz; }
    vec3f GetCameraUp() const { return (GetCalculatedTransform() * vec4f(m_Up, 0)).xyz; }
    vec3f GetCameraLookAt() const { return (GetCalculatedTransform() * vec4f(m_LookAt, 0)).xyz; }
    vec3f GetCameraRight() const { return (GetCalculatedTransform() * vec4f(m_Right, 0)).xyz; }

private:
    vec3f m_Position;
    vec3f m_LookAt;
    vec3f m_Right;
    vec3f m_Up;
};

}  // namespace Hitagi::Asset
