#include "Animation.hpp"

namespace Hitagi::Asset {
bool Animation::Animate() {
    m_Clock.Tick();

    if (m_Clock.TotalTime() > m_Duration) {
        if (m_Loop) {
            m_Clock.Reset();
        } else
            return true;
    }

    size_t frame = m_FPS * m_Clock.TotalTime().count();

    for (auto&& [node, animation] : m_Channels) {
        TRS trs = animation.at(std::min(frame, animation.size() - 1));
        node->SetTRS(trs.translation, trs.rotation, trs.scaling);
    }
    return false;
}

void Animation::Play() {
    if (m_Clock.TotalTime() >= m_Duration) {
        m_Clock.Reset();
    }
    m_Clock.Start();
}

AnimationBuilder& AnimationBuilder::AppenTRSToChannel(std::shared_ptr<SceneNode> channle, Animation::TRS trs) {
    m_Result->m_Channels[channle].emplace_back(std::move(trs));
    return *this;
}

AnimationBuilder& AnimationBuilder::SetDuration(std::chrono::duration<double> duration) {
    m_Result->m_Duration = duration;
    return *this;
}

AnimationBuilder& AnimationBuilder::SetFrameRate(unsigned fps) {
    m_Result->m_FPS = fps;
    return *this;
}

std::shared_ptr<Animation> AnimationBuilder::Finish() {
    auto result = m_Result;
    m_Result    = nullptr;
    return result;
}

}  // namespace Hitagi::Asset