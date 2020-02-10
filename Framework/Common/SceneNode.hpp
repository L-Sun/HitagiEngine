#pragma once
#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <list>
#include "SceneObject.hpp"

namespace My {

class BaseSceneNode {
protected:
    std::string                                      m_strName;
    std::list<std::shared_ptr<BaseSceneNode>>        m_Chlidren;
    std::list<std::shared_ptr<SceneObjectTransform>> m_Transforms;
    mat4f m_RuntimeTransform = mat4f(1.0f);
    bool  m_bDirty           = true;

    virtual void dump(std::ostream& out) const {}

public:
    BaseSceneNode() {}
    BaseSceneNode(std::string_view name) { m_strName = name; }

    virtual ~BaseSceneNode() {}

    const std::string& GetName() const { return m_strName; }

    void AppendChild(std::shared_ptr<BaseSceneNode>&& sub_node) {
        m_Chlidren.push_back(std::move(sub_node));
    }
    void AppendTransform(std::shared_ptr<SceneObjectTransform>&& transform) {
        m_Transforms.push_back(std::move(transform));
    }

    const std::shared_ptr<mat4f> GetCalculatedTransform() const {
        std::shared_ptr<mat4f> result(new mat4f(1.0f));

        for (auto trans : m_Transforms) {
            *result *= static_cast<mat4f>(*trans);
        }
        *result *= m_RuntimeTransform;
        return result;
    }
    // Get is the node updated
    bool Dirty() const { return m_bDirty; }
    void ClearDirty() { m_bDirty = false; }

    void RotateBy(const float& x, const float& y, const float& z) {
        m_RuntimeTransform *= rotate(mat4f(1.0f), x, y, z);
        m_bDirty = true;
    }
    void Move(const float& x, const float& y, const float& z) {
        m_RuntimeTransform *= translate(mat4f(1.0f), vec3f(x, y, z));
        m_bDirty = true;
    }

    void Reset() { m_RuntimeTransform = mat4f(1.0f); }

    friend std::ostream& operator<<(std::ostream&        out,
                                    const BaseSceneNode& node) {
        static thread_local int32_t indent = 0;
        indent++;
        out << std::string(indent, ' ') << "Scene Node" << std::endl;
        out << std::string(indent, ' ') << "----------" << std::endl;
        out << std::string(indent, ' ') << "Name: " << node.m_strName
            << std::endl;
        node.dump(out);
        out << std::endl;

        for (const std::shared_ptr<BaseSceneNode>& sub_node : node.m_Chlidren) {
            out << *sub_node << std::endl;
        }
        for (const std::shared_ptr<SceneObjectTransform>& sub_node :
             node.m_Transforms) {
            out << *sub_node << std::endl;
        }
        indent--;

        return out;
    }
};

template <typename T>
class SceneNode : public BaseSceneNode {
protected:
    std::string m_keySceneObject;

    virtual void dump(std::ostream& out) const {
        out << m_keySceneObject << std::endl;
    }

public:
    using BaseSceneNode::BaseSceneNode;
    SceneNode() = default;
    void AddSceneObjectRef(std::string_view key) { m_keySceneObject = key; }
    const std::string& GetSceneObjectRef() { return m_keySceneObject; }
};

typedef BaseSceneNode SceneEmptyNode;
class SceneGeometryNode : public SceneNode<SceneObjectGeometry> {
protected:
    bool                  m_bVisible;
    bool                  m_bShadow;
    bool                  m_bMotionBlur;
    std::shared_ptr<void> m_pRigidBody = nullptr;

    virtual void dump(std::ostream& out) const {
        SceneNode::dump(out);
        out << "Visible: " << m_bVisible << std::endl;
        out << "Shadow: " << m_bShadow << std::endl;
        out << "Motion Blur: " << m_bMotionBlur << std::endl;
    }

public:
    using SceneNode::SceneNode;
    void SetVisibility(bool visible) { m_bVisible = visible; }
    void SetIfCastShadow(bool shadow) { m_bShadow = shadow; }
    void SetIfMotionBlur(bool motion_blur) { m_bMotionBlur = motion_blur; }
    const bool Visible() { return m_bVisible; }
    const bool CastShadow() { return m_bShadow; }
    const bool MotionBlur() { return m_bMotionBlur; }

    using SceneNode::AddSceneObjectRef;
    void LinkRigidBody(std::shared_ptr<void> rigidBody) {
        m_pRigidBody = rigidBody;
    }
    void UnlinkRigidBody() { m_pRigidBody = nullptr; }

    std::shared_ptr<void> RigidBody() { return m_pRigidBody; }
};

class SceneLightNode : public SceneNode<SceneObjectLight> {
protected:
    bool m_bShadow;

public:
    using SceneNode::SceneNode;
    void       SetIfCastShadow(bool shaodw) { m_bShadow = shaodw; }
    const bool CastShadow() { return m_bShadow; }
};

class SceneCameraNode : public SceneNode<SceneObjectCamera> {
protected:
    vec3f m_Target;

public:
    using SceneNode::SceneNode;
    void         SetTarget(vec3f& target) { m_Target = target; }
    const vec3f& GetTarget() { return m_Target; }
};

}  // namespace My
