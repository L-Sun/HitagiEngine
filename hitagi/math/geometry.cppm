export module math:geometry;
import std;
import :vector;
import :matrix;

export namespace hitagi::math {

template <typename T>
struct AABB {
    Vector<T, 3> min_point{std::numeric_limits<T>::max()};
    Vector<T, 3> max_point{std::numeric_limits<T>::lowest()};

    constexpr bool Valid() const noexcept {
        return min_point.x <= max_point.x &&
               min_point.y <= max_point.y &&
               min_point.z <= max_point.z;
    }

    constexpr void Expand(const Vector<T, 3>& point) noexcept {
        for (unsigned i = 0; i < 3; ++i) {
            if (point[i] < min_point[i]) min_point[i] = point[i];
            if (point[i] > max_point[i]) max_point[i] = point[i];
        }
    }

    constexpr auto Center() const noexcept -> Vector<T, 3> {
        return (min_point + max_point) * static_cast<T>(0.5);
    }

    constexpr auto Extents() const noexcept -> Vector<T, 3> {
        return (max_point - min_point) * static_cast<T>(0.5);
    }
};

using AABBf = AABB<float>;

struct Frustum {
    std::array<vec4f, 6> planes;

    enum PlaneIndex : unsigned { Left = 0, Right, Bottom, Top, Near, Far };
};

// Gribb-Hartmann frustum plane extraction from a row-major view-projection matrix
// where clip = M * v (column vector convention).
// Each plane (a,b,c,d) satisfies ax + by + cz + d >= 0 for points inside.
inline auto extract_frustum(const mat4f& vp) noexcept -> Frustum {
    Frustum f;
    auto row = [&](unsigned i) -> vec4f { return vp[i]; };

    vec4f r0 = row(0), r1 = row(1), r2 = row(2), r3 = row(3);

    auto normalize_plane = [](vec4f p) -> vec4f {
        float len = vec3f(p.xyz).norm();
        return (len > 1e-8f) ? p / len : p;
    };

    f.planes[Frustum::Left]   = normalize_plane({r3.x + r0.x, r3.y + r0.y, r3.z + r0.z, r3.w + r0.w});
    f.planes[Frustum::Right]  = normalize_plane({r3.x - r0.x, r3.y - r0.y, r3.z - r0.z, r3.w - r0.w});
    f.planes[Frustum::Bottom] = normalize_plane({r3.x + r1.x, r3.y + r1.y, r3.z + r1.z, r3.w + r1.w});
    f.planes[Frustum::Top]    = normalize_plane({r3.x - r1.x, r3.y - r1.y, r3.z - r1.z, r3.w - r1.w});
    f.planes[Frustum::Near]   = normalize_plane({r3.x + r2.x, r3.y + r2.y, r3.z + r2.z, r3.w + r2.w});
    f.planes[Frustum::Far]    = normalize_plane({r3.x - r2.x, r3.y - r2.y, r3.z - r2.z, r3.w - r2.w});
    return f;
}

// Test an AABB against 6 frustum planes.
// Returns true if the AABB is at least partially inside the frustum.
inline bool is_aabb_visible(const Frustum& frustum, const AABBf& aabb) noexcept {
    for (const auto& plane : frustum.planes) {
        vec3f normal{plane.x, plane.y, plane.z};
        // p-vertex: the AABB corner most aligned with the plane normal
        vec3f p{
            normal.x >= 0 ? aabb.max_point.x : aabb.min_point.x,
            normal.y >= 0 ? aabb.max_point.y : aabb.min_point.y,
            normal.z >= 0 ? aabb.max_point.z : aabb.min_point.z,
        };
        if (dot(normal, p) + plane.w < 0) return false;
    }
    return true;
}

// Transform an AABB by a 4x4 matrix, producing a new world-space AABB.
// Uses the Arvo method for tight transformed bounds.
inline auto transform_aabb(const mat4f& m, const AABBf& aabb) noexcept -> AABBf {
    vec3f translation{m[0][3], m[1][3], m[2][3]};
    AABBf result{.min_point = translation, .max_point = translation};

    for (unsigned i = 0; i < 3; ++i) {
        for (unsigned j = 0; j < 3; ++j) {
            float a = m[i][j] * aabb.min_point[j];
            float b = m[i][j] * aabb.max_point[j];
            result.min_point[i] += std::min(a, b);
            result.max_point[i] += std::max(a, b);
        }
    }
    return result;
}

}  // namespace hitagi::math
