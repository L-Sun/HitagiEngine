#include <hitagi/resource/animation_manager.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace hitagi::asset {
int AnimationManager::Initialize() {
    m_Logger = spdlog::stdout_color_mt("AnimationManager");
    m_Logger->info("Initializing...");

    return 0;
}
void AnimationManager::Tick() {
    std::vector<decltype(m_PlayQueue)::key_type> end_animations;
    for (auto& animation : m_PlayQueue) {
        if (animation->Animate()) {
            end_animations.emplace_back(animation);
        }
    }
    for (auto&& animation : end_animations) {
        m_PlayQueue.erase(animation);
    }
}

void AnimationManager::Finalize() {
    m_PlayQueue.clear();

    m_Logger->info("Finalized!");
    m_Logger = nullptr;
}

void AnimationManager::AddToPlayQueue(std::shared_ptr<Animation> animation) {
    m_PlayQueue.emplace(animation);
}

}  // namespace hitagi::asset