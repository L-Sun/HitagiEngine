#pragma once
#include "animation.hpp"

#include <hitagi/core/runtime_module.hpp>

#include <unordered_set>

namespace hitagi::asset {
class AnimationManager : public IRuntimeModule {
public:
    AnimationManager()                        = default;
    AnimationManager(const AnimationManager&) = delete;
    AnimationManager& operator=(const AnimationManager&) = delete;

    int  Initialize() final;
    void Tick() final;
    void Finalize() final;

    void AddToPlayQueue(std::shared_ptr<Animation> animation);

private:
    std::unordered_set<std::shared_ptr<Animation>> m_PlayQueue;
};
}  // namespace hitagi::asset
