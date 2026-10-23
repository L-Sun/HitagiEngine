export module asset:skin;
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

// Compose with MeshComponent + Transform + RelationShip. Multiple instances
// can share Mesh/Skin while referencing independent or shared joint entities.
struct SkinnedMesh {
    std::shared_ptr<const Skin> skin;
    std::vector<ecs::Entity>    joints;

    // Output snapshots, replaced on update. Never modify a published snapshot:
    // submitted render frames may still own it. Only deformed attributes exist
    // here; UVs, colors, indices and materials remain on the shared Mesh.
    std::shared_ptr<VertexArray> deformed_vertices;
};

struct SkinningSystem {
    static void OnUpdate(ecs::Schedule& schedule);
};

}  // namespace hitagi::asset

namespace hitagi::asset {

void ValidateSkinMatrix(const math::mat4f& matrix) {
    for (const auto& row : matrix.data)
        for (float value : row.data)
            if (!std::isfinite(value)) throw std::invalid_argument("Non-finite skin transform");
    if (matrix[3] != math::vec4f(0.0f, 0.0f, 0.0f, 1.0f))
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

void SkinningSystem::OnUpdate(ecs::Schedule& schedule) {
    schedule.Request("skin_meshes", [](const MeshComponent& mesh, const Transform& transform, SkinnedMesh& skin) {
        SkinMesh(mesh, transform, skin);
    });
    // Joint Transform reads happen through entity references, beyond the query.
    schedule.SetOrder("update_world_matrix_flat", "skin_meshes");
}

}  // namespace hitagi::asset
