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

    virtual void dump(std::ostream& out) const {}

public:
    BaseSceneNode() {}
    BaseSceneNode(const std::string& name) { m_strName = name; }

    virtual ~BaseSceneNode() {}

    void AppendChild(std::shared_ptr<BaseSceneNode>&& sub_node) {
        m_Chlidren.push_back(std::move(sub_node));
    }
    void AppendChild(std::shared_ptr<SceneObjectTransform>&& transform) {
        m_Transforms.push_back(std::move(transform));
    }

    const std::shared_ptr<mat4> GetCalculatedTransform() const {
        std::shared_ptr<mat4> result(new mat4(1.0f));

        for (auto trans : m_Transforms) {
            *result = *result * static_cast<mat4>(*trans);
        }
        return result;
    }

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
    void AddSceneObjectRef(const std::string& key) { m_keySceneObject = key; }
    const std::string& GetSceneObjectRef() { return m_keySceneObject; }
};

typedef BaseSceneNode SceneEmptyNode;
class SceneGeometryNode : public SceneNode<SceneObjectGeometry> {
protected:
    bool                     m_bVisible;
    bool                     m_bShadow;
    bool                     m_bMotionBlur;
    std::vector<std::string> m_Materials;
    void*                    m_pRigidBody = nullptr;

    virtual void dump(std::ostream& out) const {
        SceneNode::dump(out);
        out << "Visible: " << m_bVisible << std::endl;
        out << "Shadow: " << m_bShadow << std::endl;
        out << "Motion Blur: " << m_bMotionBlur << std::endl;
        out << "Material(s): " << std::endl;
        for (auto material : m_Materials) out << material << std::endl;
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
    void AddMaterialRef(const std::string& key) { m_Materials.push_back(key); }
    void AddMaterialRef(const std::string&& key) {
        m_Materials.push_back(std::move(key));
    }
    std::string GetMaterialRef(const size_t index) {
        return index < m_Materials.size() ? m_Materials[index] : std::string();
    }
    void  LinkRigidBody(void* rigidBody) { m_pRigidBody = rigidBody; }
    void* UnlinkRigidBody() {
        void* rigidBody = m_pRigidBody;
        m_pRigidBody    = nullptr;

        return rigidBody;
    }

    void* RigidBody() { return m_pRigidBody; }
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
    vec3 m_Target;

public:
    using SceneNode::SceneNode;
    void        SetTarget(vec3& target) { m_Target = target; }
    const vec3& GetTarget() { return m_Target; }
};

}  // namespace My
