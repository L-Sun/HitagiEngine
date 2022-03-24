#pragma once
#include "SceneObject.hpp"
#include "Timer.hpp"
#include "SceneNode.hpp"

namespace hitagi::asset {
class Animation : public SceneObject {
    friend class AnimationBuilder;

public:
    struct TRS {
        math::vec3f translation;
        math::quatf rotation;
        math::vec3f scaling;
    };

    // Return:
    //     true: the animation is end
    //     flase: the ainmation is not end.
    bool Animate();

    void        Play();
    inline void Pause() { m_Clock.Pause(); }

    inline bool IsLoop() const noexcept { return m_Loop; }
    inline void SetLoop(bool loop) noexcept { m_Loop = loop; }

    inline unsigned    FPS() const noexcept { return static_cast<unsigned>(1.0f / m_FrameTime); }
    inline double      FrameTime() const noexcept { return m_FrameTime; }
    inline const auto& Data() const noexcept { return m_Frames; }
    inline auto        GetSkeleton() noexcept { return m_Skeleton; }

private:
    core::Clock                                                      m_Clock;
    bool                                                             m_Loop = false;
    std::vector<std::unordered_map<std::shared_ptr<SceneNode>, TRS>> m_Frames;
    double                                                           m_FrameTime;
    std::shared_ptr<BoneNode>                                        m_Skeleton;
};

class AnimationBuilder {
public:
    AnimationBuilder() : m_Result(std::make_shared<Animation>()) {}
    AnimationBuilder&          SetSkeleton(std::shared_ptr<BoneNode> skeleton);
    AnimationBuilder&          NewFrame();
    AnimationBuilder&          AppenTRSToChannel(std::shared_ptr<SceneNode> channle, Animation::TRS trs);
    AnimationBuilder&          SetFrameRate(unsigned fps);
    std::shared_ptr<Animation> Finish();

private:
    std::shared_ptr<Animation> m_Result;
};

}  // namespace hitagi::asset