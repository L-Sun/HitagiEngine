#pragma once
#include <fmt/format.h>

#include "Geometry.hpp"
#include "Camera.hpp"
#include "Light.hpp"

namespace Hitagi::Asset {

class SceneNode {
protected:
    std::string                           m_Name;
    std::list<std::shared_ptr<SceneNode>> m_Chlidren;
    std::list<mat4f>                      m_Transforms;
    mat4f                                 m_RuntimeTransform = mat4f(1.0f);

    virtual void Dump(std::ostream& out, unsigned indent) const {}

public:
    SceneNode() = default;
    SceneNode(std::string_view name) : m_Name(name) {}

    virtual ~SceneNode() = default;

    const std::string& GetName() const { return m_Name; }

    void AppendChild(std::shared_ptr<SceneNode>&& sub_node) { m_Chlidren.push_back(std::move(sub_node)); }
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

    void ApplyTransform(const mat4f& trans) {
        m_RuntimeTransform = trans * m_RuntimeTransform;
    }

    void Reset(bool recursive = false) {
        m_RuntimeTransform = mat4f(1.0f);
        if (recursive)
            for (auto&& child : m_Chlidren)
                if (child) child->Reset(recursive);
    }

    friend std::ostream& operator<<(std::ostream& out, const SceneNode& node) {
        static unsigned indent = 0;
        out << fmt::format(
            "{0:{1}}{2:-^30}\n"
            "{0:{1}}{3:<15}{4:>15}\n",
            "", indent, "Scene Node", "Name:", node.m_Name);

        node.Dump(out, indent);
        out << fmt::format("{0:{1}}{2}\n", "", indent, "Children:");
        indent += 4;
        for (const std::shared_ptr<SceneNode>& sub_node : node.m_Chlidren) {
            out << *sub_node << std::endl;
        }

        indent -= 4;
        return out << fmt::format("{0:{1}}{2:-^30}", "", indent, "End");
    }
};

template <typename T>
class SceneNodeWithRef : public SceneNode {
protected:
    std::weak_ptr<T> m_SceneObjectRef;

    void Dump(std::ostream& out, unsigned indent) const override {
        if (auto obj = m_SceneObjectRef.lock()) {
            out << fmt::format("{0:{1}}{2:<15}{3:>15}\n", "", indent, "Obj Ref:", obj->GetGuid());
        }
    }

public:
    using SceneNode::SceneNode;
    SceneNodeWithRef() = default;
    void             SetSceneObjectRef(std::weak_ptr<T> ref) { m_SceneObjectRef = ref; }
    std::weak_ptr<T> GetSceneObjectRef() { return m_SceneObjectRef; }
};

using SceneEmptyNode = SceneNode;

class GeometryNode : public SceneNodeWithRef<Geometry> {
protected:
    bool m_Visible = true;

    void Dump(std::ostream& out, unsigned indent) const override {
        SceneNodeWithRef::Dump(out, indent);
        out << fmt::format("{0:{1}}{2:<15}{3:>15}\n", "", indent, "Visible:", m_Visible);
    }

public:
    using SceneNodeWithRef::SceneNodeWithRef;
    using SceneNodeWithRef::SetSceneObjectRef;

    void       SetVisibility(bool visible) { m_Visible = visible; }
    const bool Visible() { return m_Visible; }
};

class LightNode : public SceneNodeWithRef<Light> {
public:
    using SceneNodeWithRef::SceneNodeWithRef;
};

class CameraNode : public SceneNodeWithRef<Camera> {
public:
    CameraNode(std::string_view name, const vec3f& position = vec3f(0.0f), const vec3f& up = vec3f(0, 0, 1),
               const vec3f& look_at = vec3f(0, -1, 0))
        : SceneNodeWithRef(name),
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

class BoneNode : public SceneNode {
public:
    using SceneNode::SceneNode;

private:
    bool m_Visibility = false;
};

}  // namespace Hitagi::Asset
