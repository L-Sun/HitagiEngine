module;
#include "interop/tracy_macros.hpp"

export module asset.base:skin;
import interop.tracy;
import std;
import math;
import core;
import ecs;
import :resource;
import :mesh;
import :transform;

export namespace hitagi::asset {

// Maps bind-pose mesh-local positions into each joint's local space. Slot i
// matches BlendIndex == i and SkinnedMesh::joints[i], not animation target IDs.
class Skin final : public Resource {
public:
    explicit Skin(std::vector<math::mat4f> inverse_bind_matrices, std::string_view name = "");
    auto GetInverseBindMatrices() const noexcept -> std::span<const math::mat4f> { return m_InverseBindMatrices; }
    void Load(const ResourceLoadContext&) final { SetLoadState(ResourceLoadState::Loaded); }
    void Unload() final { SetLoadState(ResourceLoadState::Unloaded); }

private:
    std::vector<math::mat4f> m_InverseBindMatrices;
};

struct SkinPose {
    std::pmr::vector<math::mat4f> matrices;
    math::AABBf bounds;
    // GPU palette path: immutable frame snapshot and static binding resource.
    std::shared_ptr<const Skin> skin;
    std::shared_ptr<const std::pmr::vector<math::mat4f>> joint_worlds;
    math::mat4f world_to_mesh = math::mat4f::identity();
};

// Compose with MeshComponent + Transform + RelationShip. Multiple instances
// can share Mesh/Skin while referencing independent or shared joint entities.
struct SkinnedMesh {
    std::shared_ptr<const Skin> skin;
    std::vector<ecs::Entity>    joints;

    // Output snapshots, replaced on update. Never modify a published snapshot:
    // submitted render frames may still own it. Only deformed attributes exist
    // here; UVs, colors, indices and materials remain on the shared Mesh.
    std::shared_ptr<VertexArray> deformed_vertices;
    std::shared_ptr<const SkinPose> pose;
    // Explicit offline/reference path. Runtime renderers consume pose on the GPU.
    bool cpu_deformation = false;
    bool gpu_palette = false; // Experimental; CPU reference remains available.
    std::shared_ptr<const VertexArray> validated_vertices;
    std::size_t validated_joint_count = 0;
    std::pmr::vector<math::AABBf> joint_bounds;
    math::AABBf unweighted_bounds;
    std::pmr::vector<math::AABBf> joint_bind_bounds;

};

struct SkinningSystem {
    static void ConfigureSchedule(ecs::Schedule& schedule);
};

}  // namespace hitagi::asset

namespace hitagi::asset {

void ValidateSkinMatrix(const math::mat4f& matrix) {
    for (const auto& row : matrix.data)
        for (float value : row.data)
            if (!std::isfinite(value)) throw std::invalid_argument("Non-finite skin transform");
    // General matrix inversion can round an affine last row away from exact 1.
    constexpr float affine_tolerance = 1e-5f;
    for (unsigned i = 0; i < 4; ++i)
        if (std::abs(matrix[3][i] - (i == 3 ? 1.0f : 0.0f)) > affine_tolerance)
            throw std::invalid_argument("Skin transforms must be affine");
}

Skin::Skin(std::vector<math::mat4f> inverse_bind_matrices, std::string_view name)
    : Resource(Type::Skin, name), m_InverseBindMatrices(std::move(inverse_bind_matrices)) {
    if (m_InverseBindMatrices.empty()) throw std::invalid_argument("Skin requires joints");
    for (const auto& matrix : m_InverseBindMatrices) {
        ValidateSkinMatrix(matrix);
        if (math::determinant(matrix) == 0.0f) throw std::invalid_argument("Singular inverse bind matrix");
    }
}

// Cache is scoped to one skinning update, so animated transforms cannot become stale.
// Sharing requires the same skin, ordered entity bindings, and mesh-local space.
struct SkinPaletteEntry {
    const SkinnedMesh* source;
    math::mat4f mesh_world;
    std::shared_ptr<const SkinPose> pose;
};
struct JointWorldEntry {
    const SkinnedMesh* source;
    std::shared_ptr<const std::pmr::vector<math::mat4f>> worlds;
};
struct SkinPaletteCache {
    std::pmr::vector<SkinPaletteEntry> palettes;
    std::pmr::vector<JointWorldEntry> bindings;
    std::pmr::unordered_map<ecs::Entity, math::mat4f> joint_worlds;
};

auto GatherJointWorlds(const SkinnedMesh& instance, SkinPaletteCache& cache)
    -> std::shared_ptr<const std::pmr::vector<math::mat4f>> {
    auto binding = std::ranges::find_if(cache.bindings, [&](const auto& entry) {
        return std::ranges::equal(entry.source->joints, instance.joints);
    });
    if (binding != cache.bindings.end()) return binding->worlds;
    ZoneScopedNS("Skin.GatherJointWorlds", 0);
    auto worlds = std::make_shared<std::pmr::vector<math::mat4f>>();
    worlds->reserve(instance.joints.size());
    for (auto joint : instance.joints) {
        auto found = cache.joint_worlds.find(joint);
        if (found == cache.joint_worlds.end()) {
            if (!joint || !joint.Has<Transform>()) throw std::invalid_argument("Skin references a missing joint Transform");
            const auto& world = joint.Get<Transform>().world_matrix;
            ValidateSkinMatrix(world);
            found = cache.joint_worlds.emplace(joint, world).first;
        }
        worlds->push_back(found->second);
    }
    cache.bindings.push_back({&instance, worlds});
    return worlds;
}

void UpdateSkinPose(const MeshComponent& component, const Transform& transform, SkinnedMesh& instance,
                    SkinPaletteCache& cache) {
    ZoneScopedNS("Skin.UpdatePose", 0);
    instance.pose.reset();
    instance.deformed_vertices.reset();
    if (!component.mesh || !component.mesh->vertices || !instance.skin)
        throw std::invalid_argument("Skinned mesh requires vertices and skin");
    const auto& source = *component.mesh->vertices;
    const auto inverse_bind = instance.skin->GetInverseBindMatrices();
    if (instance.joints.size() != inverse_bind.size()) throw std::invalid_argument("Skin joint count mismatch");
    if (instance.validated_vertices != component.mesh->vertices || instance.validated_joint_count != inverse_bind.size()) {
        instance.validated_vertices.reset();
        const auto positions = source.Span<VertexAttribute::Position>();
        const auto indices = source.Span<VertexAttribute::BlendIndex>();
        const auto weights = source.Span<VertexAttribute::BlendWeight>();
        if (positions.size() != source.Size() || indices.size() != source.Size() || weights.size() != source.Size())
            throw std::invalid_argument("Skin requires positions, indices and weights");
        instance.joint_bounds.assign(inverse_bind.size(), {});
        instance.unweighted_bounds = {};
        for (std::size_t v = 0; v < source.Size(); ++v) {
            double total = 0;
            for (unsigned k = 0; k < 4; ++k) {
                const float w = weights[v][k];
                if (!std::isfinite(w) || w < 0) throw std::invalid_argument("Invalid skin weight");
                if (w > 0) {
                    if (indices[v][k] >= inverse_bind.size()) throw std::invalid_argument("Skin joint index out of range");
                    instance.joint_bounds[indices[v][k]].Expand(positions[v]);
                }
                total += w;
            }
            if (total == 0) instance.unweighted_bounds.Expand(positions[v]);
        }
        instance.joint_bind_bounds.resize(inverse_bind.size());
        for (std::size_t i = 0; i < inverse_bind.size(); ++i)
            instance.joint_bind_bounds[i] = instance.joint_bounds[i].Valid()
                ? math::transform_aabb(inverse_bind[i], instance.joint_bounds[i]) : math::AABBf{};
        instance.validated_joint_count = inverse_bind.size();
        instance.validated_vertices = component.mesh->vertices;
    }
    auto pose = std::make_shared<SkinPose>();
    if (instance.gpu_palette && !instance.cpu_deformation) {
        ValidateSkinMatrix(transform.world_matrix);
        if (math::determinant(transform.world_matrix) == 0) throw std::invalid_argument("Singular skinned mesh transform");
        pose->skin = instance.skin;
        pose->world_to_mesh = math::inverse(transform.world_matrix);
        pose->joint_worlds = GatherJointWorlds(instance, cache);
        // Conservative CPU bounds: transform precomputed joint-local boxes,
        // then the union back to mesh space. No CPU palette or GPU readback.
        math::AABBf world_bounds;
        for (std::size_t i = 0; i < inverse_bind.size(); ++i) {
            if (!instance.joint_bind_bounds[i].Valid()) continue;
            const auto box = math::transform_aabb((*pose->joint_worlds)[i], instance.joint_bind_bounds[i]);
            world_bounds.Expand(box.min_point);
            world_bounds.Expand(box.max_point);
        }
        pose->bounds = instance.unweighted_bounds;
        if (world_bounds.Valid()) {
            const auto box = math::transform_aabb(pose->world_to_mesh, world_bounds);
            pose->bounds.Expand(box.min_point);
            pose->bounds.Expand(box.max_point);
        }
        instance.pose = std::move(pose);
        return;
    }
    const auto cached = std::ranges::find_if(cache.palettes, [&](const auto& entry) {
        return entry.source->skin == instance.skin &&
               std::memcmp(&entry.mesh_world, &transform.world_matrix, sizeof(math::mat4f)) == 0 &&
               std::ranges::equal(entry.source->joints, instance.joints);
    });
    if (cached != cache.palettes.end()) {
        pose->matrices = cached->pose->matrices;
    } else {
        ZoneScopedNS("Skin.BuildPalette", 0);
        ValidateSkinMatrix(transform.world_matrix);
        if (math::determinant(transform.world_matrix) == 0) throw std::invalid_argument("Singular skinned mesh transform");
        const auto world_to_mesh = math::inverse(transform.world_matrix);
        const auto worlds = GatherJointWorlds(instance, cache);
        // Preserve multiplication association: (mesh^-1 * joint) * inverse_bind.
        auto local = world_to_mesh * std::span<const math::mat4f>(*worlds);
        pose->matrices = math::mat4f::mul_batch(local, inverse_bind);
        cache.palettes.push_back({&instance, transform.world_matrix, pose});
    }
    // Bounds are per mesh, even when its palette is shared with another mesh.
    pose->bounds = instance.unweighted_bounds;
    for (std::size_t i = 0; i < inverse_bind.size(); ++i) {
        if (instance.joint_bounds[i].Valid()) {
            const auto bounds = math::transform_aabb(pose->matrices[i], instance.joint_bounds[i]);
            pose->bounds.Expand(bounds.min_point);
            pose->bounds.Expand(bounds.max_point);
        }
    }
    instance.pose = std::move(pose);
}

void SkinMesh(const MeshComponent& component, const Transform& transform, SkinnedMesh& instance) {
    instance.deformed_vertices.reset();
    if (!component.mesh || !component.mesh->vertices || !instance.skin)
        throw std::invalid_argument("Skinned mesh requires mesh vertices and a skin");
    const auto& source    = *component.mesh->vertices;
    const auto  positions = source.Span<VertexAttribute::Position>();
    const auto  indices   = source.Span<VertexAttribute::BlendIndex>();
    const auto  weights   = source.Span<VertexAttribute::BlendWeight>();
    if (positions.size() != source.Size() || indices.size() != source.Size() || weights.size() != source.Size())
        throw std::invalid_argument("Skinned mesh requires positions, joint indices and weights");
    const auto inverse_bind = instance.skin->GetInverseBindMatrices();
    if (instance.joints.size() != inverse_bind.size()) throw std::invalid_argument("Skin joint count mismatch");
    ValidateSkinMatrix(transform.world_matrix);
    if (math::determinant(transform.world_matrix) == 0.0f) throw std::invalid_argument("Singular skinned mesh transform");
    const auto               world_to_mesh = math::inverse(transform.world_matrix);
    std::vector<math::mat4f> palette;
    palette.reserve(instance.joints.size());
    for (std::size_t i = 0; i < instance.joints.size(); ++i) {
        const auto joint = instance.joints[i];
        if (!joint || !joint.Has<Transform>()) throw std::invalid_argument("Skin references a missing joint Transform");
        const auto& joint_world = joint.Get<Transform>().world_matrix;
        ValidateSkinMatrix(joint_world);
        palette.emplace_back(world_to_mesh * joint_world * inverse_bind[i]);
    }

    VertexArray::Data        output;
    std::vector<math::mat4f> blended(source.Size(), math::mat4f::identity());
    for (std::size_t vertex = 0; vertex < source.Size(); ++vertex) {
        double total = 0.0;
        for (unsigned influence = 0; influence < 4; ++influence) {
            const float weight = weights[vertex][influence];
            if (!std::isfinite(weight) || weight < 0.0f) throw std::invalid_argument("Invalid skin weight");
            if (weight > 0.0f && indices[vertex][influence] >= palette.size()) throw std::invalid_argument("Skin joint index out of range");
            total += weight;
        }
        // Unweighted vertices retain their mesh-local position/directions.
        if (total == 0.0) continue;
        auto matrix = math::mat4f::zero();
        for (unsigned influence = 0; influence < 4; ++influence) {
            if (weights[vertex][influence] == 0.0f) continue;
            matrix += palette[indices[vertex][influence]] * static_cast<float>(weights[vertex][influence] / total);
        }
        blended[vertex] = matrix;
    }
    output[VertexAttribute::Position] = core::Buffer(source.Size() * sizeof(math::vec3f));
    {
        auto destination = output[VertexAttribute::Position].Span<math::vec3f>();
        for (std::size_t i = 0; i < positions.size(); ++i) {
            const auto& p  = positions[i];
            destination[i] = (blended[i] * math::vec4f(p.x, p.y, p.z, 1.0f)).xyz;
        }
    }
    const auto deform_direction = [&]<VertexAttribute Attribute>() {
        const auto values = source.Span<Attribute>();
        if (values.empty()) return;
        output[Attribute] = core::Buffer(source.Size() * sizeof(math::vec3f));
        {
            auto destination = output[Attribute].Span<math::vec3f>();
            for (std::size_t i = 0; i < values.size(); ++i) {
                const auto& matrix = blended[i];
                const auto& value  = values[i];
                math::vec3f result;
                if constexpr (Attribute == VertexAttribute::Normal) {
                    // For affine skin matrices, normals need only the inverse
                    // transpose of the linear 3x3 part. Its cofactor columns
                    // avoid a full 4x4 inverse per vertex. Preserve determinant
                    // sign for reflections; normalization removes its magnitude.
                    const math::vec3f x(matrix[0][0], matrix[1][0], matrix[2][0]);
                    const math::vec3f y(matrix[0][1], matrix[1][1], matrix[2][1]);
                    const math::vec3f z(matrix[0][2], matrix[1][2], matrix[2][2]);
                    const auto        cx          = math::cross(y, z);
                    const float       determinant = math::dot(x, cx);
                    result                        = std::abs(determinant) > 1e-8f
                                                        ? (cx * value.x + math::cross(z, x) * value.y + math::cross(x, y) * value.z) / determinant
                                                        : value;
                } else {
                    result = (matrix * math::vec4f(value.x, value.y, value.z, 0.0f)).xyz;
                }
                const float length = result.norm();
                destination[i]     = length > 1e-8f ? result / length : value;
            }
        }
    };
    deform_direction.template operator()<VertexAttribute::Normal>();
    deform_direction.template operator()<VertexAttribute::Tangent>();
    deform_direction.template operator()<VertexAttribute::Bitangent>();
    instance.deformed_vertices = std::make_shared<VertexArray>(source.Size(), std::move(output), "SkinnedVertices");
}

void SkinningSystem::ConfigureSchedule(ecs::Schedule& schedule) {
    schedule.Request("skin_meshes", [](ecs::Query<const MeshComponent&, const Transform&, SkinnedMesh&> query) {
        SkinPaletteCache cache;
        query.Each([&](const MeshComponent& mesh, const Transform& transform, SkinnedMesh& skin) {
            UpdateSkinPose(mesh, transform, skin, cache);
            if (skin.cpu_deformation) SkinMesh(mesh, transform, skin);
        });
    });
    // Joint Transform reads happen through entity references, beyond the query.
    schedule.SetOrder("update_world_matrix_flat", "skin_meshes");
}

}  // namespace hitagi::asset
