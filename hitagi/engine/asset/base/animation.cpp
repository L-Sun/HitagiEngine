export module asset.base:animation;
import std;
import math;
import ecs;
import :resource;
import :transform;

export namespace hitagi::asset {

enum class AnimationInterpolation { Step,
                                    Linear };

template <typename T>
struct AnimationKey {
    double time = 0.0;
    T      value;
};

template <typename T>
struct AnimationCurve {
    AnimationInterpolation       interpolation = AnimationInterpolation::Linear;
    std::vector<AnimationKey<T>> keys;
};

// Target IDs are clip-local logical slots, resolved by AnimationTarget on entities.
// They are unrelated to a mesh's joint palette indices.
struct AnimationTrack {
    std::uint32_t               target = 0;
    AnimationCurve<math::vec3f> positions;
    AnimationCurve<math::quatf> rotations;
    AnimationCurve<math::vec3f> scales;
};

class AnimationClip final : public Resource {
public:
    AnimationClip(double duration, std::vector<AnimationTrack> tracks, std::string_view name = "");
    auto GetDuration() const noexcept -> double { return m_Duration; }
    auto GetTracks() const noexcept -> std::span<const AnimationTrack> { return m_Tracks; }
    auto FindTrack(std::uint32_t target) const noexcept -> const AnimationTrack*;
    void Load(const ResourceLoadContext&) final { SetLoadState(ResourceLoadState::Loaded); }
    void Unload() final { SetLoadState(ResourceLoadState::Unloaded); }

private:
    double                      m_Duration;
    std::vector<AnimationTrack> m_Tracks;
};

struct AnimationPlayer {
    std::shared_ptr<const AnimationClip> clip;
    double                               time    = 0.0;
    double                               speed   = 1.0;
    bool                                 looping = true;
    bool                                 playing = true;
};

// One player writes each target. Pausing stops time, but still evaluates the pose.
// Removing this component releases the target for procedural animation.
struct AnimationTarget {
    ecs::Entity   player;
    std::uint32_t target = 0;
};

// Local pose data has no entity, matrix cache, or playback ownership.
struct LocalPose {
    math::vec3f position = math::vec3f(0.0f);
    math::quatf rotation = math::quatf::identity();
    math::vec3f scaling  = math::vec3f(1.0f);
    auto        ToMatrix() const -> math::mat4f {
        return math::translate(position) * math::rotate(rotation) * math::scale(scaling);
    }
};

// Missing channels preserve the supplied input. Sampling never mutates a World.
auto SampleAnimationTrack(const AnimationTrack& track, double time, LocalPose input) -> LocalPose;
auto BlendLocalPose(const LocalPose& from, const LocalPose& to, float alpha) -> LocalPose;
auto ReadLocalPose(const Transform& transform) -> LocalPose;
void WriteLocalPose(const LocalPose& pose, Transform& transform);
auto ResolveAnimationTime(double time, double duration, bool looping) -> double;

struct AnimationSystem {
    static void ConfigureSchedule(ecs::Schedule& schedule);
};

}  // namespace hitagi::asset

namespace hitagi::asset {

template <typename T>
void ValidateAnimationCurve(AnimationCurve<T>& curve, double duration) {
    if (curve.interpolation != AnimationInterpolation::Step && curve.interpolation != AnimationInterpolation::Linear)
        throw std::invalid_argument("Unsupported animation interpolation");
    double previous = -1.0;
    for (auto& key : curve.keys) {
        if (!std::isfinite(key.time) || key.time < 0.0 || key.time > duration || key.time <= previous)
            throw std::invalid_argument("Animation key times must be finite, increasing and within the clip");
        previous = key.time;
        for (float value : key.value.data)
            if (!std::isfinite(value)) throw std::invalid_argument("Non-finite animation key");
        if constexpr (std::same_as<T, math::quatf>) {
            const float length = key.value.norm();
            if (!std::isfinite(length) || length < 1e-8f) throw std::invalid_argument("Invalid animation quaternion");
            for (auto& value : key.value.data) value /= length;
        }
    }
}

AnimationClip::AnimationClip(double duration, std::vector<AnimationTrack> tracks, std::string_view name)
    : Resource(Type::AnimationClip, name), m_Duration(duration), m_Tracks(std::move(tracks)) {
    if (!std::isfinite(duration) || duration < 0.0) throw std::invalid_argument("Invalid animation duration");
    std::ranges::sort(m_Tracks, {}, &AnimationTrack::target);
    for (std::size_t i = 0; i < m_Tracks.size(); ++i) {
        auto& track = m_Tracks[i];
        if (i && m_Tracks[i - 1].target == track.target) throw std::invalid_argument("Duplicate animation target");
        ValidateAnimationCurve(track.positions, duration);
        ValidateAnimationCurve(track.rotations, duration);
        ValidateAnimationCurve(track.scales, duration);
    }
}

auto AnimationClip::FindTrack(std::uint32_t target) const noexcept -> const AnimationTrack* {
    const auto it = std::ranges::lower_bound(m_Tracks, target, {}, &AnimationTrack::target);
    return it != m_Tracks.end() && it->target == target ? std::addressof(*it) : nullptr;
}

auto InterpolateRotation(const math::quatf& from, const math::quatf& to, float alpha) -> math::quatf {
    // Shortest-arc slerp; near-identical rotations use normalized lerp.
    auto  end    = to;
    float cosine = math::dot(from, end);
    if (cosine < 0.0f) {
        for (auto& value : end.data) value = -value;
        cosine = -cosine;
    }
    cosine  = std::clamp(cosine, 0.0f, 1.0f);
    float a = 1.0f - alpha;
    float b = alpha;
    if (cosine < 0.9995f) {
        const float angle = std::acos(cosine);
        a                 = std::sin((1.0f - alpha) * angle) / std::sin(angle);
        b                 = std::sin(alpha * angle) / std::sin(angle);
    }
    math::quatf result;
    for (unsigned i = 0; i < 4; ++i) result[i] = from[i] * a + end[i] * b;
    const float length = result.norm();
    for (auto& value : result.data) value /= length;
    return result;
}

template <typename T>
auto SampleAnimationCurve(const AnimationCurve<T>& curve, double time) -> T {
    if (time <= curve.keys.front().time) return curve.keys.front().value;
    if (time >= curve.keys.back().time) return curve.keys.back().value;
    const auto  right = std::ranges::upper_bound(curve.keys, time, {}, &AnimationKey<T>::time);
    const auto& left  = *std::prev(right);
    if (curve.interpolation == AnimationInterpolation::Step) return left.value;
    const float alpha = static_cast<float>((time - left.time) / (right->time - left.time));
    if constexpr (std::same_as<T, math::quatf>) {
        return InterpolateRotation(left.value, right->value, alpha);
    } else {
        return left.value * (1.0f - alpha) + right->value * alpha;
    }
}

auto ReadLocalPose(const Transform& transform) -> LocalPose {
    return {.position = transform.position, .rotation = transform.rotation, .scaling = transform.scaling};
}

void WriteLocalPose(const LocalPose& pose, Transform& transform) {
    transform.position = pose.position;
    transform.rotation = pose.rotation;
    transform.scaling  = pose.scaling;
}

auto SampleAnimationTrack(const AnimationTrack& track, double time, LocalPose input) -> LocalPose {
    if (!std::isfinite(time)) throw std::invalid_argument("Non-finite animation sample time");
    if (!track.positions.keys.empty()) input.position = SampleAnimationCurve(track.positions, time);
    if (!track.rotations.keys.empty()) input.rotation = SampleAnimationCurve(track.rotations, time);
    if (!track.scales.keys.empty()) input.scaling = SampleAnimationCurve(track.scales, time);
    return input;
}

auto BlendLocalPose(const LocalPose& from, const LocalPose& to, float alpha) -> LocalPose {
    if (!std::isfinite(alpha) || alpha < 0.0f || alpha > 1.0f)
        throw std::invalid_argument("Pose blend alpha must be in [0, 1]");
    return {.position = from.position * (1.0f - alpha) + to.position * alpha,
            .rotation = InterpolateRotation(from.rotation, to.rotation, alpha),
            .scaling  = from.scaling * (1.0f - alpha) + to.scaling * alpha};
}

auto ResolveAnimationTime(double time, double duration, bool looping) -> double {
    if (!std::isfinite(time) || !std::isfinite(duration) || duration < 0.0)
        throw std::invalid_argument("Invalid animation time or duration");
    if (duration == 0.0) return 0.0;
    if (!looping) return std::clamp(time, 0.0, duration);
    const double wrapped = std::fmod(time, duration);
    return wrapped < 0.0 ? wrapped + duration : wrapped;
}

void AnimationSystem::ConfigureSchedule(ecs::Schedule& schedule) {
    schedule.Request("advance_animations", [&schedule](ecs::Query<AnimationPlayer&> query) {
        query.Each([&](AnimationPlayer& player) {
            if (!player.clip) return;
            if (!std::isfinite(player.time) || !std::isfinite(player.speed)) throw std::invalid_argument("Invalid animation playback state");
            double time = player.time + (player.playing ? schedule.GetDeltaSeconds() * player.speed : 0.0);
            if (!std::isfinite(time)) throw std::invalid_argument("Animation time overflow");
            player.time = ResolveAnimationTime(time, player.clip->GetDuration(), player.looping);
        });
    });
    schedule.Request("animate_targets", [](ecs::Query<ecs::Entity, const AnimationTarget&> query) {
        query.Each([&](ecs::Entity entity, const AnimationTarget& target) {
            if (!entity.Has<Transform>() || !target.player || !target.player.Has<AnimationPlayer>()) return;
            const auto& player = target.player.Get<AnimationPlayer>();
            if (!player.clip) return;
            const auto* track = player.clip->FindTrack(target.target);
            if (!track) return;
            auto& transform = entity.Get<Transform>();
            WriteLocalPose(SampleAnimationTrack(*track, player.time, ReadLocalPose(transform)), transform);
        });
    });
    // Cross-entity reads/writes are not inferred by Schedule. Declare the phase
    // boundary explicitly rather than depending on system registration order.
    schedule.SetOrder("advance_animations", "animate_targets");
    schedule.SetOrder("animate_targets", "update_local_matrix");
}

}  // namespace hitagi::asset
