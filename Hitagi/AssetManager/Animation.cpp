#include "Animation.hpp"

namespace Hitagi::Asset {
bool Animation::Animate() {
    if (m_Clock.IsPaused()) return false;

    auto deltatime = m_Clock.DeltaTime().count();
    if (deltatime < m_FrameTime)
        return false;

    m_Clock.Tick();

    if (m_Clock.TotalTime().count() > m_Frames.size() * m_FrameTime) {
        if (m_Loop) {
            m_Clock.Reset();
        } else
            return true;
    }
    size_t frame = FPS() * m_Clock.TotalTime().count();

    for (auto&& [node, trs] : m_Frames.at(std::min(frame, m_Frames.size() - 1))) {
        vec3f velocity = (trs.translation - node->GetPosition()) / deltatime;
        node->SetTRS(trs.translation, trs.rotation, trs.scaling);
        node->SetVelocity(velocity);
    }
    return false;
}

void Animation::Play() {
    if (m_Clock.TotalTime().count() >= m_Frames.size() * m_FrameTime) {
        m_Clock.Reset();
    }
    m_Clock.Start();
}

AnimationBuilder& AnimationBuilder::SetSkeleton(std::shared_ptr<BoneNode> skeleton) {
    m_Result->m_Skeleton = skeleton;
    return *this;
}

AnimationBuilder& AnimationBuilder::NewFrame() {
    m_Result->m_Frames.resize(m_Result->m_Frames.size() + 1);
    return *this;
}

AnimationBuilder& AnimationBuilder::AppenTRSToChannel(std::shared_ptr<SceneNode> channle, Animation::TRS trs) {
    m_Result->m_Frames.back()[channle] = std::move(trs);
    return *this;
}

AnimationBuilder& AnimationBuilder::SetFrameRate(unsigned fps) {
    m_Result->m_FrameTime = 1.0 / static_cast<double>(fps);
    return *this;
}

std::shared_ptr<Animation> AnimationBuilder::Finish() {
    auto result = m_Result;
    m_Result    = nullptr;
    return result;
}

}  // namespace Hitagi::Asset