#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "IPhysicsManager.hpp"
#include "Geometry.hpp"

namespace Hitagi::Physics {
class HitagiPhysicsManager : public IPhysicsManager {
public:
    HitagiPhysicsManager() : m_Logger(spdlog::stdout_color_st("HitagiPhysicsManager")) {
#if defined(_DEBUG)
        m_Logger->set_level(spdlog::level::debug);

#endif
    }
    virtual ~HitagiPhysicsManager() {}

    int  Initialize() final;
    void Finalize() final;
    void Tick() final;

    void CreateRigidBody(Resource::SceneGeometryNode& node, const Resource::SceneObjectGeometry& geometry) final;
    void DeleteRigidBody(Resource::SceneGeometryNode& node) final;

    int  CreateRigidBodies() final;
    void ClearRigidBodies() final;

    mat4f GetRigidBodyTransform(std::shared_ptr<void> rigidBody) final;
    void  UpdateRigidBodyTransform(Resource::SceneGeometryNode& node) final;

    void ApplyCentralForce(std::shared_ptr<void> rigidBody, vec3f force) final;

#if defined(_DEBUG)
    void DrawDebugInfo();
#endif

protected:
#if defined(_DEBUG)
    void DrawAabb(const Geometry& geometry, const mat4f& trans, const vec3f& centerOfMass);
#endif

private:
    std::shared_ptr<spdlog::logger> m_Logger;
};
}  // namespace Hitagi::Physics