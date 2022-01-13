#pragma once
#include "IRuntimeModule.hpp"
#include "Animation.hpp"

#include <unordered_set>

namespace Hitagi::Asset {
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
}  // namespace Hitagi::Asset
