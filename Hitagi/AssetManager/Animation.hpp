#pragma once
#include "SceneObject.hpp"
#include "Timer.hpp"
#include "SceneNode.hpp"

namespace Hitagi::Asset {
class Animation : public SceneObject {
    friend class AnimationBuilder;

public:
    struct TRS {
        vec3f translation;
        quatf rotation;
        vec3f scaling;
    };

    // Return:
    //     true: the animation is end
    //     flase: the ainmation is not end.
    bool Animate();

    void        Play();
    inline void Pause() { m_Clock.Pause(); }

    inline bool IsLoop() const noexcept { return m_Loop; }
    inline void SetLoop(bool loop) noexcept { m_Loop = loop; }

private:
    Core::Clock                                                      m_Clock;
    bool                                                             m_Loop = false;
    std::chrono::duration<double>                                    m_Duration;
    unsigned                                                         m_FPS;
    std::unordered_map<std::shared_ptr<SceneNode>, std::vector<TRS>> m_Channels;
};

class AnimationBuilder {
public:
    AnimationBuilder() : m_Result(std::make_shared<Animation>()) {}
    AnimationBuilder&          AppenTRSToChannel(std::shared_ptr<SceneNode> channle, Animation::TRS trs);
    AnimationBuilder&          SetDuration(std::chrono::duration<double> duration);
    AnimationBuilder&          SetFrameRate(unsigned fps);
    std::shared_ptr<Animation> Finish();

private:
    std::shared_ptr<Animation> m_Result;
};

}  // namespace Hitagi::Asset